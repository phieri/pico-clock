#ifndef PICO_CLOCK_CONFIG_H
#define PICO_CLOCK_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PICO_DEFAULT_HOSTNAME "ntp-clock"

typedef enum {
    PICO_DATE_DISPLAY_OFF = 0,
    PICO_DATE_DISPLAY_ON = 1,
    PICO_DATE_DISPLAY_AUTO = 2,
} pico_date_display_mode_t;

typedef struct {
    bool timezone_set;
    int32_t timezone_offset_seconds;
    bool clock_colour_set;
    uint8_t clock_colour;
    bool wifi_configured;
    char wifi_ssid[33];
    char wifi_password[64];
    bool ntp_server_set;
    char ntp_server[64];
    pico_date_display_mode_t date_display_mode;
    char hostname[33];
} pico_config_t;

void config_init(pico_config_t *config);
bool config_load(pico_config_t *config);
bool config_save(const pico_config_t *config);
bool config_handle_command(pico_config_t *config, const char *line, char *response, size_t response_size);
void config_reset(pico_config_t *config);

#endif
