/*
 * BSP layer for user button (GPIO22).
 *
 * Pure hardware abstraction — GPIO pin definition and basic I/O.
 * Debounce, long/short press detection are application-level logic.
 */

#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Pin definitions
 * -------------------------------------------------------------------------- */
#define BSP_BUTTON_GPIO          GPIO_NUM_20
#define BSP_BUTTON_ACTIVE_LEVEL  0            /**< 0 = active-low (button → GND) */

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */

/**
 * @brief 初始化按钮 GPIO (输入 + 上拉)。
 * @return ESP_OK on success.
 */
esp_err_t bsp_button_init(void);

/**
 * @brief 读取按钮当前电平。
 * @return 0 = 按下 (active-low), 1 = 释放。
 */
int bsp_button_get_level(void);

#ifdef __cplusplus
}
#endif
