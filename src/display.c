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

static void display_draw_background(display_framebuffer_t *framebuffer) {
    display_clear(framebuffer, 0x00u);
}

void display_draw_time(display_framebuffer_t *framebuffer, const char *time_buffer, uint8_t colour) {
    const size_t time_length = strlen(time_buffer);
    if (time_length == 0u) {
        return;
    }

    display_draw_background(framebuffer);

    const int max_width_scale = (DISPLAY_WIDTH - 80u) / (int)(time_length * 6u - 1u);
    const int max_height_scale = (DISPLAY_HEIGHT - 80u) / 7;
    int scale = max_width_scale < max_height_scale ? max_width_scale : max_height_scale;
    if (scale < 1) {
        scale = 1;
    }

    const int text_width = (int)((time_length * 6u - 1u) * (unsigned)scale);
    const int text_height = 7 * scale;
    const int x = (DISPLAY_WIDTH - (unsigned)text_width) / 2u;
    const int y = (DISPLAY_HEIGHT - (unsigned)text_height) / 2u;

    display_draw_text(framebuffer, x, y, time_buffer, scale, colour);
}
