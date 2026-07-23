#include "network.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "cpr_compat.h"
#include "cyw43.h"

#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#define NTP_SERVER_PORT 123u
#define NTP_SERVER_FALLBACK_IPV6 "2001:4860:4860::8888"
#define NTP_SERVER_FALLBACK_IPV4 "216.239.35.0"
#define WIFI_CONNECT_TIMEOUT_MS 30000u
#define CAPTIVE_PORTAL_URL "http://networkcheck.kde.org/"
#define CAPTIVE_PORTAL_FALLBACK_URL_1 "http://detectportal.firefox.com/success.txt"
#define CAPTIVE_PORTAL_FALLBACK_URL_2 "http://captive.apple.com/hotspot-detect.html"
#define CAPTIVE_PORTAL_FALLBACK_URL_3 "http://clients3.google.com/generate_204"
#define NTP_TIMEOUT_MS 5000u
#define NTP_MAX_SERVERS 4u

typedef struct __attribute__((packed)) {
    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;
    uint32_t ref_tm_s;
    uint32_t ref_tm_f;
    uint32_t orig_tm_s;
    uint32_t orig_tm_f;
    uint32_t rx_tm_s;
    uint32_t rx_tm_f;
    uint32_t tx_tm_s;
    uint32_t tx_tm_f;
} ntp_packet_t;

typedef struct {
    bool received;
    uint8_t payload[48];
    uint16_t length;
} ntp_receive_state_t;

typedef struct {
    bool valid;
    int64_t offset_ms;
    int64_t latency_ms;
    uint64_t server_epoch_seconds;
} ntp_sample_t;

typedef struct {
    bool scan_complete;
    size_t count;
    char ssids[8][33];
} wifi_scan_state_t;

static void ntp_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    ntp_receive_state_t *state = (ntp_receive_state_t *)arg;
    if (!state || !p) {
        return;
    }

    state->length = pbuf_copy_partial(p, state->payload, sizeof(state->payload), 0);
    state->received = true;
    pbuf_free(p);
}

static int wifi_scan_result_cb(void *env, const cyw43_ev_scan_result_t *result) {
    wifi_scan_state_t *scan_state = (wifi_scan_state_t *)env;
    if (scan_state == NULL) {
        return 0;
    }

    if (result == NULL) {
        scan_state->scan_complete = true;
        return 0;
    }

    if (result->ssid_len == 0u || result->auth_mode != CYW43_AUTH_OPEN) {
        return 0;
    }

    if (scan_state->count >= (sizeof(scan_state->ssids) / sizeof(scan_state->ssids[0]))) {
        return 0;
    }

    size_t ssid_len = result->ssid_len;
    if (ssid_len >= sizeof(scan_state->ssids[0])) {
        ssid_len = sizeof(scan_state->ssids[0]) - 1u;
    }

    for (size_t index = 0; index < scan_state->count; ++index) {
        if (strncmp(scan_state->ssids[index], (const char *)result->ssid, ssid_len) == 0 && scan_state->ssids[index][ssid_len] == '\0') {
            return 0;
        }
    }

    memcpy(scan_state->ssids[scan_state->count], result->ssid, ssid_len);
    scan_state->ssids[scan_state->count][ssid_len] = '\0';
    ++scan_state->count;
    return 0;
}

static bool response_body_indicates_success(const cpr_response_t *response) {
    if (response == NULL || response->text == NULL || response->text_length == 0u) {
        return false;
    }

    char normalized[64];
    size_t normalized_length = response->text_length;
    if (normalized_length >= sizeof(normalized)) {
        normalized_length = sizeof(normalized) - 1u;
    }

    for (size_t index = 0; index < normalized_length; ++index) {
        normalized[index] = (char)tolower((unsigned char)response->text[index]);
    }
    normalized[normalized_length] = '\0';

    return strstr(normalized, "ok") != NULL || strstr(normalized, "success") != NULL || strstr(normalized, "generate_204") != NULL;
}

