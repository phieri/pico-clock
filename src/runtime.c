#include "runtime.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "network.h"

#define NTP_SYNC_INTERVAL_MS (30u * 60u * 1000u)
#define CLOCK_REFRESH_INTERVAL_MS 1000u

static bool process_serial_input(runtime_state_t *state) {
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) {
        return false;
    }

    if (ch == '\r' || ch == '\n') {
        if (state->serial_length == 0u) {
            return false;
        }

        state->serial_buffer[state->serial_length] = '\0';
        char response[96];
        if (config_handle_command(&state->config, state->serial_buffer, response, sizeof(response))) {
            if (!config_save(&state->config)) {
                printf("config save failed\n");
            }
            printf("%s\n", response);
            state->config_dirty = true;
        } else {
            printf("%s\n", response);
        }
        state->serial_length = 0u;
        return true;
    }

    if (state->serial_length < (RUNTIME_SERIAL_BUFFER_SIZE - 1u)) {
        state->serial_buffer[state->serial_length++] = (char)ch;
    }
    return false;
}

static void refresh_clock_display(runtime_state_t *state) {
    uint32_t now = clock_now_ms();
    uint64_t epoch = clock_current_epoch_seconds(&state->clock, now);
    char time_buffer[16];
    char date_buffer[24];
    int32_t timezone_offset_seconds = state->config.timezone_set ? state->config.timezone_offset_seconds : 0;
    bool show_date = false;
    clock_format_hms(epoch, timezone_offset_seconds, time_buffer, sizeof(time_buffer));
    if (state->config.date_display_mode == PICO_DATE_DISPLAY_ON) {
        show_date = true;
    } else if (state->config.date_display_mode == PICO_DATE_DISPLAY_AUTO) {
        show_date = clock_should_show_date(epoch, timezone_offset_seconds, 2u);
    }
    if (show_date) {
        clock_format_date(epoch, timezone_offset_seconds, date_buffer, sizeof(date_buffer));
    }
    display_draw_time(&state->display, time_buffer, show_date ? date_buffer : NULL, show_date,
                      state->config.clock_colour_set ? state->config.clock_colour : 0xFFu);
}

static void render_runtime_view(runtime_state_t *state) {
    if (!state->clock.has_time) {
        display_draw_startup(&state->display, state->config.clock_colour_set ? state->config.clock_colour : 0xFFu);
    } else {
        refresh_clock_display(state);
    }
}

static bool ensure_network_ready(runtime_state_t *state) {
    if (state->wifi_ready) {
        return true;
    }

    state->wifi_ready = wifi_connect(&state->config);
    if (!state->wifi_ready) {
        sleep_ms(5000);
        return false;
    }

    return true;
}

static void sync_time_if_needed(runtime_state_t *state) {
    if (!state->clock.has_time || (clock_now_ms() - state->clock.last_sync_ms) > NTP_SYNC_INTERVAL_MS) {
        ntp_sync(&state->clock, &state->config);
    }
}

void runtime_state_init(runtime_state_t *state) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    stdio_init_all();
    clock_init(&state->clock);
    display_init(&state->display);
    config_init(&state->config);
    if (!config_load(&state->config)) {
        printf("config load failed; using defaults\n");
    }
}

void runtime_run(runtime_state_t *state) {
    if (state == NULL) {
        return;
    }

    while (true) {
        process_serial_input(state);

        if (state->config_dirty) {
            state->config_dirty = false;
            state->wifi_ready = false;
        }

        render_runtime_view(state);

        if (!ensure_network_ready(state)) {
            continue;
        }

        sync_time_if_needed(state);
        render_runtime_view(state);
        sleep_ms(CLOCK_REFRESH_INTERVAL_MS);
    }
}
