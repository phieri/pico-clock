#ifndef PICO_CLOCK_CLOCK_H
#define PICO_CLOCK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool has_time;
    uint64_t boot_epoch_seconds;
    uint32_t boot_ms;
    uint32_t last_sync_ms;
    int32_t drift_ms;
} clock_state_t;

void clock_init(clock_state_t *state);
uint32_t clock_now_ms(void);
uint64_t clock_current_epoch_seconds(const clock_state_t *state, uint32_t now);
void clock_format_hms(uint64_t epoch_seconds, char *buffer, size_t size);

#endif
