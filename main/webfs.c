#include "webfs.h"

#include "http_file_server.h"

esp_err_t webfs_service_start(const webfs_service_config_t *config)
{
    if (!config || !config->base_path) {
        return ESP_ERR_INVALID_ARG;
    }

    const http_file_server_config_t server_config = {
        .base_path = config->base_path,
        .get_storage_info = config->get_storage_info,
        .storage_info_ctx = config->storage_info_ctx,
    };

    return http_file_server_start(&server_config);
}

esp_err_t webfs_service_stop(void)
{
    return http_file_server_stop();
}

bool webfs_service_is_running(void)
{
    return http_file_server_is_running();
}
