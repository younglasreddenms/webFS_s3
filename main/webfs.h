#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "http_file_server.h"

typedef struct {
    const char *base_path;
    http_file_server_storage_info_fn_t get_storage_info;
    const void *storage_info_ctx;
} webfs_service_config_t;

esp_err_t webfs_service_start(const webfs_service_config_t *config);
esp_err_t webfs_service_stop(void);
bool webfs_service_is_running(void);
