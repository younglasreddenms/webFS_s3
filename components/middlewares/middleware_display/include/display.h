/*
 * Middleware: SSD1306 OLED display.
 *
 * Public surface: 64x128 portrait, page-based rendering.
 * The middleware rotates this onto the 128x64 physical SSD1306 panel.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void display_init(void);
void display_clear(void);
void display_text(int page, const char *str);             /* page 0-15, max 8 chars */
void display_write_page(int page, int column,
                        const uint8_t *data, size_t len); /* logical page partial write */
void display_bitmap(const uint8_t *bitmap_64x128);         /* page order, bit0 is top */
void display_framebuffer(const uint8_t *fb_64x128);        /* raw 1024-byte logical fb */

#define DISPLAY_WIDTH          64
#define DISPLAY_HEIGHT         128
#define DISPLAY_PAGE_COUNT     (DISPLAY_HEIGHT / 8)
#define DISPLAY_BITMAP_SIZE    (DISPLAY_WIDTH * DISPLAY_PAGE_COUNT)
#define DISPLAY_MAX_TEXT_CHARS (DISPLAY_WIDTH / 8)

#ifdef __cplusplus
}
#endif
