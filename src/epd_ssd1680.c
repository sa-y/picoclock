#include "epd_ssd1680.h"
#include "app_config.h"
#include "power_mgmt.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <stdbool.h>

// SSD1680 command definitions
enum {
  EPD_CMD_DRIVER_OUTPUT_CTRL = 0x01,
  EPD_CMD_DEEP_SLEEP = 0x10,
  EPD_CMD_DATA_ENTRY_MODE = 0x11,
  EPD_CMD_SW_RESET = 0x12,
  EPD_CMD_TEMP_SENSOR_CTRL = 0x18,
  EPD_CMD_MASTER_ACTIVATION = 0x20,
  EPD_CMD_DISP_UPDATE_CTRL1 = 0x21,
  EPD_CMD_DISP_UPDATE_CTRL2 = 0x22,
  EPD_CMD_WRITE_RAM_BW = 0x24,
  EPD_CMD_WRITE_RAM_RED = 0x26,
  EPD_CMD_BORDER_WAVEFORM = 0x3C,
  EPD_CMD_SET_RAM_X_RANGE = 0x44,
  EPD_CMD_SET_RAM_Y_RANGE = 0x45,
  EPD_CMD_SET_RAM_X_ADDR = 0x4E,
  EPD_CMD_SET_RAM_Y_ADDR = 0x4F,
};

static volatile bool s_epd_wait_done = false;

static void epd_busy_irq_handler(void) {
  uint32_t events = gpio_get_irq_event_mask(EPD_PIN_BUSY);
  if (events & GPIO_IRQ_LEVEL_LOW) {
    gpio_set_irq_enabled(EPD_PIN_BUSY, GPIO_IRQ_LEVEL_LOW, false);
    gpio_acknowledge_irq(EPD_PIN_BUSY, events);
    s_epd_wait_done = true;
  }
}

static int64_t epd_timeout_alarm_cb(alarm_id_t id, void *user_data) {
  (void)id;
  (void)user_data;
  s_epd_wait_done = true;
  return 0;
}

