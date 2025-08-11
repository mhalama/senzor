#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "wifi.h"

#define TAG "WIFI"

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "event base: %i, event_id: %i", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
bool is_wifi_connected() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) return false;
    return esp_netif_is_netif_up(netif);
}

void disconnect_wifi() {
    if (!is_wifi_connected()) return;

    esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_OK) {
        ESP_LOGI("WIFI", "WiFi disconnected");
    } else {
        ESP_LOGE("WIFI", "Failed to disconnect WiFi: %s", esp_err_to_name(err));
    }
}

void init_wifi_radio() {
    esp_log_level_set("init wifi radio", ESP_LOG_DEBUG);
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void connect_wifi() {
    if (is_wifi_connected()) return;

    esp_log_level_set("Connecting to WIFI", ESP_LOG_DEBUG);
    //
    // esp_netif_create_default_wifi_sta();
    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    //
    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_connect(); // Uz byla esp_wifi_start v ESP-NOW

    ESP_LOGI("WiFi", "Connecting to Wi-Fi: %s", wifi_config.sta.ssid);

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdTRUE,
                                           pdMS_TO_TICKS(20000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("WiFi", "Connected!");
    } else {
        ESP_LOGE("WiFi", "Can not connect to WiFi.");
    }
}
