#include "clock.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pico/time.h"

static int64_t clock_adjusted_epoch_seconds(uint64_t epoch_seconds, int32_t timezone_offset_seconds) {
    return (int64_t)epoch_seconds + (int64_t)timezone_offset_seconds;
}

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

static bool is_leap_year(unsigned year) {
    return (year % 400u == 0u) || ((year % 4u == 0u) && (year % 100u != 0u));
}

static unsigned days_in_year(unsigned year) {
    return is_leap_year(year) ? 366u : 365u;
}

static unsigned days_in_month(unsigned year, unsigned month) {
    static const unsigned days_per_month[12u] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
    if (month == 2u && is_leap_year(year)) {
        return 29u;
    }
    return days_per_month[month - 1u];
}

static void days_to_date(int64_t day_count, unsigned *year, unsigned *month, unsigned *day) {
    unsigned current_year = 1970u;
    int64_t current_day_count = day_count;

    if (current_day_count >= 0) {
        while (current_day_count >= (int64_t)days_in_year(current_year)) {
            current_day_count -= (int64_t)days_in_year(current_year);
            ++current_year;
        }
    } else {
        while (current_day_count < 0) {
            --current_year;
            current_day_count += (int64_t)days_in_year(current_year);
        }
    }

    unsigned current_month = 1u;
    while (current_day_count >= (int64_t)days_in_month(current_year, current_month)) {
        current_day_count -= (int64_t)days_in_month(current_year, current_month);
        ++current_month;
    }

    *year = current_year;
    *month = current_month;
    *day = (unsigned)current_day_count + 1u;
}

void clock_format_hms(uint64_t epoch_seconds, int32_t timezone_offset_seconds, char *buffer, size_t size) {
    int64_t adjusted_seconds = clock_adjusted_epoch_seconds(epoch_seconds, timezone_offset_seconds);
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

void clock_format_date(uint64_t epoch_seconds, int32_t timezone_offset_seconds, char *buffer, size_t size) {
    int64_t adjusted_seconds = clock_adjusted_epoch_seconds(epoch_seconds, timezone_offset_seconds);
    int64_t day_seconds = adjusted_seconds % 86400LL;
    int64_t day_count = adjusted_seconds / 86400LL;
    if (day_seconds < 0) {
        day_seconds += 86400LL;
        --day_count;
    }

    unsigned year = 0u;
    unsigned month = 0u;
    unsigned day = 0u;
    days_to_date(day_count, &year, &month, &day);

    static const char *const weekday_names[7u] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    int64_t weekday_index = (4LL + day_count) % 7LL;
    if (weekday_index < 0) {
        weekday_index += 7LL;
    }

    snprintf(buffer, size, "%04u-%02u-%02u %s", year, month, day, weekday_names[(size_t)weekday_index]);
}

bool clock_should_show_date(uint64_t epoch_seconds, int32_t timezone_offset_seconds, uint8_t date_display_mode) {
    if (date_display_mode == 0u) {
        return false;
    }
    if (date_display_mode != 2u) {
        return true;
    }

    int64_t adjusted_seconds = clock_adjusted_epoch_seconds(epoch_seconds, timezone_offset_seconds);
    int64_t day_seconds = adjusted_seconds % 86400LL;
    if (day_seconds < 0) {
        day_seconds += 86400LL;
    }

    return (day_seconds >= (23u * 3600u)) || (day_seconds <= (1u * 3600u));
}
