#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// Wi-Fi settings (replace with your network credentials)
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define WIFI_AUTH CYW43_AUTH_WPA2_AES_PSK

// NTP server and timezone offset (JST = UTC+9)
#define NTP_SERVER "ntp.nict.jp"
#define NTP_PORT 123
#define TIMEZONE_OFFSET_S (9 * 3600)

// Resync interval in minutes (360 = 6 hours)
#define NTP_RESYNC_MINUTES 360

// Perform a full e-Paper refresh every N partial updates to clear ghosting
#define FULL_REFRESH_INTERVAL_MINUTES 60

#endif // WIFI_CONFIG_H
