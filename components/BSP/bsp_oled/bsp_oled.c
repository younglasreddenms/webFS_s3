/*
 * BSP layer implementation for SSD1306 OLED over I2C (ESP-IDF v6.0 API).
 */

#include "bsp_oled.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bsp_oled";

static i2c_master_bus_handle_t  s_bus   = NULL;
static i2c_master_dev_handle_t  s_dev   = NULL;

/* ── Low-level helpers ────────────────────────────── */

void bsp_oled_write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

void bsp_oled_write_data(const uint8_t *data, size_t len)
{
    uint8_t buf[129];
    while (len > 0) {
        size_t chunk = (len > 128) ? 128 : len;
        buf[0] = 0x40;
        memcpy(buf + 1, data, chunk);
        i2c_master_transmit(s_dev, buf, chunk + 1, 100);
        data += chunk;
        len -= chunk;
    }
}

/* ── Init ─────────────────────────────────────────── */

void bsp_oled_set_page_column(uint8_t page, uint8_t column)
{
    bsp_oled_write_cmd(0xB0 | (page & 0x0F));
    bsp_oled_write_cmd(0x00 | (column & 0x0F));
    bsp_oled_write_cmd(0x10 | ((column >> 4) & 0x0F));
}

void bsp_oled_write_page(uint8_t page, uint8_t column,
                         const uint8_t *data, size_t len)
{
    const uint8_t page_count = BSP_OLED_HEIGHT / 8;

    if (!data || page >= page_count || column >= BSP_OLED_WIDTH || len == 0) {
        return;
    }

    if (len > BSP_OLED_WIDTH - column) {
        len = BSP_OLED_WIDTH - column;
    }

    bsp_oled_set_page_column(page, column);
    bsp_oled_write_data(data, len);
}

void bsp_oled_init(void)
{
    ESP_LOGI(TAG, "Init I2C%u (SDA=%d, SCL=%d, addr=0x%02X)",
             BSP_OLED_I2C_PORT, BSP_OLED_SDA_PIN, BSP_OLED_SCL_PIN,
             BSP_OLED_I2C_ADDR);

    /* I2C master bus */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port    = BSP_OLED_I2C_PORT,
        .sda_io_num  = BSP_OLED_SDA_PIN,
        .scl_io_num  = BSP_OLED_SCL_PIN,
        .clk_source  = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    /* I2C device */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BSP_OLED_I2C_ADDR,
        .scl_speed_hz    = BSP_OLED_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    /* SSD1306 init sequence */
    static const uint8_t init_cmds[] = {
        0xAE,        /* Display off */
        0xD5, 0x80,  /* Clock divide */
        0xA8, 0x3F,  /* Multiplex 64 */
        0xD3, 0x00,  /* Offset */
        0x40,        /* Start line */
        0x8D, 0x14,  /* Charge pump */
        0x20, 0x02,  /* Page addressing */
        0xA1,        /* Segment remap */
        0xC8,        /* COM scan direction */
        0xDA, 0x12,  /* COM pins */
        0x81, 0xCF,  /* Contrast */
        0xD9, 0xF1,  /* Pre-charge */
        0xDB, 0x40,  /* VCOM detect */
        0xA4,        /* RAM content */
        0xA6,        /* Normal display */
        0xAF,        /* Display on */
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        bsp_oled_write_cmd(init_cmds[i]);
    }

    ESP_LOGI(TAG, "Ready (%dx%d)", BSP_OLED_WIDTH, BSP_OLED_HEIGHT);
}
