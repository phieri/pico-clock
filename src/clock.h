#ifndef PICO_CLOCK_CLOCK_H
#define PICO_CLOCK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NTP_SYNC_INTERVAL_INITIAL_MS (30u * 60u * 1000u)
#define NTP_SYNC_INTERVAL_MAX_MS (24u * 60u * 60u * 1000u)

typedef struct {
    bool has_time;
    uint64_t boot_epoch_seconds;
    uint32_t boot_ms;
    uint32_t last_sync_ms;
    uint32_t sync_interval_ms;
    int32_t drift_ms;
} clock_state_t;

void clock_init(clock_state_t *state);
uint32_t clock_now_ms(void);
uint32_t clock_next_sync_interval_ms(uint32_t current_interval_ms);
uint64_t clock_current_epoch_seconds(const clock_state_t *state, uint32_t now);
void clock_format_hms(uint64_t epoch_seconds, int32_t timezone_offset_seconds, char *buffer, size_t size);
void clock_format_date(uint64_t epoch_seconds, int32_t timezone_offset_seconds, char *buffer, size_t size);
bool clock_should_show_date(uint64_t epoch_seconds, int32_t timezone_offset_seconds, uint8_t date_display_mode);

#endif
