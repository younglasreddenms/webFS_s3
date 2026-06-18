/*
 * BSP layer for SSD1306 OLED (I2C).
 *
 * Physical panel: SDA=GPIO8, SCL=GPIO9, I2C addr=0x3C, 128x64.
 * Higher display code may expose a rotated 64x128 logical surface.
 */

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_OLED_I2C_PORT    I2C_NUM_0
#define BSP_OLED_I2C_ADDR    0x3C
#define BSP_OLED_SDA_PIN     GPIO_NUM_33
#define BSP_OLED_SCL_PIN     GPIO_NUM_32
#define BSP_OLED_I2C_FREQ_HZ 400000
#define BSP_OLED_WIDTH       128
#define BSP_OLED_HEIGHT      64

/** @brief Init I2C + SSD1306 and clear the screen. */
void bsp_oled_init(void);

/** @brief Send a single command byte to the SSD1306. */
void bsp_oled_write_cmd(uint8_t cmd);

/** @brief Send data bytes to the SSD1306 (prepends 0x40 marker). */
void bsp_oled_write_data(const uint8_t *data, size_t len);

/** @brief Set the current hardware page and column. */
void bsp_oled_set_page_column(uint8_t page, uint8_t column);

/** @brief Write a byte range inside one hardware page. */
void bsp_oled_write_page(uint8_t page, uint8_t column,
                         const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
