#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "esp_ota_ops.h"
#include "sockhelper.h"
#include "socota.h"

#define TAG "SOCOTA"
#define OTA_BUF_SIZE 8192

static int do_ota_update(int sock) {
    esp_ota_handle_t ota_handle = 0;
    int read_bytes = 0;
    char rx_buffer[OTA_BUF_SIZE];

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition: %s", update_partition->label);

    int err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA begin successful");

    size_t offset = 0;
    while ((read_bytes = recv(sock, rx_buffer, OTA_BUF_SIZE, 0)) > 0) {
        err = esp_ota_write(ota_handle, rx_buffer, read_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            break;
        }
        offset += read_bytes;
        ESP_LOGI(TAG, "Written %d bytes", offset);
    }

    if (read_bytes < 0) {
        ESP_LOGE(TAG, "Error reading data from socket: errno %d", errno);
    }

    err = esp_ota_end(ota_handle);
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(update_partition);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "OTA update successful, restarting...");
            esp_restart();
        }
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
    } else {
        ESP_LOGE(TAG, "OTA end failed");
    }

    return err;
}

void init_ota_socket_server() {
    socket_server_params *params = malloc(sizeof(socket_server_params));
    params->port = SOCKOTA_PORT;
    params->handler = do_ota_update;
    params->redirect_stdout = true;
    params->redirect_stdin = false;

    xTaskCreate(socket_server, "ota_socket_server", 2*OTA_BUF_SIZE, params, 5, NULL);
}
