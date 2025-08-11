#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "sockhelper.h"
#include "socirnec.h"

#include <driver/gpio.h>

#include "driver/rmt_tx.h"

// Implementuje IR protokol podobny NEC (pulse distance modulation) pro odeslani dat do senzoru. Pouziva jine kodovani nez bootloader
// Bity 0 a 1 jsou kodovany jako kratky pulz (aktivni vysilani 38 kHz) nasledovany ruzne dlouhou pauzou
// 0 - pauza je pocatecnimu aktivnimu impulzu
// 1 - pauza je 4x pocatecnimu aktivnimu impulzu
// ukonceni komunikace - pulz je trojnasobny
// Vyhoda - slave si muze casove synchronizovat na zaklade delky pocatecniho pulzu

#define TAG "SOCIRNEC"

#define PULSE_LENGTH_US  400
#define BUF_SIZE         4096
#define RMT_CLK_HZ       1000000
#define CARRIER_FREQ_HZ  38000
#define CARRIER_DUTY     33

static int _pin;
static int _speed;

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    rmt_encoder_t *bytes_encoder;
    int state;
} rmt_nec_protocol_encoder_t;

static rmt_symbol_word_t nec_ending_symbol = {
    .level0 = 1, // low active
    .duration0 = PULSE_LENGTH_US * 3,
    .level1 = 0,
    .duration1 = PULSE_LENGTH_US
};

static rmt_bytes_encoder_config_t bytes_encoder_config = {
    .bit0 = {
        .level0 = 1,
        .duration0 = PULSE_LENGTH_US,
        .level1 = 0,
        .duration1 = PULSE_LENGTH_US,
    },
    .bit1 = {
        .level0 = 1,
        .duration0 = PULSE_LENGTH_US,
        .level1 = 0,
        .duration1 = PULSE_LENGTH_US * 4,
    },
};

IRAM_ATTR static size_t rmt_encode_nec_protocol(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                                const void *primary_data, size_t data_size,
                                                rmt_encode_state_t *ret_state) {
    rmt_nec_protocol_encoder_t *nec_encoder = __containerof(encoder, rmt_nec_protocol_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    switch (nec_encoder->state) {
        case 0:
            encoded_symbols += nec_encoder->bytes_encoder->encode(nec_encoder->bytes_encoder, channel, primary_data,
                                                                  data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                nec_encoder->state = 1; // we can only switch to next state when current encoder finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space to put other encoding artifacts
            }
        // fall-through
        case 1:
            encoded_symbols += nec_encoder->copy_encoder->encode(nec_encoder->copy_encoder, channel, &nec_ending_symbol,
                                                                 sizeof(nec_ending_symbol), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                state |= RMT_ENCODING_COMPLETE;
                nec_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space to put other encoding artifacts
            }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_nec_protocol_encoder(rmt_encoder_t *encoder) {
    ESP_LOGI(TAG, "DELETING NEC ENCODER");

    rmt_nec_protocol_encoder_t *nec_encoder = __containerof(encoder, rmt_nec_protocol_encoder_t, base);
    rmt_del_encoder(nec_encoder->copy_encoder);
    rmt_del_encoder(nec_encoder->bytes_encoder);
    free(nec_encoder);

    return ESP_OK;
}

IRAM_ATTR static esp_err_t rmt_nec_protocol_encoder_reset(rmt_encoder_t *encoder) {
    rmt_nec_protocol_encoder_t *nec_encoder = __containerof(encoder, rmt_nec_protocol_encoder_t, base);
    rmt_encoder_reset(nec_encoder->copy_encoder);
    rmt_encoder_reset(nec_encoder->bytes_encoder);
    nec_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_new_nec_protocol_encoder(rmt_encoder_handle_t *ret_encoder) {
    rmt_nec_protocol_encoder_t *nec_encoder = rmt_alloc_encoder_mem(sizeof(rmt_nec_protocol_encoder_t));
    nec_encoder->base.encode = rmt_encode_nec_protocol;
    nec_encoder->base.del = rmt_del_nec_protocol_encoder;
    nec_encoder->base.reset = rmt_nec_protocol_encoder_reset;

    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &nec_encoder->copy_encoder);
    rmt_new_bytes_encoder(&bytes_encoder_config, &nec_encoder->bytes_encoder);

    *ret_encoder = &nec_encoder->base;
    return ESP_OK;
}

static void send_buffer_with_rmt(uint8_t *buf, size_t len) {
    rmt_tx_channel_config_t tx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = _pin,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_CLK_HZ,
        .trans_queue_depth = 4,
        .intr_priority = 3
    };

    rmt_channel_handle_t tx_channel;
    rmt_encoder_handle_t nec_encoder;

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &tx_channel));
    ESP_ERROR_CHECK(rmt_new_nec_protocol_encoder(&nec_encoder));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    ESP_LOGI(TAG, "modulate carrier to TX channel");
    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = (float)CARRIER_DUTY / 100.0f,
        .frequency_hz = CARRIER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

    ESP_LOGI(TAG, "Sending %d bytes via IR UART...", (int)len);
    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
        .flags = {
            .queue_nonblocking = false
        }
    };

    ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, buf, len, &transmit_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, portMAX_DELAY));

    ESP_LOGI(TAG, "Transmission complete.");

    ESP_ERROR_CHECK(rmt_disable(tx_channel));
    ESP_ERROR_CHECK(rmt_del_channel(tx_channel));

    ESP_ERROR_CHECK(rmt_del_nec_protocol_encoder(nec_encoder));
}

static int do_irtx_update(int sock) {
    int read_bytes = 0;
    size_t tx_buf_len = 0;
    uint8_t tx_buf[512];

    while (tx_buf_len < 512 && (read_bytes = recv(sock, tx_buf + tx_buf_len, 512 - tx_buf_len, 0)) > 0) {
        tx_buf_len += read_bytes;
    }

    if (tx_buf_len == sizeof(tx_buf)) {
        ESP_LOGW(TAG, "Buffer plný, možná nekompletní data.");
    }

    ESP_LOGI(TAG, "Sending %d bytes...", tx_buf_len);

    ESP_LOG_BUFFER_HEX(TAG, tx_buf, tx_buf_len);

    send_buffer_with_rmt(tx_buf, tx_buf_len);

    ESP_LOGI(TAG, "... finished");

    return ESP_OK;
}

void irnec_socket_writer_init(int speed, int pin, int socket_port) {
    ESP_LOGI(TAG, "Spoustim IR NEC vysilac...");

    _speed = speed;
    _pin = pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << _pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    socket_server_params *params = malloc(sizeof(socket_server_params));
    if (!params) {
        ESP_LOGE(TAG, "Nelze alokovat pamet pro socket_server_params");
        return;
    }
    params->port = socket_port;
    params->handler = do_irtx_update;
    params->redirect_stdout = false;
    params->redirect_stdin = false;

    xTaskCreate(socket_server, "irnec_socket_server", 12000, params, 5, NULL);
}
