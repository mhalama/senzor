#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "util.h"

void log_hex(uint8_t *data, size_t len) {
    char buf[300] = {0}; // Pozor na přetečení – pro 32 bytů potřebuješ 3*32 = 96 znaků
    char *p = buf;

    for (size_t i = 0; i < len && (p - buf) < sizeof(buf) - 3; i++) {
        p += sprintf(p, "%02X ", data[i]);
    }

    ESP_LOGI("LOG_HEX", "Data: %s", buf);
}
