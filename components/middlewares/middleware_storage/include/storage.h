#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

#include "esp_err.h"

typedef struct {
    const char *base_path;
    const char *partition_label;
    bool format_if_mount_failed;
} storage_config_t;

esp_err_t storage_mount(const storage_config_t *config);
esp_err_t storage_get_info(const char *partition_label, size_t *total, size_t *used);
esp_err_t storage_stat(const char *path, struct stat *out_stat);
bool storage_path_exists(const char *path);
bool storage_path_is_dir(const char *path);
esp_err_t storage_create_dir(const char *path);
esp_err_t storage_remove_path(const char *path);
esp_err_t storage_rename_path(const char *from, const char *to);
