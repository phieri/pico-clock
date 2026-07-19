#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/flash.h"
#include "pico/stdlib.h"

#include "lfs.h"

#define CONFIG_LFS_BLOCK_SIZE FLASH_SECTOR_SIZE
#define CONFIG_LFS_BLOCK_COUNT 16u
#define CONFIG_LFS_READ_SIZE 16u
#define CONFIG_LFS_PROG_SIZE 256u
#define CONFIG_LFS_CACHE_SIZE 64u
#define CONFIG_LFS_LOOKAHEAD 32u
#define CONFIG_LFS_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (CONFIG_LFS_BLOCK_SIZE * CONFIG_LFS_BLOCK_COUNT))
#define CONFIG_FILE_NAME "/config.bin"

#define CONFIG_MAGIC 0x50434f4eUL
#define CONFIG_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t timezone_offset_seconds;
    uint8_t timezone_set;
    uint8_t clock_colour;
    uint8_t clock_colour_set;
    uint8_t wifi_configured;
    char wifi_ssid[32];
    char wifi_password[64];
    uint8_t ntp_server_set;
    char ntp_server[64];
} persisted_config_t;

typedef struct {
    uint32_t flash_offset;
    size_t block_size;
    size_t block_count;
} flash_lfs_context_t;

static lfs_t g_lfs;
static struct lfs_config g_lfs_config;
static flash_lfs_context_t g_lfs_context;
static bool g_lfs_ready = false;

static int flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    const flash_lfs_context_t *ctx = (const flash_lfs_context_t *)c->context;
    const uint8_t *source = (const uint8_t *)(XIP_BASE + ctx->flash_offset + (size_t)block * ctx->block_size + (size_t)off);
    memcpy(buffer, source, size);
    return 0;
}

static int flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    flash_lfs_context_t *ctx = (flash_lfs_context_t *)c->context;
    uint32_t flash_offset = ctx->flash_offset + (size_t)block * ctx->block_size + (size_t)off;
    flash_range_program(flash_offset, (const uint8_t *)buffer, size);
    return 0;
}

static int flash_erase(const struct lfs_config *c, lfs_block_t block) {
    flash_lfs_context_t *ctx = (flash_lfs_context_t *)c->context;
    uint32_t flash_offset = ctx->flash_offset + (size_t)block * ctx->block_size;
    flash_range_erase(flash_offset, ctx->block_size);
    return 0;
}

static int flash_sync(const struct lfs_config *c) {
    (void)c;
    return 0;
}

static void lowercase_copy(char *dst, size_t size, const char *src) {
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; ++i) {
        dst[i] = (char)((src[i] >= 'A' && src[i] <= 'Z') ? (src[i] - 'A' + 'a') : src[i]);
    }
    dst[i] = '\0';
}

static void copy_string(char *dst, size_t dst_size, const char *src) {
    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void config_apply_defaults(pico_config_t *config) {
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->clock_colour = 0xFFu;
    config->wifi_ssid[0] = '\0';
    config->wifi_password[0] = '\0';
    config->ntp_server[0] = '\0';
    copy_string(config->ntp_server, sizeof(config->ntp_server), "2001:4860:4860::8888,216.239.35.0");
}

static void config_load_persisted(const persisted_config_t *persisted, pico_config_t *config) {
    if (persisted == NULL || config == NULL) {
        return;
    }

    config->timezone_set = persisted->timezone_set != 0;
    config->timezone_offset_seconds = persisted->timezone_offset_seconds;
    config->clock_colour_set = persisted->clock_colour_set != 0;
    config->clock_colour = persisted->clock_colour;
    config->wifi_configured = persisted->wifi_configured != 0;
    copy_string(config->wifi_ssid, sizeof(config->wifi_ssid), persisted->wifi_ssid);
    copy_string(config->wifi_password, sizeof(config->wifi_password), persisted->wifi_password);
    config->ntp_server_set = persisted->ntp_server_set != 0;
    copy_string(config->ntp_server, sizeof(config->ntp_server), persisted->ntp_server);
}

static void config_store_persisted(const pico_config_t *config, persisted_config_t *persisted) {
    if (persisted == NULL || config == NULL) {
        return;
    }

    memset(persisted, 0, sizeof(*persisted));
    persisted->magic = CONFIG_MAGIC;
    persisted->version = CONFIG_VERSION;
    persisted->timezone_offset_seconds = config->timezone_set ? config->timezone_offset_seconds : 0;
    persisted->timezone_set = config->timezone_set ? 1u : 0u;
    persisted->clock_colour = config->clock_colour_set ? config->clock_colour : 0xFFu;
    persisted->clock_colour_set = config->clock_colour_set ? 1u : 0u;
    persisted->wifi_configured = config->wifi_configured ? 1u : 0u;
    copy_string(persisted->wifi_ssid, sizeof(persisted->wifi_ssid), config->wifi_ssid);
    copy_string(persisted->wifi_password, sizeof(persisted->wifi_password), config->wifi_password);
    persisted->ntp_server_set = config->ntp_server_set ? 1u : 0u;
    copy_string(persisted->ntp_server, sizeof(persisted->ntp_server), config->ntp_server);
}

static bool parse_timezone_offset(const char *text, int32_t *offset_seconds) {
    if (text == NULL || *text == '\0' || offset_seconds == NULL) {
        return false;
    }

    if (strcmp(text, "utc") == 0 || strcmp(text, "z") == 0) {
        *offset_seconds = 0;
        return true;
    }

    char *end = NULL;
    long hours = strtol(text, &end, 10);
    if (end == text) {
        return false;
    }

    if (*end == '\0') {
        *offset_seconds = (int32_t)(hours * 3600L);
        return true;
    }

    if (*end == ':' && end[1] != '\0') {
        char *minute_end = NULL;
        long minutes = strtol(end + 1, &minute_end, 10);
        if (minute_end == (end + 1) || *minute_end != '\0') {
            return false;
        }
        if (hours < 0) {
            minutes = -minutes;
        }
        *offset_seconds = (int32_t)((hours * 3600L) + (minutes * 60L));
        return true;
    }

    return false;
}

static bool parse_colour(const char *text, uint8_t *value) {
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 255) {
        return false;
    }

    *value = (uint8_t)parsed;
    return true;
}

