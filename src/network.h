#ifndef PICO_CLOCK_NETWORK_H
#define PICO_CLOCK_NETWORK_H

#include <stdbool.h>

#include "clock.h"

bool wifi_connect(void);
bool ntp_sync(clock_state_t *state);

#endif
