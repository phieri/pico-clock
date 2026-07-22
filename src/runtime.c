#include "runtime.h"

#include <stdio.h>
#include <string.h>

#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "network.h"

#define NTP_SYNC_INTERVAL_MS (30u * 60u * 1000u)
#define CLOCK_REFRESH_INTERVAL_MS 1000u

static runtime_state_t *s_runtime_state = NULL;

static uint32_t runtime_lock_state(runtime_state_t *state) {
    if (state == NULL) {
        return 0u;
    }
    return spin_lock_blocking(state->state_lock);
}

static void runtime_unlock_state(runtime_state_t *state, uint32_t irq_state) {
    if (state == NULL) {
        return;
    }
    spin_unlock(state->state_lock, irq_state);
}

static void runtime_copy_string(char *dst, size_t size, const char *src) {
    if (dst == NULL || size == 0u) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    size_t length = strlen(src);
    if (length >= size) {
        length = size - 1u;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';
}

static void runtime_set_network_flags(runtime_state_t *state, bool config_dirty, bool wifi_ready) {
    if (state == NULL) {
        return;
    }

    uint32_t irq_state = runtime_lock_state(state);
    state->config_dirty = config_dirty;
    state->wifi_ready = wifi_ready;
    runtime_unlock_state(state, irq_state);
}

static bool runtime_is_wifi_ready(const runtime_state_t *state) {
    if (state == NULL) {
        return false;
    }

    uint32_t irq_state = runtime_lock_state((runtime_state_t *)state);
    bool ready = state->wifi_ready;
    runtime_unlock_state((runtime_state_t *)state, irq_state);
    return ready;
}

static clock_state_t runtime_read_clock(const runtime_state_t *state) {
    clock_state_t clock_copy = {0};
    if (state == NULL) {
        return clock_copy;
    }

    uint32_t irq_state = runtime_lock_state((runtime_state_t *)state);
    clock_copy = state->clock;
    runtime_unlock_state((runtime_state_t *)state, irq_state);
    return clock_copy;
}

static pico_config_t runtime_read_config(const runtime_state_t *state) {
    pico_config_t config_copy = {0};
    if (state == NULL) {
        return config_copy;
    }

    uint32_t irq_state = runtime_lock_state((runtime_state_t *)state);
    config_copy = state->config;
    runtime_unlock_state((runtime_state_t *)state, irq_state);
    return config_copy;
}

static void runtime_write_clock(runtime_state_t *state, const clock_state_t *clock_state) {
    if (state == NULL || clock_state == NULL) {
        return;
    }

    uint32_t irq_state = runtime_lock_state(state);
    state->clock = *clock_state;
    runtime_unlock_state(state, irq_state);
}

static bool runtime_should_sync_time(const runtime_state_t *state, uint32_t now) {
    if (state == NULL) {
        return false;
    }

    clock_state_t clock_copy = runtime_read_clock(state);
    return !clock_copy.has_time || (now - clock_copy.last_sync_ms) > NTP_SYNC_INTERVAL_MS;
}

static bool runtime_update_startup_config_window(runtime_state_t *state, uint32_t now) {
    if (state == NULL) {
        return false;
    }

    uint32_t irq_state = runtime_lock_state(state);
    if (state->startup_config_window_active && now >= state->startup_config_deadline_ms) {
        state->startup_config_window_active = false;
        printf("serial configuration window closed; beginning time acquisition\n");
    }
    bool active = state->startup_config_window_active;
    runtime_unlock_state(state, irq_state);
    return active;
}

static void runtime_extend_startup_config_window(runtime_state_t *state, uint32_t now) {
    if (state == NULL) {
        return;
    }

    uint32_t irq_state = runtime_lock_state(state);
    if (state->startup_config_window_active) {
        state->startup_config_deadline_ms = now + STARTUP_CONFIG_DELAY_MS;
    }
    runtime_unlock_state(state, irq_state);
}

static bool process_serial_input(runtime_state_t *state) {
    if (state == NULL) {
        return false;
    }

    uint32_t now = clock_now_ms();
    if (!runtime_update_startup_config_window(state, now)) {
        return false;
    }

    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) {
        return false;
    }

    runtime_extend_startup_config_window(state, clock_now_ms());

    if (ch == '\r' || ch == '\n') {
        if (state->serial_length == 0u) {
            return false;
        }

        state->serial_buffer[state->serial_length] = '\0';
        char response[96];
        uint32_t irq_state = runtime_lock_state(state);
        bool handled = config_handle_command(&state->config, state->serial_buffer, response, sizeof(response));
        if (handled) {
            if (!config_save(&state->config)) {
                printf("config save failed\n");
            }
            state->config_dirty = true;
            state->wifi_ready = false;
            printf("%s\n", response);
        } else {
            printf("%s\n", response);
        }
        runtime_unlock_state(state, irq_state);

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
    clock_state_t clock_copy = runtime_read_clock(state);
    uint64_t epoch = clock_current_epoch_seconds(&clock_copy, now);
    char time_buffer[16];
    char date_buffer[24];
    char status_buffer[16];
    pico_config_t config_copy = runtime_read_config(state);
    int32_t timezone_offset_seconds = config_copy.timezone_set ? config_copy.timezone_offset_seconds : 0;
    bool show_date = false;
    clock_format_hms(epoch, timezone_offset_seconds, time_buffer, sizeof(time_buffer));
    if (config_copy.date_display_mode == PICO_DATE_DISPLAY_ON) {
        show_date = true;
    } else if (config_copy.date_display_mode == PICO_DATE_DISPLAY_AUTO) {
        show_date = clock_should_show_date(epoch, timezone_offset_seconds, config_copy.date_display_mode);
    }
    if (show_date) {
        clock_format_date(epoch, timezone_offset_seconds, date_buffer, sizeof(date_buffer));
    }
    if (!runtime_is_wifi_ready(state)) {
        runtime_copy_string(status_buffer, sizeof(status_buffer), "SYNC");
    } else if (!clock_copy.has_time) {
        runtime_copy_string(status_buffer, sizeof(status_buffer), "WAIT");
    } else {
        runtime_copy_string(status_buffer, sizeof(status_buffer), "NTP");
    }
    display_draw_time(&state->display, time_buffer, show_date ? date_buffer : NULL, show_date, status_buffer,
                      config_copy.clock_colour_set ? config_copy.clock_colour : 0xFFu);
}

static void render_runtime_view(runtime_state_t *state) {
    clock_state_t clock_copy = runtime_read_clock(state);
    if (!clock_copy.has_time) {
        pico_config_t config_copy = runtime_read_config(state);
        display_draw_startup(&state->display, config_copy.clock_colour_set ? config_copy.clock_colour : 0xFFu);
    } else {
        refresh_clock_display(state);
    }
}

static void core1_network_worker(void) {
    runtime_state_t *state = s_runtime_state;
    if (state == NULL) {
        return;
    }

    while (true) {
        bool config_dirty = false;
        bool wifi_ready = false;
        uint32_t irq_state = runtime_lock_state(state);
        config_dirty = state->config_dirty;
        wifi_ready = state->wifi_ready;
        runtime_unlock_state(state, irq_state);

        if (config_dirty) {
            runtime_set_network_flags(state, false, false);
        }

        if (!wifi_ready) {
            pico_config_t config_copy = runtime_read_config(state);
            bool connected = wifi_connect(&config_copy);
            runtime_set_network_flags(state, false, connected);
            if (!connected) {
                sleep_ms(5000);
                continue;
            }
        }

        uint32_t now = clock_now_ms();
        if (runtime_update_startup_config_window(state, now) && runtime_should_sync_time(state, now)) {
            pico_config_t config_copy = runtime_read_config(state);
            clock_state_t clock_copy = runtime_read_clock(state);
            clock_state_t synced_clock = clock_copy;
            if (ntp_sync(&synced_clock, &config_copy)) {
                runtime_write_clock(state, &synced_clock);
            }
        }

        sleep_ms(1000);
    }
}

void runtime_state_init(runtime_state_t *state) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    stdio_init_all();
    spin_lock_init(&state->state_lock);
    clock_init(&state->clock);
    display_init(&state->display);
    config_init(&state->config);
    state->startup_config_window_active = true;
    state->startup_config_deadline_ms = clock_now_ms() + STARTUP_CONFIG_DELAY_MS;
    printf("startup configuration window active for %lu seconds\n", (unsigned long)(STARTUP_CONFIG_DELAY_MS / 1000u));
    if (!config_load(&state->config)) {
        printf("config load failed; using defaults\n");
    }

    s_runtime_state = state;
    multicore_reset_core1();
    multicore_launch_core1(core1_network_worker);
}

void runtime_run(runtime_state_t *state) {
    if (state == NULL) {
        return;
    }

    while (true) {
        process_serial_input(state);
        render_runtime_view(state);
        sleep_ms(CLOCK_REFRESH_INTERVAL_MS);
    }
}
