#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

typedef esp_err_t (*http_file_server_storage_info_fn_t)(const void *ctx, size_t *total, size_t *used);

typedef struct {
    const char *base_path;
    http_file_server_storage_info_fn_t get_storage_info;
    const void *storage_info_ctx;
} http_file_server_config_t;

esp_err_t http_file_server_start(const http_file_server_config_t *config);
esp_err_t http_file_server_stop(void);
bool http_file_server_is_running(void);
