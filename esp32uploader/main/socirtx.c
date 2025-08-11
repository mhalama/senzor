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
#include "socirtx.h"

#include <driver/gpio.h>

#include "driver/rmt_tx.h"

#define TAG "SOCIRTX"

#define BUF_SIZE         4096
#define RMT_CLK_HZ       1000000
#define CARRIER_FREQ_HZ  38000
#define CARRIER_DUTY     33

static int _pin;
static int _speed;

static uint8_t tx_buffer[BUF_SIZE] = {0};

static void send_buffer_with_rmt(uint8_t *buf, size_t len) {
    esp_log_level_set("*", ESP_LOG_DEBUG);

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = _pin,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000,
        .trans_queue_depth = 10,
        .intr_priority = 3
    };

    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33f,
        .frequency_hz = 38000,
    };

    rmt_channel_handle_t tx_channel;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &tx_channel));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = 1000000 / _speed / 2,
            .level0 = 1,
            .duration1 = 1000000 / _speed / 2,
            .level1 = 1,
        },
        .bit1 = {
            .duration0 = 1000000 / _speed / 2,
            .level0 = 0,
            .duration1 = 1000000 / _speed / 2,
            .level1 = 0,
        },
        .flags = {
            .msb_first = 1
        }
    };

    rmt_encoder_handle_t encoder = NULL;
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_encoder_config, &encoder));

    ESP_LOGI(TAG, "Sending %d bytes via IR UART...", (int)len);

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
        .flags = {
            .queue_nonblocking = false
        }
    };

    ESP_ERROR_CHECK(rmt_transmit(tx_channel, encoder, buf, len, &transmit_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, portMAX_DELAY));

    ESP_LOGI(TAG, "Transmission complete.");

    ESP_ERROR_CHECK(rmt_disable(tx_channel));
    ESP_ERROR_CHECK(rmt_del_channel(tx_channel));
}

static uint8_t nimbble_symbols[] =
{
    0xd, 0xe, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
    0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34
};

static uint8_t nibblify_stream(uint8_t *src, size_t srclen, uint8_t *dest, size_t destlen) {
    size_t dest_ix = 0;

    for (int i = 0; i < srclen; i += 2) {
        uint8_t a = nimbble_symbols[src[i] >> 4];
        uint8_t b = nimbble_symbols[src[i] & 0x0f];
        uint8_t c = 0;
        uint8_t d = 0;
        if (i + 1 < srclen) {
            c = nimbble_symbols[src[i + 1] >> 4];
            d = nimbble_symbols[src[i + 1] & 0x0f];
        }

        uint8_t x = a << 2 | b >> 4;
        uint8_t y = b << 4 | c >> 2;
        uint8_t z = c << 6 | d;

        if (destlen - dest_ix < 3) return 0;

        dest[dest_ix++] = x;
        dest[dest_ix++] = y;
        dest[dest_ix++] = z;
    }
    return dest_ix;
}

static int do_irtx_update(int sock) {
    int read_bytes = 0;
    size_t tx_buf_len = 0;
    uint8_t synchro_len = 12;
    uint8_t buf[512];

    // Pripravime preambuli - synchronizace 01 01 01 01 01 ... musi jich byt (12*x - 4) / 8 bytu
    memset(tx_buffer, 0xCC, synchro_len); // 16 dvojic 01 - 32 bitu
    tx_buf_len = synchro_len;
    // tx_buffer[tx_buf_len++] = 0x38; // 0x38;
    // tx_buffer[tx_buf_len++] = 0xAB;

    // nacteme START symbol
    tx_buf_len += recv(sock, tx_buffer+tx_buf_len, 4, 0);

    size_t offset = 0;
    // pozor u zprav predpokladame, ze jsou vzdy ve wordech
    while ((read_bytes = recv(sock, buf + offset, 512 - offset, 0)) > 0) {
        offset += read_bytes;
        if (offset % 1) continue;
        size_t nibbles_count = nibblify_stream(buf, offset, tx_buffer + tx_buf_len, BUF_SIZE - tx_buf_len);
        offset = 0;
        if (!nibbles_count) {
            ESP_LOGE(TAG, "Failed to encode nibbles, aborting");
            return ESP_ERR_INVALID_ARG;
        }
        tx_buf_len += nibbles_count;
    }

    if (offset) {
        ESP_LOGE(TAG, "Failed to encode last nibbles - message need to be word aligned, aborting");
        return ESP_ERR_INVALID_ARG;
    }

    if (read_bytes < 0) {
        ESP_LOGE(TAG, "Error reading data from socket: errno %d", errno);
        return errno;
    }

    ESP_LOGI(TAG, "Sending %d bytes...", tx_buf_len);

    ESP_LOG_BUFFER_HEX(TAG, tx_buffer, tx_buf_len);


    send_buffer_with_rmt(tx_buffer, tx_buf_len);

    ESP_LOGI(TAG, "... finished");

    return ESP_OK;
}

void irtx_socket_writer_init(int speed, int pin, int socket_port) {
    ESP_LOGI(TAG, "Spoustim IR TX vysilac...");

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
    params->port = socket_port;
    params->handler = do_irtx_update;
    params->redirect_stdout = false;
    params->redirect_stdin = false;

    xTaskCreate(socket_server, "irtx_socket_server", 12000, params, 5, NULL);
}
