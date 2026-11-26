#ifndef EPD_SSD1680_H
#define EPD_SSD1680_H

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

// Waveshare Pico-ePaper-2.13 HAT pinout (SPI1)
#define EPD_SPI_PORT spi1
#define EPD_PIN_DC 8
#define EPD_PIN_CS 9
#define EPD_PIN_SCK 10
#define EPD_PIN_MOSI 11
#define EPD_PIN_RST 12
#define EPD_PIN_BUSY 13

// Initialize GPIO/SPI and configure the e-Paper controller for full refresh
void epd_init_full(void);

// Clear the screen to white using full refresh
void epd_clear(void);

// Perform a full refresh with the given 4000-byte framebuffer (1=white,
// 0=black). Also updates the previous-frame RAM (0x26) so subsequent partial
// updates work cleanly.
void epd_display_full(const uint8_t *image);

// Perform a fast partial refresh (no full-screen flash) with the given
// framebuffer.
void epd_display_partial(const uint8_t *image);

// Put the e-Paper controller into deep sleep to protect the panel.
void epd_sleep(void);

#endif // EPD_SSD1680_H
