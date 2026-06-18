#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"

typedef struct {
    const char *ssid;
    const char *password;
    int max_retry;
    wifi_auth_mode_t authmode;
} wifi_manager_sta_config_t;

esp_err_t wifi_manager_sta_start(const wifi_manager_sta_config_t *config);
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info);
bool wifi_manager_is_connected(void);