static void epd_hw_init(void) {
  static bool initialized = false;
  if (initialized) {
    spi_set_baudrate(EPD_SPI_PORT, EPD_SPI_BAUDRATE);
    return;
  }
  initialized = true;

  spi_init(EPD_SPI_PORT, EPD_SPI_BAUDRATE);
  gpio_set_function(EPD_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(EPD_PIN_MOSI, GPIO_FUNC_SPI);

  gpio_init(EPD_PIN_CS);
  gpio_set_dir(EPD_PIN_CS, GPIO_OUT);
  gpio_put(EPD_PIN_CS, 1);

  gpio_init(EPD_PIN_DC);
  gpio_set_dir(EPD_PIN_DC, GPIO_OUT);
  gpio_put(EPD_PIN_DC, 0);

  gpio_init(EPD_PIN_RST);
  gpio_set_dir(EPD_PIN_RST, GPIO_OUT);
  gpio_put(EPD_PIN_RST, 1);

  gpio_init(EPD_PIN_BUSY);
  gpio_set_dir(EPD_PIN_BUSY, GPIO_IN);
  gpio_add_raw_irq_handler(EPD_PIN_BUSY, epd_busy_irq_handler);
  irq_set_enabled(IO_IRQ_BANK0, true);
}

static void epd_reset(void) {
  gpio_put(EPD_PIN_RST, 1);
  sleep_ms(20);
  gpio_put(EPD_PIN_RST, 0);
  sleep_ms(2);
  gpio_put(EPD_PIN_RST, 1);
  sleep_ms(20);
}

static bool epd_wait_busy(void) {
  // Wait until BUSY goes low (max ~10s safety timeout)
  if (gpio_get(EPD_PIN_BUSY) == 0) {
    return true;
  }

  s_epd_wait_done = false;
  alarm_id_t alarm =
      add_alarm_in_ms(EPD_BUSY_TIMEOUT_MS, epd_timeout_alarm_cb, NULL, true);
  gpio_set_irq_enabled(EPD_PIN_BUSY, GPIO_IRQ_LEVEL_LOW, true);

  power_sleep_until_io_or_timer(&s_epd_wait_done);

  gpio_set_irq_enabled(EPD_PIN_BUSY, GPIO_IRQ_LEVEL_LOW, false);
  if (alarm > 0) {
    cancel_alarm(alarm);
  }

  return gpio_get(EPD_PIN_BUSY) == 0;
}

static void epd_send_cmd(uint8_t cmd) {
  gpio_put(EPD_PIN_DC, 0);
  gpio_put(EPD_PIN_CS, 0);
  spi_write_blocking(EPD_SPI_PORT, &cmd, 1);
  gpio_put(EPD_PIN_CS, 1);
}

static void epd_send_data(uint8_t data) {
  gpio_put(EPD_PIN_DC, 1);
  gpio_put(EPD_PIN_CS, 0);
  spi_write_blocking(EPD_SPI_PORT, &data, 1);
  gpio_put(EPD_PIN_CS, 1);
}

static void epd_send_data_buf(const uint8_t *buf, size_t len) {
  gpio_put(EPD_PIN_DC, 1);
  gpio_put(EPD_PIN_CS, 0);
  spi_write_blocking(EPD_SPI_PORT, buf, len);
  gpio_put(EPD_PIN_CS, 1);
}

static void epd_set_window(uint16_t x_start_byte, uint16_t y_start,
                           uint16_t x_end_byte, uint16_t y_end) {
  epd_send_cmd(EPD_CMD_SET_RAM_X_RANGE);
  epd_send_data((uint8_t)(x_start_byte & 0xFF));
  epd_send_data((uint8_t)(x_end_byte & 0xFF));

  epd_send_cmd(EPD_CMD_SET_RAM_Y_RANGE);
  epd_send_data((uint8_t)(y_start & 0xFF));
  epd_send_data((uint8_t)((y_start >> 8) & 0xFF));
  epd_send_data((uint8_t)(y_end & 0xFF));
  epd_send_data((uint8_t)((y_end >> 8) & 0xFF));
}

static void epd_set_cursor(uint16_t x_byte, uint16_t y) {
  epd_send_cmd(EPD_CMD_SET_RAM_X_ADDR);
  epd_send_data((uint8_t)(x_byte & 0xFF));

  epd_send_cmd(EPD_CMD_SET_RAM_Y_ADDR);
  epd_send_data((uint8_t)(y & 0xFF));
  epd_send_data((uint8_t)((y >> 8) & 0xFF));
}

static void epd_configure_scan_and_window(void) {
  const uint16_t max_gate = EPD_NATIVE_HEIGHT - 1;
  epd_send_cmd(EPD_CMD_DRIVER_OUTPUT_CTRL);
  epd_send_data((uint8_t)(max_gate & 0xFF));
  epd_send_data((uint8_t)((max_gate >> 8) & 0xFF));
  epd_send_data(0x00);

  epd_send_cmd(EPD_CMD_DATA_ENTRY_MODE); // X inc, Y inc
  epd_send_data(0x03);

  epd_set_window(0, 0, EPD_STRIDE_BYTES - 1, max_gate);
}

static bool epd_turn_on_full(void) {
  epd_send_cmd(EPD_CMD_DISP_UPDATE_CTRL2);
  epd_send_data(0xF7);
  epd_send_cmd(EPD_CMD_MASTER_ACTIVATION);
  return epd_wait_busy();
}

static bool epd_turn_on_partial(void) {
  epd_send_cmd(EPD_CMD_DISP_UPDATE_CTRL2);
  epd_send_data(0xFF);
  epd_send_cmd(EPD_CMD_MASTER_ACTIVATION);
  return epd_wait_busy();
}

bool epd_init_full(void) {
  epd_hw_init();
  epd_reset();

  if (!epd_wait_busy()) {
    return false;
  }
  epd_send_cmd(EPD_CMD_SW_RESET);
  if (!epd_wait_busy()) {
    return false;
  }

  epd_configure_scan_and_window();

  epd_send_cmd(EPD_CMD_BORDER_WAVEFORM);
  epd_send_data(0x05);

  epd_send_cmd(EPD_CMD_DISP_UPDATE_CTRL1);
  epd_send_data(0x00);
  epd_send_data(0x80);

  epd_send_cmd(EPD_CMD_TEMP_SENSOR_CTRL); // Read built-in temperature sensor
  epd_send_data(0x80);

  epd_set_cursor(0, 0);
  return epd_wait_busy();
}

bool epd_display_full(const uint8_t *image) {
  if (image == NULL) {
    return false;
  }

  epd_set_cursor(0, 0);
  epd_send_cmd(EPD_CMD_WRITE_RAM_BW);
  epd_send_data_buf(image, EPD_BUFFER_SIZE);

  // Also write RED/previous RAM (0x26) as base for partial updates
  epd_set_cursor(0, 0);
  epd_send_cmd(EPD_CMD_WRITE_RAM_RED);
  epd_send_data_buf(image, EPD_BUFFER_SIZE);

  return epd_turn_on_full();
}

bool epd_display_partial(const uint8_t *image) {
  if (image == NULL) {
    return false;
  }

  epd_hw_init();

  // Brief hardware reset wakes from sleep and prepares partial update
  gpio_put(EPD_PIN_RST, 0);
  sleep_ms(2);
  gpio_put(EPD_PIN_RST, 1);
  sleep_ms(2);
  if (!epd_wait_busy()) {
    return false;
  }

  epd_send_cmd(EPD_CMD_BORDER_WAVEFORM); // BorderWaveform for partial update
  epd_send_data(0x80);

  epd_configure_scan_and_window();
  epd_set_cursor(0, 0);

  epd_send_cmd(EPD_CMD_WRITE_RAM_BW);
  epd_send_data_buf(image, EPD_BUFFER_SIZE);

  return epd_turn_on_partial();
}

void epd_sleep(void) {
  epd_send_cmd(EPD_CMD_DEEP_SLEEP); // Deep sleep mode 1
  epd_send_data(0x01);
  sleep_ms(100);
}
