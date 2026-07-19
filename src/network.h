#ifndef PICO_CLOCK_NETWORK_H
#define PICO_CLOCK_NETWORK_H

#include <stdbool.h>

#include "clock.h"
#include "config.h"

bool wifi_connect(const pico_config_t *config);
bool ntp_sync(clock_state_t *state, const pico_config_t *config);

#endif
