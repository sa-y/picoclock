#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <stdbool.h>
#include <stdint.h>

// Stop unused peripheral clocks (ADC, and USB when USB stdio is disabled),
// switch RTC clock source to XOSC, power down PLL_USB when USB stdio is
// disabled, and switch to low-power 12 MHz XOSC clock.
void power_init(void);

// Switch clk_sys and clk_peri to 12 MHz XOSC and power down PLL_SYS.
void power_set_clock_low(void);

// Enable PLL_SYS and restore clk_sys and clk_peri to 125 MHz (for Wi-Fi/NTP).
void power_set_clock_normal(void);

// Enter RP2040 deep sleep (gating all clocks except RTC) until *flag is true.
void power_sleep_until_flag(const volatile bool *flag);

// Enter RP2040 deep sleep (keeping RTC, IO/PADS, and system timer clocks
// active) until *flag is true.
void power_sleep_until_io_or_timer(const volatile bool *flag);

// Enter RP2040 deep sleep (keeping only RTC and system timer clocks active)
// for the specified duration in milliseconds.
void power_sleep_ms(uint32_t ms);

#endif // POWER_MGMT_H
