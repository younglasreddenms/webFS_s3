#include "storage.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>

#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "storage";

esp_err_t storage_mount(const storage_config_t *config)
{
    if (!config || !config->base_path || !config->partition_label) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_vfs_littlefs_conf_t conf = {
        .base_path = config->base_path,
        .partition_label = config->partition_label,
        .format_if_mount_failed = config->format_if_mount_failed,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = storage_get_info(config->partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted: total=%u used=%u", (unsigned)total, (unsigned)used);
    }
    return ret;
}

esp_err_t storage_get_info(const char *partition_label, size_t *total, size_t *used)
{
    if (!partition_label || !total || !used) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_littlefs_info(partition_label, total, used);
}

esp_err_t storage_stat(const char *path, struct stat *out_stat)
{
    if (!path || !out_stat) {
        return ESP_ERR_INVALID_ARG;
    }
    return stat(path, out_stat) == 0 ? ESP_OK : ESP_FAIL;
}

bool storage_path_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

bool storage_path_is_dir(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

esp_err_t storage_create_dir(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t remove_recursive(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return ESP_FAIL;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path) == 0 ? ESP_OK : ESP_FAIL;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child[320];
        int written = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written < 0 || written >= (int)sizeof(child) || remove_recursive(child) != ESP_OK) {
            ret = ESP_FAIL;
            break;
        }
    }

    closedir(dir);
    if (ret == ESP_OK && rmdir(path) != 0) {
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t storage_remove_path(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    return remove_recursive(path);
}

esp_err_t storage_rename_path(const char *from, const char *to)
{
    if (!from || !to) {
        return ESP_ERR_INVALID_ARG;
    }
    return rename(from, to) == 0 ? ESP_OK : ESP_FAIL;
}
