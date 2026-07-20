#ifndef PICO_CLOCK_RUNTIME_H
#define PICO_CLOCK_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#include "clock.h"
#include "config.h"
#include "display.h"

#define RUNTIME_SERIAL_BUFFER_SIZE 160u

typedef struct {
    clock_state_t clock;
    display_framebuffer_t display;
    pico_config_t config;
    char serial_buffer[RUNTIME_SERIAL_BUFFER_SIZE];
    size_t serial_length;
    bool wifi_ready;
    bool config_dirty;
} runtime_state_t;

void runtime_state_init(runtime_state_t *state);
void runtime_run(runtime_state_t *state);

#endif
