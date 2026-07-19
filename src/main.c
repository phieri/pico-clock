#include <stdbool.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "clock.h"
#include "display.h"
#include "network.h"

#define NTP_SYNC_INTERVAL_MS (30u * 60u * 1000u)

int main(void) {
    stdio_init_all();
    clock_state_t clock;
    display_framebuffer_t display;
    bool wifi_ready = false;
    clock_init(&clock);
    display_init(&display);

    while (true) {
        if (!wifi_ready) {
            wifi_ready = wifi_connect();
            if (!wifi_ready) {
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
        display_draw_time(&display, time_buffer, (long)clock.drift_ms);
        sleep_ms(1000);
    }

    return 0;
}
