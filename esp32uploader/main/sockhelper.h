#pragma once

typedef int (*socket_server_handler)(int socket);

typedef struct socket_server_params {
    socket_server_handler handler;
    int port;
    bool redirect_stdout;
    bool redirect_stdin;
} socket_server_params;

void socket_server(void *pvParameter);