static bool captive_portal_check(void) {
    static const char *const probe_urls[] = {
        CAPTIVE_PORTAL_URL,
        CAPTIVE_PORTAL_FALLBACK_URL_1,
        CAPTIVE_PORTAL_FALLBACK_URL_2,
        CAPTIVE_PORTAL_FALLBACK_URL_3,
    };

    for (size_t index = 0; index < (sizeof(probe_urls) / sizeof(probe_urls[0])); ++index) {
        cpr_response_t response = cpr_get(probe_urls[index]);
        bool success = (response.status_code == 204u) || (cpr_is_successful(&response) && response_body_indicates_success(&response));
        if (success) {
            printf("captive portal probe succeeded with HTTP %ld via %s\n", response.status_code, probe_urls[index]);
            cpr_response_free(&response);
            return true;
        }

        printf("captive portal probe failed for %s with HTTP %ld\n", probe_urls[index], response.status_code);
        cpr_response_free(&response);
    }

    return false;
}

static void trim_whitespace(char *text) {
    if (text == NULL) {
        return;
    }

    char *start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }

    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, (size_t)(end - start + 1u));
    }
}

static size_t collect_server_names(const pico_config_t *config, char servers[NTP_MAX_SERVERS][64]) {
    size_t count = 0;
    const char *source = (config != NULL && config->ntp_server_set && config->ntp_server[0] != '\0') ? config->ntp_server : NTP_SERVER_FALLBACK_IPV6;
    char work[128];
    memset(work, 0, sizeof(work));
    strncpy(work, source, sizeof(work) - 1u);

    char *token = strtok(work, ",");
    while (token != NULL && count < NTP_MAX_SERVERS) {
        trim_whitespace(token);
        if (token[0] != '\0') {
            memset(servers[count], 0, sizeof(servers[count]));
            strncpy(servers[count], token, sizeof(servers[count]) - 1u);
            ++count;
        }
        token = strtok(NULL, ",");
    }

    if (count == 0) {
        memset(servers[0], 0, sizeof(servers[0]));
        strncpy(servers[0], NTP_SERVER_FALLBACK_IPV6, sizeof(servers[0]) - 1u);
        memset(servers[1], 0, sizeof(servers[1]));
        strncpy(servers[1], NTP_SERVER_FALLBACK_IPV4, sizeof(servers[1]) - 1u);
        count = 2u;
    }

    return count;
}

static bool resolve_server_address(const char *host, ip_addr_t *server_addr) {
    if (host == NULL || server_addr == NULL) {
        return false;
    }

    memset(server_addr, 0, sizeof(*server_addr));
    if (host[0] != '\0' && ipaddr_aton(host, server_addr)) {
        return true;
    }

    return false;
}

static uint64_t ntp_timestamp_to_epoch_ms(uint32_t seconds, uint32_t fraction) {
    uint64_t epoch_seconds = (uint64_t)seconds - 2208988800ULL;
    uint64_t fractional_ms = ((uint64_t)fraction * 1000ULL) >> 32u;
    return (epoch_seconds * 1000ULL) + fractional_ms;
}

static uint8_t ntp_packet_version(const ntp_packet_t *packet) {
    if (packet == NULL) {
        return 0u;
    }
    return (uint8_t)((packet->li_vn_mode >> 3u) & 0x7u);
}

static uint8_t ntp_packet_mode(const ntp_packet_t *packet) {
    if (packet == NULL) {
        return 0u;
    }
    return (uint8_t)(packet->li_vn_mode & 0x7u);
}

static uint8_t ntp_packet_leap_indicator(const ntp_packet_t *packet) {
    if (packet == NULL) {
        return 0u;
    }
    return (uint8_t)((packet->li_vn_mode >> 6u) & 0x3u);
}

static bool ntp_packet_is_kiss_o_death(const ntp_packet_t *packet) {
    return packet != NULL && ntp_packet_mode(packet) == 5u && packet->stratum == 0u;
}

