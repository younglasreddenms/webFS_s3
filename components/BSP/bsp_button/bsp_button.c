/*
 * BSP layer implementation for user button (GPIO22).
 */

#include "bsp_button.h"
#include "esp_log.h"

static const char *TAG = "bsp_button";

esp_err_t bsp_button_init(void)
{
    ESP_LOGI(TAG, "Initializing button on GPIO%u (active-low)", BSP_BUTTON_GPIO);

    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BSP_BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Button ready");
    return ESP_OK;
}

int bsp_button_get_level(void)
{
    return gpio_get_level(BSP_BUTTON_GPIO);
}
