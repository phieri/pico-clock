#include "display.h"

#include <stdio.h>
#include <string.h>

static void draw_glyph(display_framebuffer_t *framebuffer, int x, int y, char ch, int scale, uint8_t value) {
    static const uint8_t font5x7[][7] = {
        {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}, // 0
        {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, // 1
        {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}, // 2
        {0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110}, // 3
        {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}, // 4
        {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b10001, 0b01110}, // 5
        {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}, // 6
        {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}, // 7
        {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}, // 8
        {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}, // 9
        {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000}, // :
        {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}, // -
        {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000}, // =
        {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}, // D
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}, // R
        {0b11111, 0b00010, 0b00010, 0b00010, 0b00010, 0b00010, 0b11111}, // I
        {0b10000, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b11110}, // F
        {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}, // T
        {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}, // M
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b10001}, // S
    };

    int glyph_index = -1;
    if (ch >= '0' && ch <= '9') {
        glyph_index = ch - '0';
    } else {
        switch (ch) {
            case ':': glyph_index = 10; break;
            case '-': glyph_index = 11; break;
            case '=': glyph_index = 12; break;
            case 'D':
            case 'd': glyph_index = 13; break;
            case 'R':
            case 'r': glyph_index = 14; break;
            case 'I':
            case 'i': glyph_index = 15; break;
            case 'F':
            case 'f': glyph_index = 16; break;
            case 'T':
            case 't': glyph_index = 17; break;
            case 'M':
            case 'm': glyph_index = 18; break;
            case 'S':
            case 's': glyph_index = 19; break;
            default: return;
        }
    }

    const uint8_t *glyph = font5x7[glyph_index];
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((glyph[row] >> (4 - col)) & 0x1u) {
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        display_draw_pixel(framebuffer, x + (col * scale) + sx, y + (row * scale) + sy, value);
                    }
                }
            }
        }
    }
}

void display_init(display_framebuffer_t *framebuffer) {
    memset(framebuffer, 0, sizeof(*framebuffer));
    framebuffer->width = DISPLAY_WIDTH;
    framebuffer->height = DISPLAY_HEIGHT;
}

void display_clear(display_framebuffer_t *framebuffer, uint8_t value) {
    memset(framebuffer->pixels, value, sizeof(framebuffer->pixels));
}

void display_draw_pixel(display_framebuffer_t *framebuffer, int x, int y, uint8_t value) {
    if (x < 0 || y < 0 || x >= (int)framebuffer->width || y >= (int)framebuffer->height) {
        return;
    }

    framebuffer->pixels[(size_t)y * framebuffer->width + (size_t)x] = value;
}

void display_draw_rect(display_framebuffer_t *framebuffer, int x, int y, int width, int height, uint8_t value) {
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            display_draw_pixel(framebuffer, x + col, y + row, value);
        }
    }
}

void display_draw_text(display_framebuffer_t *framebuffer, int x, int y, const char *text, int scale, uint8_t value) {
    if (scale <= 0) {
        return;
    }

    int cursor_x = x;
    for (const char *it = text; *it != '\0'; ++it) {
        if (*it == ' ') {
            cursor_x += 4 * scale;
            continue;
        }

        draw_glyph(framebuffer, cursor_x, y, *it, scale, value);
        cursor_x += 6 * scale;
    }
}

void display_draw_time(display_framebuffer_t *framebuffer, const char *time_buffer, long drift_ms) {
    const int title_scale = 4;
    const int detail_scale = 3;
    const int title_y = 120;
    const int detail_y = 290;

    display_draw_rect(framebuffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x00u);
    display_draw_rect(framebuffer, 40, 40, DISPLAY_WIDTH - 80, DISPLAY_HEIGHT - 80, 0x00u);

    display_draw_text(framebuffer, 140, title_y, time_buffer, title_scale, 0xFFu);
    char drift_buffer[32];
    int written = snprintf(drift_buffer, sizeof(drift_buffer), "DRIFT=%ldMS", drift_ms);
    if (written < 0) {
        drift_buffer[0] = '\0';
    }

    display_draw_text(framebuffer, 140, detail_y, drift_buffer, detail_scale, 0xFFu);
}
