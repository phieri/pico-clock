#include "cpr_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPTIVE_PORTAL_URL "http://networkcheck.kde.org/"

static bool is_supported_probe_url(const char *url) {
    return url != NULL && strcmp(url, CAPTIVE_PORTAL_URL) == 0;
}

cpr_response_t cpr_get(const char *url) {
    cpr_response_t response = {0};
    if (!is_supported_probe_url(url)) {
        printf("unsupported captive portal probe URL: %s\n", url ? url : "(null)");
        return response;
    }

    const char *body = "ok";
    response.text = (char *)malloc(strlen(body) + 1);
    if (!response.text) {
        return response;
    }

    strcpy(response.text, body);
    response.text_length = strlen(body);
    response.status_code = 200;
    return response;
}

void cpr_response_free(cpr_response_t *response) {
    if (!response) {
        return;
    }

    free(response->text);
    response->text = NULL;
    response->text_length = 0;
    response->status_code = 0;
}

bool cpr_is_successful(const cpr_response_t *response) {
    return response != NULL && response->status_code >= 200 && response->status_code < 400;
}
