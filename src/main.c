#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "clock.h"
#include "config.h"
#include "display.h"
#include "network.h"

#define NTP_SYNC_INTERVAL_MS (30u * 60u * 1000u)
#define SERIAL_BUFFER_SIZE 160u

int main(void) {
    stdio_init_all();
    clock_state_t clock;
    display_framebuffer_t display;
    pico_config_t config;
    bool wifi_ready = false;
    bool config_dirty = false;
    char serial_buffer[SERIAL_BUFFER_SIZE];
    size_t serial_length = 0;

    clock_init(&clock);
    display_init(&display);
    config_init(&config);
    if (!config_load(&config)) {
        printf("config load failed; using defaults\n");
    }

    while (true) {
        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT) {
            if (ch == '\r' || ch == '\n') {
                if (serial_length > 0) {
                    serial_buffer[serial_length] = '\0';
                    char response[96];
                    if (config_handle_command(&config, serial_buffer, response, sizeof(response))) {
                        if (!config_save(&config)) {
                            printf("config save failed\n");
                        }
                        printf("%s\n", response);
                        config_dirty = true;
                    } else {
                        printf("%s\n", response);
                    }
                    serial_length = 0;
                }
            } else if (serial_length < (SERIAL_BUFFER_SIZE - 1u)) {
                serial_buffer[serial_length++] = (char)ch;
            }
        }

        if (config_dirty) {
            config_dirty = false;
            wifi_ready = false;
        }

        if (!wifi_ready) {
            wifi_ready = wifi_connect(&config);
            if (!wifi_ready) {
                sleep_ms(5000);
                continue;
            }
        }

        if (!clock.has_time || (clock_now_ms() - clock.last_sync_ms) > NTP_SYNC_INTERVAL_MS) {
            ntp_sync(&clock, &config);
        }

        uint32_t now = clock_now_ms();
        uint64_t epoch = clock_current_epoch_seconds(&clock, now);
        char time_buffer[16];
        int32_t timezone_offset_seconds = config.timezone_set ? config.timezone_offset_seconds : 0;
        clock_format_hms(epoch, timezone_offset_seconds, time_buffer, sizeof(time_buffer));
        display_draw_time(&display, time_buffer, (long)clock.drift_ms, config.clock_colour_set ? config.clock_colour : 0xFFu);
        sleep_ms(1000);
    }

    return 0;
}
