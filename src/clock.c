#include "clock.h"

#include <stdio.h>
#include <string.h>

#include "pico/time.h"

void clock_init(clock_state_t *state) {
    memset(state, 0, sizeof(*state));
}

uint32_t clock_now_ms(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

uint64_t clock_current_epoch_seconds(const clock_state_t *state, uint32_t now) {
    if (!state->has_time) {
        return 0;
    }

    uint32_t elapsed_ms = now - state->boot_ms;
    int64_t elapsed_seconds = (int64_t)(elapsed_ms / 1000u);
    int64_t adjusted_seconds = (int64_t)state->boot_epoch_seconds + elapsed_seconds + (state->drift_ms / 1000);
    return (uint64_t)adjusted_seconds;
}

void clock_format_hms(uint64_t epoch_seconds, int32_t timezone_offset_seconds, char *buffer, size_t size) {
    int64_t adjusted_seconds = (int64_t)epoch_seconds + (int64_t)timezone_offset_seconds;
    int64_t day_seconds = adjusted_seconds % 86400LL;
    if (day_seconds < 0) {
        day_seconds += 86400LL;
    }
    uint32_t seconds = (uint32_t)day_seconds;
    uint32_t hours = seconds / 3600u;
    uint32_t minutes = (seconds % 3600u) / 60u;
    uint32_t secs = seconds % 60u;
    snprintf(buffer, size, "%02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes, (unsigned long)secs);
}
