#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include <stdio.h>

#include "app_config.h"
#include "epd_ssd1680.h"
#include "graphics.h"
#include "ntp_client.h"
#include "power_mgmt.h"

static uint8_t frame_buf[EPD_BUFFER_SIZE];
static volatile bool s_minute_tick = false;

static void rtc_alarm_callback(void) { s_minute_tick = true; }

static bool is_same_minute(const datetime_t *a, const datetime_t *b) {
  return a->year == b->year && a->month == b->month && a->day == b->day &&
         a->hour == b->hour && a->min == b->min;
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

static bool display_full_frame(void) {
  bool ok = epd_init_full() && epd_display_full(frame_buf);
  epd_sleep();
  return ok;
}

static bool display_partial_frame(void) {
  bool ok = epd_display_partial(frame_buf);
  epd_sleep();
  return ok;
}

int main(void) {
  stdio_init_all();
  power_init();
  rtc_init();

  render_status("PicoClock", "Syncing NTP...");
  (void)display_full_frame();

  datetime_t now = {0};
  bool sync_err_shown = false;
  while (!ntp_sync_datetime(&now)) {
    if (!sync_err_shown) {
      render_status("NTP Sync Failed", "Retrying in 30s");
      sync_err_shown = display_full_frame();
    }
    power_sleep_ms(30000);
  }

  rtc_set_datetime(&now);
  // Wait briefly for RTC registers to latch
  sleep_ms(100);
  rtc_get_datetime(&now);

  render_clock(&now);
  int partial_count = display_full_frame() ? 0 : FULL_REFRESH_INTERVAL_MINUTES;

  datetime_t last_displayed = now;
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
    // Clear the flag before reading the RTC so any minute-boundary alarm
    // firing during or after rtc_get_datetime() is preserved.
    s_minute_tick = false;

    if (!rtc_get_datetime(&now)) {
      power_sleep_until_flag(&s_minute_tick);
      continue;
    }
    if (is_same_minute(&now, &last_displayed)) {
      power_sleep_until_flag(&s_minute_tick);
      continue;
    }

    last_displayed = now;
    partial_count++;
    resync_minutes++;

    // Update the display for the new minute before any blocking NTP sync
    render_clock(&now);
    if (partial_count >= FULL_REFRESH_INTERVAL_MINUTES) {
      if (display_full_frame()) {
        partial_count = 0;
      } else {
        partial_count = FULL_REFRESH_INTERVAL_MINUTES;
      }
    } else {
      if (!display_partial_frame()) {
        // Force a full refresh (with reset/re-init) on the next update
        partial_count = FULL_REFRESH_INTERVAL_MINUTES;
      }
    }

    if (resync_minutes >= NTP_RESYNC_MINUTES) {
      resync_minutes = 0;
      datetime_t synced = {0};
      if (ntp_sync_datetime(&synced)) {
        rtc_set_datetime(&synced);
        sleep_ms(100);
        if (!is_same_minute(&synced, &now)) {
          now = synced;
          last_displayed = now;
          render_clock(&now);
          if (!display_partial_frame()) {
            partial_count = FULL_REFRESH_INTERVAL_MINUTES;
          }
        }
      }
    }
  }

  return 0;
}
