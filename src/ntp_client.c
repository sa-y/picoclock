#include "ntp_client.h"
#include "app_config.h"

#include <string.h>
#include <time.h>

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define NTP_MSG_LEN 48
#define NTP_DELTA 2208988800UL
#define NTP_POLL_INTERVAL_MS 20
#define NTP_ORIGINATE_TS_OFFSET 24
#define NTP_TRANSMIT_TS_OFFSET 40
#define NTP_TIMESTAMP_LEN 8
#define NTP_LI_UNSYNC 3
#define NTP_MODE_SERVER 4
#define NTP_MAX_VALID_STRATUM 15

typedef struct {
  ip_addr_t server_addr;
  struct udp_pcb *pcb;
  uint32_t active_gen;
  uint8_t tx_timestamp[NTP_TIMESTAMP_LEN];
  volatile bool dns_done;
  volatile bool dns_ok;
  volatile bool ntp_done;
  volatile bool ntp_ok;
  datetime_t result;
} ntp_state_t;

typedef struct {
  uint32_t gen;
} ntp_dns_token_t;

static ntp_state_t s_ntp_state;
static ntp_dns_token_t s_dns_tokens[NTP_MAX_ATTEMPTS];
static size_t s_dns_token_idx = 0;
static uint32_t s_dns_gen = 0;

static bool ntp_seconds_to_local_datetime(uint32_t ntp_sec, int32_t tz_offset_s,
                                          datetime_t *out_dt) {
  if (out_dt == NULL) {
    return false;
  }

  // Support NTP Era 0 (1900..2036) and Era 1 (2036..2104)
  uint64_t unix_sec = (ntp_sec >= NTP_DELTA)
                          ? (uint64_t)ntp_sec - NTP_DELTA
                          : (uint64_t)ntp_sec + (1ULL << 32) - NTP_DELTA;
  time_t local_epoch = (time_t)unix_sec + tz_offset_s;
  struct tm tm_val;
  if (gmtime_r(&local_epoch, &tm_val) == NULL) {
    return false;
  }

  out_dt->year = (int16_t)(tm_val.tm_year + 1900);
  out_dt->month = (int8_t)(tm_val.tm_mon + 1);
  out_dt->day = (int8_t)tm_val.tm_mday;
  out_dt->dotw = (int8_t)tm_val.tm_wday;
  out_dt->hour = (int8_t)tm_val.tm_hour;
  out_dt->min = (int8_t)tm_val.tm_min;
  out_dt->sec = (int8_t)tm_val.tm_sec;
  return true;
}

static void build_client_timestamp(uint32_t gen,
                                   uint8_t out_ts[NTP_TIMESTAMP_LEN]) {
  uint64_t now_us = time_us_64();
  uint32_t hi = (uint32_t)(now_us >> 32) ^ gen;
  uint32_t lo = (uint32_t)now_us;
  if (hi == 0 && lo == 0) {
    lo = 1;
  }
  out_ts[0] = (uint8_t)(hi >> 24);
  out_ts[1] = (uint8_t)(hi >> 16);
  out_ts[2] = (uint8_t)(hi >> 8);
  out_ts[3] = (uint8_t)hi;
  out_ts[4] = (uint8_t)(lo >> 24);
  out_ts[5] = (uint8_t)(lo >> 16);
  out_ts[6] = (uint8_t)(lo >> 8);
  out_ts[7] = (uint8_t)lo;
}

static bool ntp_parse_response(const struct pbuf *p,
                               const uint8_t expected_orig_ts[NTP_TIMESTAMP_LEN],
                               int32_t tz_offset_s, datetime_t *out_dt) {
  if (p == NULL || expected_orig_ts == NULL || out_dt == NULL ||
      p->tot_len < NTP_MSG_LEN) {
    return false;
  }

  uint8_t b0 = pbuf_get_at(p, 0);
  uint8_t li = (b0 >> 6) & 0x03;
  uint8_t mode = b0 & 0x07;
  uint8_t stratum = pbuf_get_at(p, 1);
  if (li == NTP_LI_UNSYNC || mode != NTP_MODE_SERVER || stratum == 0 ||
      stratum > NTP_MAX_VALID_STRATUM) {
    return false;
  }

  uint8_t orig_ts[NTP_TIMESTAMP_LEN] = {0};
  if (pbuf_copy_partial(p, orig_ts, sizeof(orig_ts), NTP_ORIGINATE_TS_OFFSET) !=
          sizeof(orig_ts) ||
      memcmp(orig_ts, expected_orig_ts, sizeof(orig_ts)) != 0) {
    return false;
  }

  uint8_t sec_buf[4] = {0};
  if (pbuf_copy_partial(p, sec_buf, sizeof(sec_buf), NTP_TRANSMIT_TS_OFFSET) !=
      sizeof(sec_buf)) {
    return false;
  }

  uint32_t ntp_sec = ((uint32_t)sec_buf[0] << 24) |
                     ((uint32_t)sec_buf[1] << 16) |
                     ((uint32_t)sec_buf[2] << 8) | ((uint32_t)sec_buf[3]);
  if (ntp_sec == 0) {
    return false;
  }
  return ntp_seconds_to_local_datetime(ntp_sec, tz_offset_s, out_dt);
}

