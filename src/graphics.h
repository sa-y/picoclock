#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "epd_ssd1680.h"
#include <stdbool.h>
#include <stdint.h>

// Set to 1 if you mount the display upside-down
#define EPD_ROTATE_180 0

void gfx_clear(uint8_t *buf);
void gfx_set_pixel(uint8_t *buf, int x, int y, bool black);
void gfx_fill_rect(uint8_t *buf, int x, int y, int w, int h, bool black);
void gfx_draw_hline(uint8_t *buf, int x, int y, int w, bool black);

// Draw ASCII text using a 5x7 font scaled by `scale` (1 = 5x7, 2 = 10x14, etc.)
void gfx_draw_text(uint8_t *buf, int x, int y, const char *str, int scale,
                   bool black);

// Draw large "HH:MM" clock digits centered at (x, y)
void gfx_draw_large_time(uint8_t *buf, int x, int y, int hour, int min);

#endif // GRAPHICS_H
