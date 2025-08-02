#pragma once

#include <stdint.h>

void ir_init();
extern uint8_t rx_buf[10], rx_buf_len;
extern uint8_t ir_comm_active;