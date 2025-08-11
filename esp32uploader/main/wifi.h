#pragma once

#define WIFI_SSID "......"
#define WIFI_PASS "......"

void init_wifi_radio();
void connect_wifi();
bool is_wifi_connected();
void disconnect_wifi();
