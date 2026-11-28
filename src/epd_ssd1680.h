#ifndef EPD_SSD1680_H
#define EPD_SSD1680_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Waveshare Pico-ePaper-2.13 (V4 / SSD1680) native resolution
#define EPD_NATIVE_WIDTH 122
#define EPD_NATIVE_HEIGHT 250
#define EPD_STRIDE_BYTES ((EPD_NATIVE_WIDTH + 7) / 8) // 16 bytes (128 bits)
#define EPD_BUFFER_SIZE (EPD_STRIDE_BYTES * EPD_NATIVE_HEIGHT) // 4000 bytes

// Landscape logical dimensions
#define EPD_LANDSCAPE_WIDTH EPD_NATIVE_HEIGHT // 250
#define EPD_LANDSCAPE_HEIGHT EPD_NATIVE_WIDTH // 122

// Waveshare Pico-ePaper-2.13 HAT pinout (SPI1), overridable via -D flags
#ifndef EPD_SPI_PORT
#define EPD_SPI_PORT spi1
#endif
#ifndef EPD_PIN_DC
#define EPD_PIN_DC 8
#endif
#ifndef EPD_PIN_CS
#define EPD_PIN_CS 9
#endif
#ifndef EPD_PIN_SCK
#define EPD_PIN_SCK 10
#endif
#ifndef EPD_PIN_MOSI
#define EPD_PIN_MOSI 11
#endif
#ifndef EPD_PIN_RST
#define EPD_PIN_RST 12
#endif
#ifndef EPD_PIN_BUSY
#define EPD_PIN_BUSY 13
#endif

// Initialize GPIO/SPI and configure the e-Paper controller for full refresh.
// Returns true on success, or false if the controller timed out.
bool epd_init_full(void);

// Perform a full refresh with the given 4000-byte framebuffer (1=white,
// 0=black). Also updates the previous-frame RAM (0x26) so subsequent partial
// updates work cleanly. Returns true on success.
bool epd_display_full(const uint8_t *image);

// Perform a fast partial refresh (no full-screen flash) with the given
// framebuffer. Returns true on success.
bool epd_display_partial(const uint8_t *image);

// Put the e-Paper controller into deep sleep to protect the panel.
void epd_sleep(void);

#endif // EPD_SSD1680_H