static bool ntp_packet_is_valid_response(const ntp_packet_t *packet) {
    if (packet == NULL) {
        return false;
    }
    if (ntp_packet_version(packet) != 4u || ntp_packet_mode(packet) != 4u) {
        return false;
    }
    if (ntp_packet_leap_indicator(packet) == 3u) {
        return false;
    }
    return packet->stratum != 0u;
}

static void ntp_format_reference_id(const ntp_packet_t *packet, char *buffer, size_t size) {
    if (buffer == NULL || size == 0u) {
        return;
    }

    buffer[0] = '\0';
    if (packet == NULL) {
        return;
    }

    const uint8_t *raw_ref_id = (const uint8_t *)&packet->ref_id;
    size_t index = 0;
    while (index + 1u < size) {
        unsigned char ch = raw_ref_id[index];
        buffer[index] = isprint(ch) ? (char)ch : '?';
        if (ch == '\0') {
            break;
        }
        ++index;
    }
    buffer[(index < size) ? index : (size - 1u)] = '\0';
}

static void ntp_epoch_ms_to_timestamp(uint64_t epoch_ms, uint32_t *seconds, uint32_t *fraction) {
    if (seconds == NULL || fraction == NULL) {
        return;
    }

    uint64_t epoch_seconds = epoch_ms / 1000ULL;
    uint32_t fractional_ms = (uint32_t)(epoch_ms % 1000ULL);
    *seconds = (uint32_t)(epoch_seconds + 2208988800ULL);
    uint64_t fraction_value = ((uint64_t)fractional_ms << 32u) / 1000ULL;
    *fraction = (uint32_t)fraction_value;
}

