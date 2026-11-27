#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Wi-Fi settings (replace with your network credentials)
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#ifndef WIFI_AUTH
#define WIFI_AUTH CYW43_AUTH_WPA2_AES_PSK
#endif
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 20000
#endif

// NTP server, timeouts, and timezone offset (JST = UTC+9)
#ifndef NTP_SERVER
#define NTP_SERVER "ntp.nict.jp"
#endif
#ifndef NTP_PORT
#define NTP_PORT 123
#endif
#ifndef TIMEZONE_OFFSET_S
#define TIMEZONE_OFFSET_S (9 * 3600)
#endif
#ifndef NTP_TIMEOUT_MS
#define NTP_TIMEOUT_MS 5000
#endif
#ifndef NTP_MAX_ATTEMPTS
#define NTP_MAX_ATTEMPTS 3
#endif
#ifndef NTP_RETRY_DELAY_MS
#define NTP_RETRY_DELAY_MS 500
#endif

// Initial NTP sync retry delay in milliseconds (30000 = 30 seconds)
#ifndef NTP_INITIAL_RETRY_DELAY_MS
#define NTP_INITIAL_RETRY_DELAY_MS 30000
#endif

// Resync interval in minutes (360 = 6 hours) and retry interval on failure
#ifndef NTP_RESYNC_MINUTES
#define NTP_RESYNC_MINUTES 360
#endif
#ifndef NTP_RESYNC_RETRY_MINUTES
#define NTP_RESYNC_RETRY_MINUTES 15
#endif

// Display & e-Paper settings
// Perform a full e-Paper refresh every N partial updates to clear ghosting
#ifndef FULL_REFRESH_INTERVAL_MINUTES
#define FULL_REFRESH_INTERVAL_MINUTES 60
#endif
// Set to 1 if you mount the display upside-down
#ifndef EPD_ROTATE_180
#define EPD_ROTATE_180 0
#endif
#ifndef EPD_SPI_BAUDRATE
#define EPD_SPI_BAUDRATE (4 * 1000 * 1000)
#endif
#ifndef EPD_BUSY_TIMEOUT_MS
#define EPD_BUSY_TIMEOUT_MS 10000
#endif

#endif // APP_CONFIG_H
