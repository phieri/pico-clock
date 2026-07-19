#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

#define NTP_SERVER_IP "216.239.35.0"
#define NTP_SERVER_PORT 123
#define NTP_SYNC_INTERVAL_MS (30u * 60u * 1000u)
#define WIFI_SSID_DEFAULT ""
#define WIFI_PASSWORD_DEFAULT ""
#define WIFI_CONNECT_TIMEOUT_MS 30000u

typedef struct {
    bool has_time;
    uint64_t boot_epoch_seconds;
    uint32_t boot_ms;
    uint32_t last_sync_ms;
    int32_t drift_ms;
} clock_state_t;

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

static void clock_init(clock_state_t *state) {
    memset(state, 0, sizeof(*state));
}

static uint32_t clock_now_ms(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static bool wifi_connect(void) {
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return false;
    }

    if (cyw43_arch_enable_sta_mode()) {
        printf("sta mode failed\n");
        cyw43_arch_deinit();
        return false;
    }

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID_DEFAULT, WIFI_PASSWORD_DEFAULT,
                                           CYW43_AUTH_OPEN, WIFI_CONNECT_TIMEOUT_MS)) {
        printf("wifi connect failed\n");
        cyw43_arch_deinit();
        return false;
    }

    printf("wifi connected\n");
    return true;
}

static bool ntp_sync(clock_state_t *state) {
    if (!cyw43_arch_wifi_is_connected()) {
        return false;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printf("socket create failed\n");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NTP_SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(NTP_SERVER_IP);

    uint8_t packet[48] = {0};
    packet[0] = 0x1b;

    cyw43_arch_lwip_begin();
    int sent = sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
    cyw43_arch_lwip_end();
    if (sent < 0) {
        printf("ntp send failed\n");
        close(sock);
        return false;
    }

    cyw43_arch_lwip_begin();
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int received = recvfrom(sock, packet, sizeof(packet), 0, NULL, NULL);
    cyw43_arch_lwip_end();
    close(sock);

    if (received < 0) {
        printf("ntp receive failed\n");
        return false;
    }

    ntp_packet_t *ntp = (ntp_packet_t *)packet;
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

static uint64_t clock_current_epoch_seconds(clock_state_t *state, uint32_t now) {
    if (!state->has_time) {
        return 0;
    }

    uint32_t elapsed_ms = now - state->boot_ms;
    int64_t elapsed_seconds = (int64_t)(elapsed_ms / 1000u);
    int64_t adjusted_seconds = (int64_t)state->boot_epoch_seconds + elapsed_seconds + (state->drift_ms / 1000);
    return (uint64_t)adjusted_seconds;
}

static void clock_format_hms(uint64_t epoch_seconds, char *buffer, size_t size) {
    uint32_t seconds = (uint32_t)(epoch_seconds % 86400u);
    uint32_t hours = seconds / 3600u;
    uint32_t minutes = (seconds % 3600u) / 60u;
    uint32_t secs = seconds % 60u;
    snprintf(buffer, size, "%02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes, (unsigned long)secs);
}

int main(void) {
    stdio_init_all();
    clock_state_t clock;
    clock_init(&clock);

    while (true) {
        if (!cyw43_arch_wifi_is_connected()) {
            if (!wifi_connect()) {
                sleep_ms(5000);
                continue;
            }
        }

        if (!clock.has_time || (clock_now_ms() - clock.last_sync_ms) > NTP_SYNC_INTERVAL_MS) {
            ntp_sync(&clock);
        }

        uint32_t now = clock_now_ms();
        uint64_t epoch = clock_current_epoch_seconds(&clock, now);
        char time_buffer[16];
        clock_format_hms(epoch, time_buffer, sizeof(time_buffer));
        printf("\r%s drift=%ldms", time_buffer, (long)clock.drift_ms);
        fflush(stdout);
        sleep_ms(1000);
    }

    return 0;
}
