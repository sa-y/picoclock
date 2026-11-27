#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include <stdio.h>

#include "app_config.h"
#include "epd_ssd1680.h"
#include "graphics.h"
#include "ntp_client.h"
#include "power_mgmt.h"

typedef struct {
  datetime_t last_displayed;
  int partial_count;
  int minutes_until_resync;
} clock_state_t;

static uint8_t frame_buf[EPD_BUFFER_SIZE];
static gfx_canvas_t s_canvas = {
    .buf = frame_buf,
    .native_width = EPD_NATIVE_WIDTH,
    .native_height = EPD_NATIVE_HEIGHT,
    .stride_bytes = EPD_STRIDE_BYTES,
    .rotate_180 = (EPD_ROTATE_180 != 0),
};
static volatile bool s_minute_tick = false;

static void rtc_alarm_callback(void) { s_minute_tick = true; }

static bool is_same_minute(const datetime_t *a, const datetime_t *b) {
  return a->year == b->year && a->month == b->month && a->day == b->day &&
         a->hour == b->hour && a->min == b->min;
}

static void render_status(const char *line1, const char *line2) {
  gfx_clear(&s_canvas);
  if (line1) {
    gfx_draw_text(&s_canvas, 10, 34, line1, 2, true);
  }
  if (line2) {
    gfx_draw_text(&s_canvas, 10, 62, line2, 2, true);
  }
}

static void render_clock(const datetime_t *dt) {
  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%04d/%02d/%02d", dt->year, dt->month,
           dt->day);

  gfx_clear(&s_canvas);
  // Date in top-left corner, smaller than time
  gfx_draw_text(&s_canvas, 8, 6, date_str, 2, true);
  // Subtle divider line
  gfx_draw_hline(&s_canvas, 8, 26, EPD_LANDSCAPE_WIDTH - 16, true);
  // Large HH:MM centered in the main area
  gfx_draw_large_time(&s_canvas, 14, 36, dt->hour, dt->min);
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

static void apply_rtc_datetime(datetime_t *dt) {
  rtc_set_datetime(dt);
  // Wait briefly for RTC registers to latch
  sleep_ms(100);
  rtc_get_datetime(dt);
}

static void update_clock_display(clock_state_t *state, const datetime_t *dt,
                                 bool force_full) {
  const bool use_full =
      force_full || (state->partial_count + 1 >= FULL_REFRESH_INTERVAL_MINUTES);

  render_clock(dt);
  if (use_full) {
    state->partial_count =
        display_full_frame() ? 0 : FULL_REFRESH_INTERVAL_MINUTES;
  } else if (display_partial_frame()) {
    state->partial_count++;
  } else {
    // Force a full refresh (with reset/re-init) on the next update
    state->partial_count = FULL_REFRESH_INTERVAL_MINUTES;
  }

  state->last_displayed = *dt;
}

static bool sync_ntp_datetime(datetime_t *out_dt) {
  power_set_clock_normal();
  bool ok = ntp_sync_datetime(out_dt);
  power_set_clock_low();
  return ok;
}

static void resync_ntp_if_due(clock_state_t *state) {
  if (state->minutes_until_resync > 0) {
    return;
  }

  datetime_t synced = {0};
  if (!sync_ntp_datetime(&synced)) {
    int retry = NTP_RESYNC_RETRY_MINUTES;
    if (retry > NTP_RESYNC_MINUTES) {
      retry = NTP_RESYNC_MINUTES;
    }
    state->minutes_until_resync = retry > 0 ? retry : 1;
    return;
  }

  state->minutes_until_resync = NTP_RESYNC_MINUTES;
  apply_rtc_datetime(&synced);
  if (!is_same_minute(&synced, &state->last_displayed)) {
    update_clock_display(state, &synced, false);
  }
}

int main(void) {
#if LIB_PICO_STDIO_UART || LIB_PICO_STDIO_USB
  stdio_init_all();
#endif
  power_init();
  rtc_init();

  render_status("PicoClock", "Syncing NTP...");
  (void)display_full_frame();

  datetime_t now = {0};
  bool sync_err_shown = false;
  while (!sync_ntp_datetime(&now)) {
    if (!sync_err_shown) {
      char retry_msg[24];
      snprintf(retry_msg, sizeof(retry_msg), "Retrying in %us",
               (unsigned)(NTP_INITIAL_RETRY_DELAY_MS / 1000));
      render_status("NTP Sync Failed", retry_msg);
      sync_err_shown = display_full_frame();
    }
    power_sleep_ms(NTP_INITIAL_RETRY_DELAY_MS);
  }

  apply_rtc_datetime(&now);

  clock_state_t state = {
      .last_displayed = now,
      .partial_count = 0,
      .minutes_until_resync = NTP_RESYNC_MINUTES,
  };
  update_clock_display(&state, &now, true);

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
    if (is_same_minute(&now, &state.last_displayed)) {
      power_sleep_until_flag(&s_minute_tick);
      continue;
    }

    if (state.minutes_until_resync > 0) {
      state.minutes_until_resync--;
    }

    // Update the display for the new minute before any blocking NTP sync
    update_clock_display(&state, &now, false);
    resync_ntp_if_due(&state);
  }

  return 0;
}
