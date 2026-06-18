#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "storage.h"
#include "webfs.h"
#include "wifi_manager.h"

#define APP_STORAGE_BASE_PATH "/littlefs"
#define APP_STORAGE_PARTITION_LABEL "storage"

#define APP_WIFI_SSID "sensen"
#define APP_WIFI_PASSWORD "11111111"
#define APP_WIFI_MAX_RETRY 10

static const char *TAG = "main";

static esp_err_t app_get_storage_info(const void *ctx, size_t *total, size_t *used)
{
    return storage_get_info((const char *)ctx, total, used);
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

void app_main(void)
{
    const storage_config_t storage_config = {
        .base_path = APP_STORAGE_BASE_PATH,
        .partition_label = APP_STORAGE_PARTITION_LABEL,
        .format_if_mount_failed = true,
    };
    const wifi_manager_sta_config_t wifi_config = {
        .ssid = APP_WIFI_SSID,
        .password = APP_WIFI_PASSWORD,
        .max_retry = APP_WIFI_MAX_RETRY,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const webfs_service_config_t webfs_config = {
        .base_path = APP_STORAGE_BASE_PATH,
        .get_storage_info = app_get_storage_info,
        .storage_info_ctx = APP_STORAGE_PARTITION_LABEL,
    };

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(storage_mount(&storage_config));
    ESP_ERROR_CHECK(wifi_manager_sta_start(&wifi_config));
    ESP_ERROR_CHECK(webfs_service_start(&webfs_config));

    ESP_LOGI(TAG, "Services are ready");
}