static bool ntp_query_server(clock_state_t *state, const char *server, ntp_sample_t *sample) {
    if (sample == NULL) {
        return false;
    }

    memset(sample, 0, sizeof(*sample));

    ip_addr_t server_addr;
    if (!resolve_server_address(server, &server_addr)) {
        return false;
    }

    ntp_receive_state_t receive_state = {0};
    struct udp_pcb *pcb = udp_new_ip_type(IP_IS_V6(&server_addr) ? IPADDR_TYPE_V6 : IPADDR_TYPE_V4);
    if (!pcb) {
        printf("udp pcb create failed for %s\n", server);
        return false;
    }

    if (udp_bind(pcb, IP_ADDR_ANY, 0) != ERR_OK) {
        printf("udp bind failed for %s\n", server);
        udp_remove(pcb);
        return false;
    }

    udp_recv(pcb, ntp_udp_recv, &receive_state);

    uint8_t packet[48] = {0};
    packet[0] = 0x1b;

    uint32_t send_ms = clock_now_ms();
    uint64_t local_tx_epoch_ms = 0;
    uint32_t client_tx_seconds = 0;
    uint32_t client_tx_fraction = 0;
    if (state != NULL && state->has_time) {
        local_tx_epoch_ms = (uint64_t)clock_current_epoch_seconds(state, send_ms) * 1000ULL;
    }
    ntp_epoch_ms_to_timestamp(local_tx_epoch_ms, &client_tx_seconds, &client_tx_fraction);
    ((uint32_t *)packet)[10] = htonl(client_tx_seconds);
    ((uint32_t *)packet)[11] = htonl(client_tx_fraction);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet), PBUF_RAM);
    if (!p) {
        printf("pbuf alloc failed for %s\n", server);
        udp_remove(pcb);
        return false;
    }

    if (pbuf_take(p, packet, sizeof(packet)) != ERR_OK) {
        printf("pbuf take failed for %s\n", server);
        pbuf_free(p);
        udp_remove(pcb);
        return false;
    }

    cyw43_arch_lwip_begin();
    err_t err = udp_sendto(pcb, p, &server_addr, NTP_SERVER_PORT);
    cyw43_arch_lwip_end();
    if (err != ERR_OK) {
        printf("ntp send failed for %s\n", server);
        pbuf_free(p);
        udp_remove(pcb);
        return false;
    }

    uint32_t deadline = send_ms + NTP_TIMEOUT_MS;
    while (!receive_state.received && (clock_now_ms() < deadline)) {
        sleep_ms(10);
    }

    uint32_t receive_ms = clock_now_ms();
    udp_remove(pcb);

    if (!receive_state.received) {
        printf("ntp receive failed for %s\n", server);
        return false;
    }

    if (receive_state.length < sizeof(ntp_packet_t)) {
        printf("ntp receive truncated for %s\n", server);
        return false;
    }

    ntp_packet_t *ntp = (ntp_packet_t *)receive_state.payload;
    if (ntp_packet_is_kiss_o_death(ntp)) {
        char reference_id[5] = {0};
        ntp_format_reference_id(ntp, reference_id, sizeof(reference_id));
        printf("ntp server %s returned kiss-o'-death (%s)\n", server, reference_id);
        return false;
    }

    if (!ntp_packet_is_valid_response(ntp)) {
        printf("ntp server %s returned invalid response (mode=%u version=%u leap=%u stratum=%u)\n",
               server,
               (unsigned)ntp_packet_mode(ntp),
               (unsigned)ntp_packet_version(ntp),
               (unsigned)ntp_packet_leap_indicator(ntp),
               (unsigned)ntp->stratum);
        return false;
    }

    uint32_t response_orig_seconds = ntohl(ntp->orig_tm_s);
    uint32_t response_orig_fraction = ntohl(ntp->orig_tm_f);
    if (response_orig_seconds != client_tx_seconds || response_orig_fraction != client_tx_fraction) {
        printf("ntp server %s returned mismatched originate timestamp\n", server);
        return false;
    }

    uint64_t server_tx_epoch_ms = ntp_timestamp_to_epoch_ms(ntohl(ntp->tx_tm_s), ntohl(ntp->tx_tm_f));
    uint64_t server_rx_epoch_ms = ntp_timestamp_to_epoch_ms(ntohl(ntp->rx_tm_s), ntohl(ntp->rx_tm_f));
    uint64_t response_orig_epoch_ms = ntp_timestamp_to_epoch_ms(response_orig_seconds, response_orig_fraction);

    if (state != NULL && state->has_time) {
        uint64_t local_recv_epoch_ms = (uint64_t)clock_current_epoch_seconds(state, receive_ms) * 1000ULL;
        sample->offset_ms = ((int64_t)server_rx_epoch_ms - (int64_t)response_orig_epoch_ms + (int64_t)server_tx_epoch_ms - (int64_t)local_recv_epoch_ms) / 2LL;
        sample->latency_ms = (int64_t)(receive_ms - send_ms) - ((int64_t)server_tx_epoch_ms - (int64_t)server_rx_epoch_ms);
    } else {
        sample->offset_ms = 0;
        sample->latency_ms = (int64_t)(receive_ms - send_ms);
    }

    sample->server_epoch_seconds = server_tx_epoch_ms / 1000ULL;
    sample->valid = true;
    return true;
}

static void network_apply_hostname(const pico_config_t *config) {
    struct netif *netif = netif_default;
    if (netif == NULL) {
        return;
    }

    const char *hostname = config != NULL && config->hostname[0] != '\0' ? config->hostname : PICO_DEFAULT_HOSTNAME;
    cyw43_arch_lwip_begin();
    netif_set_hostname(netif, hostname);
    cyw43_arch_lwip_end();
}

static bool wifi_initialise_network_stack(const pico_config_t *config) {
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();
    network_apply_hostname(config);
    return true;
}

static bool wifi_connect_with_credentials(const char *ssid, const char *password) {
    const bool use_open_wifi_probe = (password[0] == '\0');
    uint32_t auth_type = (password[0] != '\0') ? CYW43_AUTH_WPA3_WPA2_AES_PSK : CYW43_AUTH_OPEN;
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, auth_type, WIFI_CONNECT_TIMEOUT_MS)) {
        printf("wifi connect failed\n");
        cyw43_arch_deinit();
        return false;
    }

    printf("wifi connected\n");
    if (use_open_wifi_probe && !captive_portal_check()) {
        printf("captive portal probe failed; delaying reconnect\n");
        cyw43_arch_deinit();
        return false;
    }
    return true;
}

