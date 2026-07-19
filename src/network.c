#include "network.h"

#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#define NTP_SERVER_IP "216.239.35.0"
#define NTP_SERVER_PORT 123
#ifndef WIFI_SSID_DEFAULT
#define WIFI_SSID_DEFAULT ""
#endif
#ifndef WIFI_PASSWORD_DEFAULT
#define WIFI_PASSWORD_DEFAULT ""
#endif
#define WIFI_CONNECT_TIMEOUT_MS 30000u

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

static void ntp_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    ntp_receive_state_t *state = (ntp_receive_state_t *)arg;
    if (!state || !p) {
        return;
    }

    state->length = pbuf_copy_partial(p, state->payload, sizeof(state->payload), 0);
    state->received = true;
    pbuf_free(p);
}

bool wifi_connect(void) {
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID_DEFAULT, WIFI_PASSWORD_DEFAULT,
                                            CYW43_AUTH_OPEN, WIFI_CONNECT_TIMEOUT_MS)) {
        printf("wifi connect failed\n");
        cyw43_arch_deinit();
        return false;
    }

    printf("wifi connected\n");
    return true;
}

bool ntp_sync(clock_state_t *state) {
    ntp_receive_state_t receive_state = {0};
    struct udp_pcb *pcb = udp_new();
    if (!pcb) {
        printf("udp pcb create failed\n");
        return false;
    }

    if (udp_bind(pcb, IP_ADDR_ANY, 0) != ERR_OK) {
        printf("udp bind failed\n");
        udp_remove(pcb);
        return false;
    }

    udp_recv(pcb, ntp_udp_recv, &receive_state);

    uint8_t packet[48] = {0};
    packet[0] = 0x1b;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet), PBUF_RAM);
    if (!p) {
        printf("pbuf alloc failed\n");
        udp_remove(pcb);
        return false;
    }

    if (pbuf_take(p, packet, sizeof(packet)) != ERR_OK) {
        printf("pbuf take failed\n");
        pbuf_free(p);
        udp_remove(pcb);
        return false;
    }

    ip4_addr_t server_addr;
    if (!ip4addr_aton(NTP_SERVER_IP, &server_addr)) {
        printf("ntp server address invalid\n");
        pbuf_free(p);
        udp_remove(pcb);
        return false;
    }

    cyw43_arch_lwip_begin();
    err_t err = udp_sendto(pcb, p, &server_addr, NTP_SERVER_PORT);
    cyw43_arch_lwip_end();
    if (err != ERR_OK) {
        printf("ntp send failed\n");
        pbuf_free(p);
        udp_remove(pcb);
        return false;
    }

    uint32_t deadline = clock_now_ms() + 5000u;
    while (!receive_state.received && (clock_now_ms() < deadline)) {
        cyw43_arch_poll();
        sleep_ms(10);
    }

    udp_remove(pcb);

    if (!receive_state.received) {
        printf("ntp receive failed\n");
        return false;
    }

    ntp_packet_t *ntp = (ntp_packet_t *)receive_state.payload;
    uint32_t ntp_seconds = ntohl(ntp->tx_tm_s);
    uint32_t unix_seconds = ntp_seconds - 2208988800UL;
    uint32_t now = clock_now_ms();

    uint32_t previous_epoch = state->boot_epoch_seconds + (now - state->boot_ms) / 1000;
    state->boot_epoch_seconds = unix_seconds;
    state->boot_ms = now;
    state->last_sync_ms = now;
    state->has_time = true;

    if (state->last_sync_ms != 0) {
        uint32_t expected_epoch = previous_epoch;
        int32_t drift_ms = (int32_t)((uint64_t)unix_seconds * 1000ULL - (uint64_t)expected_epoch * 1000ULL);
        state->drift_ms = (state->drift_ms + drift_ms) / 2;
    }

    printf("ntp synced: %lu\n", (unsigned long)unix_seconds);
    return true;
}
