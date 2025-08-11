#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "sockhelper.h"

// https://github.com/NyankoLab/esp32c3-elf
// TODO predelat podle: https://components.espressif.com/components/espressif/elf_loader/versions/1.0.0

#define TAG "SOCKHELPER"

#define EXEC_BUF_LEN 8192

void socket_server(void *pvParameter) {
    socket_server_params *params = pvParameter;

    int server_fd = 0;

    struct sockaddr_storage dest_addr;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *) &dest_addr;

    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(params->port);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == 0) {
        ESP_LOGE(TAG, "setsockopt selhal");
        goto exit;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        ESP_LOGE(TAG, "setsockopt selhal");
        goto exit;
    }

    ESP_LOGI(TAG, "Socket created");

    int err = bind(server_fd, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto exit;
    }

    ESP_LOGI(TAG, "Socket server bound, port %d", params->port);

    err = listen(server_fd, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto exit;;
    }

    ESP_LOGI(TAG, "Server je připraven a poslouchá na portu %d\n", params->port);

    while (1) {
        printf("Čekám na nové připojení...\n");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int new_socket = accept(server_fd, (struct sockaddr *) &source_addr, &addr_len);
        if (new_socket < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        FILE *socket_in = NULL, *socket_out = NULL,
                *original_stdin = stdin,
                *original_stdout = stdout,
                *original_stderr = stderr;

        if (params->redirect_stdin) {
            socket_in = fdopen(new_socket, "r");
            if (socket_in == NULL) {
                ESP_LOGE(TAG, "fdopen stdin failed");
                break;
            }
            stdin = socket_in;
            stdin = socket_in;
        }

        if (params->redirect_stdout) {
            socket_out = fdopen(new_socket, "w");
            setvbuf(socket_out, NULL, _IONBF, 0);
            if (socket_out == NULL) {
                ESP_LOGE(TAG, "fdopen stdout failed");
                break;
            }
            stdout = socket_out;
            stderr = socket_out;
        }

        params->handler(new_socket);

        fflush(stdout);
        fflush(stderr);

        stdin = original_stdin;
        stdout = original_stdout;
        stderr = original_stderr;

        if (socket_in) fclose(socket_in);
        if (socket_out) fclose(socket_out);

        close(new_socket);
    }

exit:
    if (server_fd) close(server_fd);
    free(params);
    vTaskDelete(NULL);
}