static bool config_prepare_filesystem(void) {
    if (g_lfs_ready) {
        return true;
    }

    memset(&g_lfs, 0, sizeof(g_lfs));
    memset(&g_lfs_config, 0, sizeof(g_lfs_config));
    g_lfs_context.flash_offset = CONFIG_LFS_FLASH_OFFSET;
    g_lfs_context.block_size = CONFIG_LFS_BLOCK_SIZE;
    g_lfs_context.block_count = CONFIG_LFS_BLOCK_COUNT;

    g_lfs_config.context = &g_lfs_context;
    g_lfs_config.read = flash_read;
    g_lfs_config.prog = flash_prog;
    g_lfs_config.erase = flash_erase;
    g_lfs_config.sync = flash_sync;
    g_lfs_config.read_size = CONFIG_LFS_READ_SIZE;
    g_lfs_config.prog_size = CONFIG_LFS_PROG_SIZE;
    g_lfs_config.block_size = CONFIG_LFS_BLOCK_SIZE;
    g_lfs_config.block_count = CONFIG_LFS_BLOCK_COUNT;
    g_lfs_config.cache_size = CONFIG_LFS_CACHE_SIZE;
    g_lfs_config.lookahead_size = CONFIG_LFS_LOOKAHEAD;
    g_lfs_config.name_max = 32;
    g_lfs_config.file_max = 256;
    g_lfs_config.attr_max = 0;
    g_lfs_config.metadata_max = 0;
    g_lfs_config.inline_max = 16;

    int err = lfs_mount(&g_lfs, &g_lfs_config);
    if (err == LFS_ERR_CORRUPT) {
        lfs_format(&g_lfs, &g_lfs_config);
        err = lfs_mount(&g_lfs, &g_lfs_config);
    }
    if (err == LFS_ERR_NOENT) {
        lfs_format(&g_lfs, &g_lfs_config);
        err = lfs_mount(&g_lfs, &g_lfs_config);
    }
    if (err != LFS_ERR_OK) {
        return false;
    }

    g_lfs_ready = true;
    return true;
}

void config_init(pico_config_t *config) {
    config_apply_defaults(config);
}

bool config_load(pico_config_t *config) {
    if (config == NULL) {
        return false;
    }

    config_init(config);
    if (!config_prepare_filesystem()) {
        return false;
    }

    lfs_file_t file;
    int err = lfs_file_open(&g_lfs, &file, CONFIG_FILE_NAME, LFS_O_RDONLY);
    if (err != LFS_ERR_OK) {
        return true;
    }

    persisted_config_t persisted;
    memset(&persisted, 0, sizeof(persisted));
    lfs_ssize_t size = lfs_file_read(&g_lfs, &file, &persisted, sizeof(persisted));
    lfs_file_close(&g_lfs, &file);

    if (size < (lfs_ssize_t)sizeof(persisted_config_t)) {
        return true;
    }

    if (persisted.magic != CONFIG_MAGIC || persisted.version != CONFIG_VERSION) {
        return true;
    }

    config_load_persisted(&persisted, config);
    return true;
}

