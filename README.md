# PicoClock

English | [日本語](README.ja.md)

A Wi-Fi NTP digital clock built with a Raspberry Pi Pico W and a Waveshare 2.13-inch e-Paper display.

On startup, it fetches the current time from an NTP server over Wi-Fi, then updates the display every minute using the internal RTC.

## Features

- **Wi-Fi + NTP Time Sync**: Syncs the time with an NTP server on startup and periodically (every 6 hours by default). Disconnects Wi-Fi immediately after syncing.
- **Clean Landscape Layout (250×122)**:
  - Date: Displayed in the top-left corner (`yyyy/mm/dd` format)
  - Time: Displayed in a large 7-segment-style font (`HH:MM` format)
- **Low-Flicker & Burn-In Prevention**:
  - Uses **partial refresh** for minute-by-minute updates to avoid full-screen flashing.
  - Performs a **full refresh** on fixed clock minutes aligned to the top of the hour (at `:00` every hour by default, or `:00`/`:30` when set to `30`) to clear ghosting and prevent burn-in.
  - Puts the e-Paper controller into deep sleep after every update.
- **Power Management**:
  - Disables unused USB/ADC peripheral clocks and powers down `PLL_USB` permanently (USB/UART stdio disabled).
  - Runs at 12 MHz XOSC (`PLL_SYS` off) during normal operation, switching to 125 MHz only during Wi-Fi/NTP communication.
  - Puts the RP2040 into deep sleep while waiting for the next minute RTC alarm or e-Paper BUSY signal.

## Hardware Requirements

- **Raspberry Pi Pico W**
- **Waveshare Pico-ePaper-2.13** (V4 / SSD1680, 250×122, black & white, HAT form factor)

### Pinout (Waveshare Pico-ePaper-2.13 HAT Standard)

| Pico GPIO | Signal | Description |
| :--- | :--- | :--- |
| `GP8` | `DC` | Data / Command selection |
| `GP9` | `CS` | SPI chip select |
| `GP10` | `CLK` | SPI1 clock (SCK) |
| `GP11` | `MOSI` | SPI1 data out (DIN) |
| `GP12` | `RST` | Reset |
| `GP13` | `BUSY` | Busy status input |

## Prerequisites

- **Raspberry Pi Pico SDK** (tested with 2.3.1)
- **CMake** (3.13 or later)
- **ARM GCC Toolchain** / **Ninja**
- (Recommended) VS Code + Raspberry Pi Pico extension

## Setup and Build

### 1. Configure Wi-Fi Credentials

Open `src/app_config.h` and set your Wi-Fi SSID and password:

