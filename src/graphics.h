#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t *buf;
  int native_width;
  int native_height;
  int stride_bytes;
  bool rotate_180;
} gfx_canvas_t;

void gfx_clear(gfx_canvas_t *canvas);
void gfx_set_pixel(gfx_canvas_t *canvas, int x, int y, bool black);
void gfx_fill_rect(gfx_canvas_t *canvas, int x, int y, int w, int h,
                   bool black);
void gfx_draw_hline(gfx_canvas_t *canvas, int x, int y, int w, bool black);

// Draw ASCII text using a 5x7 font scaled by `scale` (1 = 5x7, 2 = 10x14, etc.)
void gfx_draw_text(gfx_canvas_t *canvas, int x, int y, const char *str,
                   int scale, bool black);

// Draw large "HH:MM" clock digits starting at top-left (x, y)
void gfx_draw_large_time(gfx_canvas_t *canvas, int x, int y, int hour, int min);

#endif // GRAPHICS_H