bool config_save(const pico_config_t *config) {
    if (config == NULL) {
        return false;
    }

    if (!config_prepare_filesystem()) {
        return false;
    }

    lfs_file_t file;
    int err = lfs_file_open(&g_lfs, &file, CONFIG_FILE_NAME, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err != LFS_ERR_OK) {
        return false;
    }

    persisted_config_t persisted;
    config_store_persisted(config, &persisted);

    err = lfs_file_write(&g_lfs, &file, &persisted, sizeof(persisted));
    if (err < 0) {
        lfs_file_close(&g_lfs, &file);
        return false;
    }

    lfs_file_sync(&g_lfs, &file);
    lfs_file_close(&g_lfs, &file);
    return true;
}

void config_reset(pico_config_t *config) {
    config_apply_defaults(config);
}

static bool config_parse_command(const char *line, char *command, size_t command_size, char *value, size_t value_size) {
    if (line == NULL || command == NULL || value == NULL || command_size == 0u || value_size == 0u) {
        return false;
    }

    char work[160];
    char *cursor = NULL;
    char *token = NULL;

    strlcpy(work, line, sizeof(work));
    cursor = work;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    token = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
        ++cursor;
    }
    if (*cursor != '\0') {
        *cursor++ = '\0';
    }
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }

    copy_string(command, command_size, token);
    copy_string(value, value_size, cursor);
    return true;
}

static bool config_handle_wifi_command(pico_config_t *config, const char *value, char *response, size_t response_size) {
    char ssid[33];
    char password[64];
    char *ssid_ptr = NULL;
    char *password_ptr = NULL;

    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));

    char work[160];
    strlcpy(work, value, sizeof(work));
    ssid_ptr = strtok(work, " ");
    if (ssid_ptr == NULL || *ssid_ptr == '\0') {
        snprintf(response, response_size, "wifi unset");
        config->wifi_configured = false;
        return true;
    }

    password_ptr = strtok(NULL, " ");
    copy_string(ssid, sizeof(ssid), ssid_ptr);
    if (password_ptr != NULL) {
        copy_string(password, sizeof(password), password_ptr);
    }

    config->wifi_configured = true;
    copy_string(config->wifi_ssid, sizeof(config->wifi_ssid), ssid);
    copy_string(config->wifi_password, sizeof(config->wifi_password), password);
    snprintf(response, response_size, "wifi configured for %s", ssid);
    return true;
}

bool config_handle_command(pico_config_t *config, const char *line, char *response, size_t response_size) {
    if (config == NULL || line == NULL || response == NULL || response_size == 0) {
        return false;
    }

    char command[32];
    char value[128];
    memset(command, 0, sizeof(command));
    memset(value, 0, sizeof(value));

    if (!config_parse_command(line, command, sizeof(command), value, sizeof(value))) {
        snprintf(response, response_size, "unknown command");
        return false;
    }

    char command_lower[32];
    lowercase_copy(command_lower, sizeof(command_lower), command);
    if (strcmp(command_lower, "timezone") == 0 || strcmp(command_lower, "tz") == 0) {
        if (value[0] == '\0') {
            snprintf(response, response_size, "timezone unset");
            config->timezone_set = false;
            return true;
        }
        int32_t offset_seconds = 0;
        if (!parse_timezone_offset(value, &offset_seconds)) {
            snprintf(response, response_size, "invalid timezone");
            return false;
        }
        config->timezone_set = true;
        config->timezone_offset_seconds = offset_seconds;
        snprintf(response, response_size, "timezone set to %ld seconds", (long)offset_seconds);
        return true;
    }

    if (strcmp(command_lower, "color") == 0 || strcmp(command_lower, "colour") == 0) {
        if (value[0] == '\0') {
            snprintf(response, response_size, "color unset");
            config->clock_colour_set = false;
            return true;
        }
        uint8_t colour = 0;
        if (!parse_colour(value, &colour)) {
            snprintf(response, response_size, "invalid color");
            return false;
        }
        config->clock_colour_set = true;
        config->clock_colour = colour;
        snprintf(response, response_size, "color set to %u", (unsigned)colour);
        return true;
    }

    if (strcmp(command_lower, "wifi") == 0) {
        return config_handle_wifi_command(config, value, response, response_size);
    }

    if (strcmp(command_lower, "ntp") == 0) {
        if (value[0] == '\0') {
            snprintf(response, response_size, "ntp unset");
            config->ntp_server_set = false;
            return true;
        }
        config->ntp_server_set = true;
        copy_string(config->ntp_server, sizeof(config->ntp_server), value);
        snprintf(response, response_size, "ntp server set to %s", config->ntp_server);
        return true;
    }

    if (strcmp(command_lower, "reset") == 0) {
        config_reset(config);
        snprintf(response, response_size, "config reset to defaults");
        return true;
    }

    snprintf(response, response_size, "unknown command");
    return false;
}
