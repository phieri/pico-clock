#ifndef PICO_CLOCK_CPR_COMPAT_H
#define PICO_CLOCK_CPR_COMPAT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    long status_code;
    char *text;
    size_t text_length;
} cpr_response_t;

cpr_response_t cpr_get(const char *url);
void cpr_response_free(cpr_response_t *response);
bool cpr_is_successful(const cpr_response_t *response);

#endif
