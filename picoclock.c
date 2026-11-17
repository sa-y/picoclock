#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include <stdio.h>

#include "epd_2in13.h"
#include "gfx.h"
#include "ntp_client.h"
#include "power_mgmt.h"
#include "wifi_config.h"

static uint8_t frame_buf[EPD_BUFFER_SIZE];
static volatile bool s_minute_tick = false;

static void rtc_alarm_callback(void) {
  s_minute_tick = true;
}

static void render_status(const char *line1, const char *line2) {
  gfx_clear(frame_buf);
  if (line1) {
    gfx_draw_text(frame_buf, 10, 34, line1, 2, true);
  }
  if (line2) {
    gfx_draw_text(frame_buf, 10, 62, line2, 2, true);
  }
}

static void render_clock(const datetime_t *dt) {
  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%04d/%02d/%02d", dt->year, dt->month,
           dt->day);

  gfx_clear(frame_buf);
  // Date in top-left corner, smaller than time
  gfx_draw_text(frame_buf, 8, 6, date_str, 2, true);
  // Subtle divider line
  gfx_draw_hline(frame_buf, 8, 26, EPD_LANDSCAPE_WIDTH - 16, true);
  // Large HH:MM centered in the main area
  gfx_draw_large_time(frame_buf, 14, 36, dt->hour, dt->min);
}

int main(void) {
  stdio_init_all();
  power_init();
  rtc_init();

  epd_init_full();
  render_status("PicoClock", "Syncing NTP...");
  epd_display_full(frame_buf);
  epd_sleep();

  datetime_t now = {0};
  bool sync_err_shown = false;
  while (!ntp_sync_datetime(&now)) {
    if (!sync_err_shown) {
      epd_init_full();
      render_status("NTP Sync Failed", "Retrying in 30s");
      epd_display_full(frame_buf);
      epd_sleep();
      sync_err_shown = true;
    }
    sleep_ms(30000);
  }

  rtc_set_datetime(&now);
  // Wait briefly for RTC registers to latch
  sleep_ms(100);
  rtc_get_datetime(&now);

  epd_init_full();
  render_clock(&now);
  epd_display_full(frame_buf);
  epd_sleep();

  int8_t last_min = now.min;
  int partial_count = 0;
  int resync_minutes = 0;

  // Trigger an RTC alarm at sec == 0 every minute
  const datetime_t alarm_dt = {
      .year = -1,
      .month = -1,
      .day = -1,
      .dotw = -1,
      .hour = -1,
      .min = -1,
      .sec = 0,
  };
  rtc_set_alarm(&alarm_dt, rtc_alarm_callback);

  while (true) {
    s_minute_tick = false;
    power_sleep_until_flag(&s_minute_tick);

    if (!rtc_get_datetime(&now)) {
      continue;
    }
    if (now.min == last_min) {
      continue;
    }

    last_min = now.min;
    partial_count++;
    resync_minutes++;

    // Update the display for the new minute before any blocking NTP sync
    render_clock(&now);
    if (partial_count >= FULL_REFRESH_INTERVAL_MINUTES) {
      epd_init_full();
      epd_display_full(frame_buf);
      epd_sleep();
      partial_count = 0;
    } else {
      epd_display_partial(frame_buf);
      epd_sleep();
    }

    if (resync_minutes >= NTP_RESYNC_MINUTES) {
      resync_minutes = 0;
      datetime_t synced = {0};
      if (ntp_sync_datetime(&synced)) {
        rtc_set_datetime(&synced);
        sleep_ms(100);
        if (synced.min != now.min || synced.hour != now.hour ||
            synced.day != now.day) {
          now = synced;
          last_min = now.min;
          render_clock(&now);
          epd_display_partial(frame_buf);
          epd_sleep();
        }
      }
    }
  }

  return 0;
}
