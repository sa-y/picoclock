#include "ntp_client.h"
#include "power_mgmt.h"
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
#define NTP_TIMEOUT_MS 5000
#define NTP_MAX_ATTEMPTS 3

typedef struct {
  ip_addr_t server_addr;
  struct udp_pcb *pcb;
  uint32_t active_gen;
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
static uint32_t s_dns_gen = 0;

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

  if (addr != NULL && ip_addr_cmp(addr, &state->server_addr) &&
      port == NTP_PORT && p->tot_len == NTP_MSG_LEN) {
    uint8_t mode = pbuf_get_at(p, 0) & 0x07;
    uint8_t stratum = pbuf_get_at(p, 1);
    if (mode == 4 && stratum != 0) {
      uint8_t sec_buf[4] = {0};
      pbuf_copy_partial(p, sec_buf, sizeof(sec_buf), 40);
      uint32_t ntp_sec = ((uint32_t)sec_buf[0] << 24) |
                         ((uint32_t)sec_buf[1] << 16) |
                         ((uint32_t)sec_buf[2] << 8) | ((uint32_t)sec_buf[3]);

      // Support NTP Era 0 (1900..2036) and Era 1 (2036..2104)
      uint64_t unix_sec = (ntp_sec >= NTP_DELTA)
                              ? (uint64_t)ntp_sec - NTP_DELTA
                              : (uint64_t)ntp_sec + (1ULL << 32) - NTP_DELTA;
      time_t local_epoch = (time_t)unix_sec + TIMEZONE_OFFSET_S;
      struct tm tm_val;
      if (gmtime_r(&local_epoch, &tm_val) != NULL) {
        state->result.year = (int16_t)(tm_val.tm_year + 1900);
        state->result.month = (int8_t)(tm_val.tm_mon + 1);
        state->result.day = (int8_t)tm_val.tm_mday;
        state->result.dotw = (int8_t)tm_val.tm_wday;
        state->result.hour = (int8_t)tm_val.tm_hour;
        state->result.min = (int8_t)tm_val.tm_min;
        state->result.sec = (int8_t)tm_val.tm_sec;
        state->ntp_ok = true;
        state->ntp_done = true;
      }
    }
  }

  pbuf_free(p);
}

static bool ntp_query_once(int attempt) {
  ntp_state_t *state = &s_ntp_state;
  bool ok = false;

  cyw43_arch_lwip_begin();
  memset(state, 0, sizeof(*state));
  s_dns_gen++;
  if (s_dns_gen == 0) {
    s_dns_gen = 1;
  }
  state->active_gen = s_dns_gen;
  s_dns_tokens[attempt].gen = s_dns_gen;

  state->pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!state->pcb) {
    cyw43_arch_lwip_end();
    goto cleanup;
  }
  udp_recv(state->pcb, ntp_recv_cb, state);

  err_t err = dns_gethostbyname(NTP_SERVER, &state->server_addr, ntp_dns_cb,
                                &s_dns_tokens[attempt]);
  if (err == ERR_OK) {
    state->dns_done = true;
    state->dns_ok = true;
  } else if (err != ERR_INPROGRESS) {
    cyw43_arch_lwip_end();
    goto cleanup;
  }
  cyw43_arch_lwip_end();

  // Wait for DNS resolution
  absolute_time_t deadline = make_timeout_time_ms(NTP_TIMEOUT_MS);
  while (!state->dns_done && !time_reached(deadline)) {
    sleep_ms(20);
  }

  if (!state->dns_done || !state->dns_ok) {
    goto cleanup;
  }

  // Send NTP request packet
  cyw43_arch_lwip_begin();
  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
  if (!p) {
    cyw43_arch_lwip_end();
    goto cleanup;
  }
  uint8_t *req = (uint8_t *)p->payload;
  memset(req, 0, NTP_MSG_LEN);
  req[0] = 0x1B; // LI=0, VN=3, Mode=3 (Client)

  err = udp_sendto(state->pcb, p, &state->server_addr, NTP_PORT);
  pbuf_free(p);
  cyw43_arch_lwip_end();

  if (err != ERR_OK) {
    goto cleanup;
  }

  // Wait for NTP response
  deadline = make_timeout_time_ms(NTP_TIMEOUT_MS);
  while (!state->ntp_done && !time_reached(deadline)) {
    sleep_ms(20);
  }

  ok = state->ntp_done && state->ntp_ok;

cleanup:
  cyw43_arch_lwip_begin();
  state->active_gen = 0;
  if (state->pcb) {
    udp_remove(state->pcb);
    state->pcb = NULL;
  }
  cyw43_arch_lwip_end();

  return ok;
}

bool ntp_sync_datetime(datetime_t *out_dt) {
  power_set_clock_normal();

  if (cyw43_arch_init()) {
    power_set_clock_low();
    return false;
  }

  cyw43_arch_enable_sta_mode();

  int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                              WIFI_AUTH, 20000);
  if (rc != 0) {
    cyw43_arch_deinit();
    power_set_clock_low();
    return false;
  }

  bool ok = false;
  for (int attempt = 0; attempt < NTP_MAX_ATTEMPTS; attempt++) {
    if (ntp_query_once(attempt)) {
      *out_dt = s_ntp_state.result;
      ok = true;
      break;
    }
    sleep_ms(500);
  }

  cyw43_arch_deinit();
  power_set_clock_low();
  return ok;
}