static bool wifi_connect_via_scan(void) {
    wifi_scan_state_t scan_state = {0};
    int scan_err = cyw43_wifi_scan(&cyw43_state, NULL, &scan_state, wifi_scan_result_cb);
    if (scan_err != 0) {
        printf("wifi scan failed: %d\n", scan_err);
        cyw43_arch_deinit();
        return false;
    }

    uint32_t scan_deadline = clock_now_ms() + WIFI_CONNECT_TIMEOUT_MS;
    while (!scan_state.scan_complete && (clock_now_ms() < scan_deadline)) {
        sleep_ms(50);
    }

    if (!scan_state.scan_complete) {
        printf("wifi scan timed out\n");
        cyw43_arch_deinit();
        return false;
    }

    for (size_t index = 0; index < scan_state.count; ++index) {
        if (cyw43_arch_wifi_connect_timeout_ms(scan_state.ssids[index], "", CYW43_AUTH_OPEN, WIFI_CONNECT_TIMEOUT_MS)) {
            continue;
        }

        printf("wifi connected to %s\n", scan_state.ssids[index]);
        if (captive_portal_check()) {
            return true;
        }

        printf("captive portal probe failed for %s; trying next network\n", scan_state.ssids[index]);
    }

    cyw43_arch_deinit();
    return false;
}

bool wifi_connect(const pico_config_t *config) {
    if (config == NULL) {
        return false;
    }

    const bool configured = config->wifi_configured && config->wifi_ssid[0] != '\0';
    const char *ssid = configured ? config->wifi_ssid : "";
    const char *password = configured ? config->wifi_password : "";

    if (!wifi_initialise_network_stack(config)) {
        return false;
    }

    if (configured) {
        return wifi_connect_with_credentials(ssid, password);
    }

    return wifi_connect_via_scan();
}

bool ntp_sync(clock_state_t *state, const pico_config_t *config) {
    if (state == NULL) {
        return false;
    }

    char servers[NTP_MAX_SERVERS][64];
    memset(servers, 0, sizeof(servers));
    size_t server_count = collect_server_names(config, servers);

    ntp_sample_t best_sample;
    memset(&best_sample, 0, sizeof(best_sample));
    bool found_sample = false;

    for (size_t index = 0; index < server_count; ++index) {
        ntp_sample_t sample;
        if (!ntp_query_server(state, servers[index], &sample)) {
            continue;
        }

        if (!sample.valid) {
            continue;
        }

        if (!found_sample || llabs(sample.latency_ms) < llabs(best_sample.latency_ms)) {
            best_sample = sample;
            found_sample = true;
        }
    }

    if (!found_sample) {
        printf("ntp sync failed; no responsive server\n");
        return false;
    }

    uint32_t now = clock_now_ms();
    uint32_t previous_boot_ms = state->boot_ms;
    int64_t elapsed_seconds = (int64_t)((now - previous_boot_ms) / 1000u);
    int32_t smoothed_drift_ms = (int32_t)((((int64_t)state->drift_ms * 3LL) + best_sample.offset_ms) / 4LL);

    state->boot_ms = now;
    state->boot_epoch_seconds = (uint64_t)((int64_t)best_sample.server_epoch_seconds - elapsed_seconds - (smoothed_drift_ms / 1000LL));
    state->last_sync_ms = now;
    state->drift_ms = smoothed_drift_ms;
    state->has_time = true;

    printf("ntp synced from %s: %lu (offset=%lldms latency=%lldms)\n",
           servers[0],
           (unsigned long)best_sample.server_epoch_seconds,
           (long long)best_sample.offset_ms,
           (long long)best_sample.latency_ms);
    return true;
}