```c
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

> **Note**: If you publish your source code to GitHub or elsewhere, be careful not to commit `src/app_config.h` with your actual SSID and password.

### 2. Build

Run **Build** from the Raspberry Pi Pico extension in VS Code, or run the following commands in the project root:

```powershell
cmake -B build -G Ninja
cmake --build build
```

When the build succeeds, `build/picoclock.uf2` is generated.

### 3. Flash to Pico W

1. Hold the **BOOTSEL** button on the Raspberry Pi Pico W and connect it to your PC via USB.
2. Copy `build/picoclock.uf2` to the mounted `RPI-RP2` drive.
3. The Pico W will reboot automatically, show `Syncing NTP...` on the screen, and then display the current date and time.

## Customization

### Configuration (`src/app_config.h`)

| Macro | Default Value | Description |
| :--- | :--- | :--- |
| `WIFI_AUTH` | `CYW43_AUTH_WPA2_AES_PSK` | Wi-Fi authentication type |
| `NTP_SERVER` | `"ntp.nict.jp"` | NTP server hostname |
| `NTP_PORT` | `123` | NTP server port |
| `TIMEZONE_OFFSET_S` | `9 * 3600` | Timezone offset in seconds (default: JST, UTC+9) |
| `NTP_RESYNC_MINUTES` | `360` | NTP resync interval in minutes |
| `NTP_RESYNC_RETRY_MINUTES` | `15` | NTP retry interval in minutes after a resync failure |
| `FULL_REFRESH_INTERVAL_MINUTES` | `60` | Full e-Paper refresh interval in minutes aligned to `:00` (must be a divisor of 60, e.g. `60` = `:00`, `30` = `:00`/`:30`) |
| `EPD_ROTATE_180` | `0` | Set to `1` if you mount the display upside-down |

Timeouts and retry parameters (such as `WIFI_CONNECT_TIMEOUT_MS`, `NTP_TIMEOUT_MS`, `NTP_INITIAL_RETRY_DELAY_MS`, `EPD_BUSY_TIMEOUT_MS`, and `EPD_SPI_BAUDRATE`) are also configurable in `src/app_config.h` and can be overridden via compiler `-D` flags.

### Enabling UART / USB stdio for Debugging

UART and USB stdio are disabled by default to minimize power consumption. To view `printf` debug output, enable either UART or USB stdio in `CMakeLists.txt` and rebuild.

#### Option A: UART stdio (recommended for testing power-saving behavior)

1. Open `CMakeLists.txt` and set `pico_enable_stdio_uart` to `1`:

   ```cmake
   pico_enable_stdio_uart(picoclock 1)
   pico_enable_stdio_usb(picoclock 0)
   ```

2. Rebuild and flash `build/picoclock.uf2`:

   ```powershell
   cmake -B build -G Ninja
   cmake --build build
   ```

3. Connect a USB-to-UART adapter (3.3 V logic) to the default UART0 pins (`GP0` = TX, `GP1` = RX, `GND`) and open a serial terminal at **115200 bps (8N1)**.

#### Option B: USB stdio (no extra wiring required)

1. Open `CMakeLists.txt` and set `pico_enable_stdio_usb` to `1`:

   ```cmake
   pico_enable_stdio_uart(picoclock 0)
   pico_enable_stdio_usb(picoclock 1)
   ```

2. Rebuild and flash `build/picoclock.uf2`:

   ```powershell
   cmake -B build -G Ninja
   cmake --build build
   ```

3. Connect the Pico W to your PC via USB and open the virtual serial port (e.g., `COMx` on Windows, `/dev/ttyACM0` on Linux, or `/dev/cu.usbmodem*` on macOS) in a serial terminal.

> **Notes**:
> - `GP0` / `GP1` do not conflict with the e-Paper HAT pins (`GP8`–`GP13`).
> - When **UART stdio** is enabled, `power_mgmt` automatically flushes pending TX data and reconfigures the UART baud rate across 12 MHz / 125 MHz clock transitions, while keeping deep sleep active.
> - When **USB stdio** is enabled, `power_mgmt` automatically keeps `PLL_USB` / `clk_usb` running and uses regular WFI instead of deep-sleep clock gating so the USB CDC connection stays alive.
> - Set both `pico_enable_stdio_uart` and `pico_enable_stdio_usb` back to `0` after debugging for minimum power consumption.

## Project Structure

```text
.
├── CMakeLists.txt          # CMake build configuration
├── pico_sdk_import.cmake   # Pico SDK import script
├── lwipopts.h              # lwIP network stack configuration
├── LICENSE                 # Apache License 2.0
└── src/                    # Application sources
    ├── main.c              # Main application (RTC management & display update loop)
    ├── app_config.h        # Wi-Fi / NTP / refresh interval settings
    ├── epd_ssd1680.h/.c    # Waveshare Pico-ePaper-2.13 (SSD1680) driver
    ├── graphics.h/.c       # 250×122 framebuffer drawing & fonts
    ├── ntp_client.h/.c     # Wi-Fi connection & NTP time client
    └── power_mgmt.h/.c     # Clock control & deep sleep power management
```

## License

This project is licensed under the **Apache License 2.0**. See [LICENSE](LICENSE) for details.

Note that `pico_sdk_import.cmake` included in the build scripts is provided by Raspberry Pi (Trading) Ltd. under the **BSD 3-Clause License**.
