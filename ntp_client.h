#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include "pico/util/datetime.h"
#include <stdbool.h>

// Connect to Wi-Fi, query NTP server, and populate `out_dt` in local time.
// Disconnects Wi-Fi before returning. Returns true on success.
bool ntp_sync_datetime(datetime_t *out_dt);

#endif // NTP_CLIENT_H
