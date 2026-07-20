#include "cpr_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPTIVE_PORTAL_URL "http://networkcheck.kde.org/"
#define CAPTIVE_PORTAL_FALLBACK_URL_1 "http://detectportal.firefox.com/success.txt"
#define CAPTIVE_PORTAL_FALLBACK_URL_2 "http://captive.apple.com/hotspot-detect.html"
#define CAPTIVE_PORTAL_FALLBACK_URL_3 "http://clients3.google.com/generate_204"

static bool is_supported_probe_url(const char *url) {
    return url != NULL && (
        strcmp(url, CAPTIVE_PORTAL_URL) == 0 ||
        strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_1) == 0 ||
        strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_2) == 0 ||
        strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_3) == 0
    );
}

static const char *probe_body_for_url(const char *url) {
    if (url == NULL) {
        return NULL;
    }

    if (strcmp(url, CAPTIVE_PORTAL_URL) == 0) {
        return "ok";
    }
    if (strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_1) == 0) {
        return "success";
    }
    if (strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_2) == 0) {
        return "Success";
    }
    if (strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_3) == 0) {
        return "";
    }

    return NULL;
}

static long probe_status_code_for_url(const char *url) {
    if (url != NULL && strcmp(url, CAPTIVE_PORTAL_FALLBACK_URL_3) == 0) {
        return 204;
    }
    return 200;
}

cpr_response_t cpr_get(const char *url) {
    cpr_response_t response = {0};
    if (!is_supported_probe_url(url)) {
        printf("unsupported captive portal probe URL: %s\n", url ? url : "(null)");
        return response;
    }

    const char *body = probe_body_for_url(url);
    if (body == NULL) {
        return response;
    }

    response.text = (char *)malloc(strlen(body) + 1);
    if (!response.text) {
        return response;
    }

    strcpy(response.text, body);
    response.text_length = strlen(body);
    response.status_code = probe_status_code_for_url(url);
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
