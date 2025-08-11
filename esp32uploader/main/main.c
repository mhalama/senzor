#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "wifi.h"
#include "socota.h"
#include "socirtx.h"
#include "socirnec.h"

#define TAG "MAIN"

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    init_wifi_radio();
    connect_wifi();

    init_ota_socket_server();

    // Zapis bitbangem do IRM-3638T - pro bootloader
    irtx_socket_writer_init(2000, 1, 9999);

    // Zapis bitbangem do IRM-3638T - pro komunikaci ala NEC
    irnec_socket_writer_init(2000, 1, 9998);

    ESP_LOGI(TAG, "VERZE 5");
}
