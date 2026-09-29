#include "cpr_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"

#define PROBE_TIMEOUT_MS 5000u

typedef struct {
    const char *url;
    const char *host;
    const char *path;
} probe_endpoint_t;

static const probe_endpoint_t endpoints[] = {
    {"http://networkcheck.kde.org/", "networkcheck.kde.org", "/"},
    {"http://detectportal.firefox.com/success.txt", "detectportal.firefox.com", "/success.txt"},
    {"http://captive.apple.com/hotspot-detect.html", "captive.apple.com", "/hotspot-detect.html"},
    {"http://clients3.google.com/generate_204", "clients3.google.com", "/generate_204"},
};

typedef struct {
    volatile bool pending;
    volatile bool done;
    volatile bool resolved;
    ip_addr_t address;
} probe_dns_t;

typedef struct {
    struct tcp_pcb *pcb;
    volatile bool done;
    bool failed;
    char data[1024];
    size_t length;
    char request[192];
} probe_http_t;

static probe_dns_t dns_states[sizeof(endpoints) / sizeof(endpoints[0])];

static void probe_dns_result(const char *name, const ip_addr_t *address, void *arg) {
    (void)name;
    probe_dns_t *state = arg;
    if (address != NULL) {
        state->address = *address;
    }
    state->resolved = address != NULL;
    state->done = true;
    state->pending = false;
}

static err_t probe_http_close(probe_http_t *state) {
    err_t result = ERR_OK;
    if (state->pcb != NULL) {
        tcp_arg(state->pcb, NULL);
        tcp_recv(state->pcb, NULL);
        tcp_err(state->pcb, NULL);
        if (tcp_close(state->pcb) != ERR_OK) {
            tcp_abort(state->pcb);
            result = ERR_ABRT;
        }
        state->pcb = NULL;
    }
    state->done = true;
    return result;
}

static void probe_http_error(void *arg, err_t error) {
    (void)error;
    probe_http_t *state = arg;
    state->pcb = NULL;
    state->failed = true;
    state->done = true;
}

static err_t probe_http_receive(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t error) {
    probe_http_t *state = arg;
    if (p == NULL || error != ERR_OK) {
        if (p != NULL) {
            state->failed = true;
            pbuf_free(p);
        }
        return probe_http_close(state);
    }

    size_t available = sizeof(state->data) - state->length - 1u;
    size_t count = p->tot_len < available ? p->tot_len : available;
    pbuf_copy_partial(p, state->data + state->length, count, 0);
    state->length += count;
    state->data[state->length] = '\0';
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    char *body = strstr(state->data, "\r\n\r\n");
    if (state->length == sizeof(state->data) - 1u ||
        (body != NULL && (strncmp(state->data, "HTTP/1.1 204 ", 13) == 0 ||
                          strncmp(state->data, "HTTP/1.0 204 ", 13) == 0))) {
        return probe_http_close(state);
    }
    return ERR_OK;
}

static err_t probe_http_connected(void *arg, struct tcp_pcb *pcb, err_t error) {
    probe_http_t *state = arg;
    if (error != ERR_OK) {
        state->failed = true;
        probe_http_close(state);
        return ERR_OK;
    }
    if (tcp_write(pcb, state->request, strlen(state->request), TCP_WRITE_FLAG_COPY) != ERR_OK ||
        tcp_output(pcb) != ERR_OK) {
        return probe_http_close(state);
    }
    return ERR_OK;
}

cpr_response_t cpr_get(const char *url) {
    cpr_response_t response = {0};
    size_t index = 0;
    for (; index < sizeof(endpoints) / sizeof(endpoints[0]); ++index) {
        if (url != NULL && strcmp(url, endpoints[index].url) == 0) {
            break;
        }
    }
    if (index == sizeof(endpoints) / sizeof(endpoints[0])) {
        return response;
    }

    probe_dns_t *dns = &dns_states[index];
    if (dns->pending) {
        return response;
    }
    dns->done = false;
    dns->resolved = false;
    cyw43_arch_lwip_begin();
    err_t error = dns_gethostbyname(endpoints[index].host, &dns->address, probe_dns_result, dns);
    if (error == ERR_INPROGRESS) {
        dns->pending = true;
    }
    cyw43_arch_lwip_end();
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (error == ERR_INPROGRESS && !dns->done &&
           (uint32_t)(to_ms_since_boot(get_absolute_time()) - start) < PROBE_TIMEOUT_MS) {
        sleep_ms(10);
    }
    if (error != ERR_OK && (error != ERR_INPROGRESS || !dns->done || !dns->resolved)) {
        return response;
    }

    probe_http_t http = {0};
    snprintf(http.request, sizeof(http.request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             endpoints[index].path, endpoints[index].host);
    cyw43_arch_lwip_begin();
    http.pcb = tcp_new_ip_type(IP_GET_TYPE(&dns->address));
    if (http.pcb != NULL) {
        tcp_arg(http.pcb, &http);
        tcp_recv(http.pcb, probe_http_receive);
        tcp_err(http.pcb, probe_http_error);
        error = tcp_connect(http.pcb, &dns->address, 80u, probe_http_connected);
        if (error != ERR_OK) {
            probe_http_close(&http);
        }
    }
    cyw43_arch_lwip_end();

    start = to_ms_since_boot(get_absolute_time());
    while (!http.done && http.pcb != NULL &&
           (uint32_t)(to_ms_since_boot(get_absolute_time()) - start) < PROBE_TIMEOUT_MS) {
        sleep_ms(10);
    }
    bool completed = http.done;
    cyw43_arch_lwip_begin();
    probe_http_close(&http);
    cyw43_arch_lwip_end();

    int code = 0;
    char *body = strstr(http.data, "\r\n\r\n");
    if (!completed || http.failed || body == NULL ||
        sscanf(http.data, "HTTP/%*u.%*u %d", &code) != 1 || code < 200 || code >= 300) {
        return response;
    }
    body += 4;
    size_t length = http.length - (size_t)(body - http.data);
    response.text = malloc(length + 1u);
    if (response.text == NULL) {
        return response;
    }
    memcpy(response.text, body, length);
    response.text[length] = '\0';
    response.text_length = length;
    response.status_code = code;
    return response;
}

void cpr_response_free(cpr_response_t *response) {
    if (response == NULL) {
        return;
    }
    free(response->text);
    response->text = NULL;
    response->text_length = 0u;
    response->status_code = 0;
}

bool cpr_is_successful(const cpr_response_t *response) {
    return response != NULL && response->status_code >= 200 && response->status_code < 300;
}