static bool wait_for_flag_timeout(const volatile bool *flag,
                                  uint32_t timeout_ms) {
  absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  while (!*flag && !time_reached(deadline)) {
    sleep_ms(NTP_POLL_INTERVAL_MS);
  }
  return *flag;
}

static void ntp_dns_cb(const char *hostname, const ip_addr_t *ipaddr,
                       void *arg) {
  (void)hostname;
  const ntp_dns_token_t *token = (const ntp_dns_token_t *)arg;
  if (token == NULL || token->gen != s_ntp_state.active_gen) {
    return;
  }
  if (ipaddr != NULL) {
    s_ntp_state.server_addr = *ipaddr;
    s_ntp_state.dns_ok = true;
  } else {
    s_ntp_state.dns_ok = false;
  }
  s_ntp_state.dns_done = true;
}

static void ntp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port) {
  (void)pcb;
  ntp_state_t *state = (ntp_state_t *)arg;

  if (p == NULL) {
    return;
  }

  if (state != NULL && addr != NULL && !state->ntp_done &&
      ip_addr_cmp(addr, &state->server_addr) && port == NTP_PORT) {
    datetime_t parsed = {0};
    if (ntp_parse_response(p, state->tx_timestamp, TIMEZONE_OFFSET_S,
                           &parsed)) {
      state->result = parsed;
      state->ntp_ok = true;
      state->ntp_done = true;
    }
  }

  pbuf_free(p);
}

static bool ntp_resolve_server(ntp_state_t *state) {
  cyw43_arch_lwip_begin();
  s_dns_gen++;
  if (s_dns_gen == 0) {
    s_dns_gen = 1;
  }
  state->active_gen = s_dns_gen;

  ntp_dns_token_t *token = &s_dns_tokens[s_dns_token_idx];
  s_dns_token_idx = (s_dns_token_idx + 1) % NTP_MAX_ATTEMPTS;
  token->gen = s_dns_gen;

  err_t err =
      dns_gethostbyname(NTP_SERVER, &state->server_addr, ntp_dns_cb, token);
  if (err == ERR_OK) {
    state->dns_done = true;
    state->dns_ok = true;
  }
  cyw43_arch_lwip_end();

  if (err != ERR_OK && err != ERR_INPROGRESS) {
    return false;
  }

  (void)wait_for_flag_timeout(&state->dns_done, NTP_TIMEOUT_MS);

  cyw43_arch_lwip_begin();
  bool ok = state->dns_done && state->dns_ok;
  state->active_gen = 0;
  cyw43_arch_lwip_end();
  return ok;
}

static bool ntp_exchange_packet(ntp_state_t *state, datetime_t *out_dt) {
  bool ok = false;

  cyw43_arch_lwip_begin();
  state->pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!state->pcb) {
    cyw43_arch_lwip_end();
    return false;
  }
  udp_recv(state->pcb, ntp_recv_cb, state);

  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
  if (!p) {
    udp_remove(state->pcb);
    state->pcb = NULL;
    cyw43_arch_lwip_end();
    return false;
  }

  uint8_t *req = (uint8_t *)p->payload;
  memset(req, 0, NTP_MSG_LEN);
  req[0] = 0x1B; // LI=0, VN=3, Mode=3 (Client)
  build_client_timestamp(s_dns_gen, state->tx_timestamp);
  memcpy(req + NTP_TRANSMIT_TS_OFFSET, state->tx_timestamp,
         sizeof(state->tx_timestamp));

  err_t err = udp_sendto(state->pcb, p, &state->server_addr, NTP_PORT);
  pbuf_free(p);
  cyw43_arch_lwip_end();

  if (err == ERR_OK) {
    (void)wait_for_flag_timeout(&state->ntp_done, NTP_TIMEOUT_MS);
  }

  cyw43_arch_lwip_begin();
  if (state->pcb) {
    udp_remove(state->pcb);
    state->pcb = NULL;
  }
  if (state->ntp_done && state->ntp_ok) {
    *out_dt = state->result;
    ok = true;
  }
  cyw43_arch_lwip_end();

  return ok;
}

static bool ntp_query_once(datetime_t *out_dt) {
  if (out_dt == NULL) {
    return false;
  }

  ntp_state_t *state = &s_ntp_state;
  cyw43_arch_lwip_begin();
  memset(state, 0, sizeof(*state));
  cyw43_arch_lwip_end();

  if (!ntp_resolve_server(state)) {
    return false;
  }
  return ntp_exchange_packet(state, out_dt);
}

bool ntp_sync_datetime(datetime_t *out_dt) {
  if (out_dt == NULL) {
    return false;
  }

  if (cyw43_arch_init()) {
    return false;
  }

  cyw43_arch_enable_sta_mode();

  int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                              WIFI_AUTH,
                                              WIFI_CONNECT_TIMEOUT_MS);
  if (rc != 0) {
    cyw43_arch_deinit();
    return false;
  }

  bool ok = false;
  for (int attempt = 0; attempt < NTP_MAX_ATTEMPTS; attempt++) {
    if (ntp_query_once(out_dt)) {
      ok = true;
      break;
    }
    if (attempt + 1 < NTP_MAX_ATTEMPTS) {
      sleep_ms(NTP_RETRY_DELAY_MS);
    }
  }

  cyw43_arch_deinit();
  return ok;
}
