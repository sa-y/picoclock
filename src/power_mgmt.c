#include "power_mgmt.h"

#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/regs/clocks.h"
#include "hardware/regs/m0plus.h"
#include "hardware/rtc.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"
#include "hardware/xosc.h"
#include "pico/time.h"

static void sleep_with_clock_mask(const volatile bool *flag, uint32_t en0_mask,
                                  uint32_t en1_mask) {
  while (!*flag) {
    uint32_t irq_state = save_and_disable_interrupts();
    if (!*flag) {
      uint32_t saved_en0 = clocks_hw->sleep_en0;
      uint32_t saved_en1 = clocks_hw->sleep_en1;
      uint32_t saved_scr = scb_hw->scr;

      clocks_hw->sleep_en0 = en0_mask;
      clocks_hw->sleep_en1 = en1_mask;
      scb_hw->scr = saved_scr | M0PLUS_SCR_SLEEPDEEP_BITS;

      __wfi();

      scb_hw->scr = saved_scr;
      clocks_hw->sleep_en0 = saved_en0;
      clocks_hw->sleep_en1 = saved_en1;
    }
    restore_interrupts(irq_state);
  }
}

void power_init(void) {
  // Disable clocks for unused peripherals (USB and ADC)
  clock_stop(clk_usb);
  clock_stop(clk_adc);

  // Switch clk_rtc from PLL_USB to XOSC (12 MHz / 256 = 46875 Hz)
  // so PLL_USB can be powered down permanently without affecting the RTC.
  clock_configure_int_divider(clk_rtc, 0,
                              CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
                              XOSC_HZ, XOSC_HZ / RTC_CLOCK_FREQ_HZ);
  pll_deinit(pll_usb);

  // Run at 12 MHz XOSC with PLL_SYS off by default
  power_set_clock_low();
}

void power_set_clock_low(void) {
  // Switch clk_sys to clk_ref (12 MHz XOSC) via the glitchless mux
  clock_configure_undivided(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0,
                            XOSC_HZ);

  // Update clk_peri to follow the 12 MHz clk_sys
  clock_configure_undivided(clk_peri, 0,
                            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, XOSC_HZ);

  // Power down PLL_SYS
  pll_deinit(pll_sys);
}

void power_set_clock_normal(void) {
  // Ensure clk_sys is on clk_ref before re-initializing PLL_SYS
  clock_configure_undivided(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0,
                            XOSC_HZ);

  pll_init(pll_sys, PLL_SYS_REFDIV, PLL_SYS_VCO_FREQ_HZ, PLL_SYS_POSTDIV1,
           PLL_SYS_POSTDIV2);

  // Switch clk_sys and clk_peri back to 125 MHz PLL_SYS
  clock_configure_undivided(clk_sys,
                            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                            SYS_CLK_HZ);

  clock_configure_undivided(clk_peri, 0,
                            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                            SYS_CLK_HZ);
}

void power_sleep_until_flag(const volatile bool *flag) {
  sleep_with_clock_mask(flag, CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS, 0);
}

void power_sleep_until_io_or_timer(const volatile bool *flag) {
  const uint32_t en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS |
                       CLOCKS_SLEEP_EN0_CLK_SYS_IO_BITS |
                       CLOCKS_SLEEP_EN0_CLK_SYS_PADS_BITS;
  const uint32_t en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS;
  sleep_with_clock_mask(flag, en0, en1);
}

static int64_t sleep_alarm_cb(alarm_id_t id, void *user_data) {
  (void)id;
  volatile bool *done = (volatile bool *)user_data;
  *done = true;
  return 0;
}

void power_sleep_ms(uint32_t ms) {
  if (ms == 0) {
    return;
  }

  volatile bool done = false;
  alarm_id_t alarm = add_alarm_in_ms(ms, sleep_alarm_cb, (void *)&done, true);
  if (alarm < 0) {
    sleep_ms(ms);
    return;
  }
  if (alarm > 0) {
    sleep_with_clock_mask(&done, CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS,
                          CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS);
    cancel_alarm(alarm);
  }
}
