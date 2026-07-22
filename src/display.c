#include "display.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int width;
    int height;
    int scale;
    int x;
    int y;
} display_layout_t;

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
        {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}, // A
        {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}, // C
        {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}, // K
        {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}, // L
        {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}, // N
        {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}, // O
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}, // P
        {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}, // U
        {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}, // Y
        {0b10001, 0b10101, 0b10101, 0b10101, 0b10101, 0b11011, 0b10001}, // W
        {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}, // E
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
            case 'A':
            case 'a': glyph_index = 20; break;
            case 'C':
            case 'c': glyph_index = 21; break;
            case 'K':
            case 'k': glyph_index = 22; break;
            case 'L':
            case 'l': glyph_index = 23; break;
            case 'N':
            case 'n': glyph_index = 24; break;
            case 'O':
            case 'o': glyph_index = 25; break;
            case 'P':
            case 'p': glyph_index = 26; break;
            case 'U':
            case 'u': glyph_index = 27; break;
            case 'Y':
            case 'y': glyph_index = 28; break;
            case 'W':
            case 'w': glyph_index = 29; break;
            case 'E':
            case 'e': glyph_index = 30; break;
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

static int display_measure_text_width(const char *text, int scale) {
    int width = 0;
    for (const char *it = text; *it != '\0'; ++it) {
        if (*it == ' ') {
            width += 4 * scale;
        } else {
            width += 6 * scale;
        }
    }
    return width;
}

static void display_prepare_layout(const char *time_buffer, const char *date_buffer, bool show_date, display_layout_t *layout) {
    const size_t time_length = strlen(time_buffer);
    const size_t date_length = (show_date && date_buffer != NULL) ? strlen(date_buffer) : 0u;
    const size_t longest_length = time_length > date_length ? time_length : date_length;

    const int max_width_scale = (DISPLAY_WIDTH - 80u) / (int)((longest_length * 6u) - 1u);
    const int max_height_scale = (DISPLAY_HEIGHT - 80u - (show_date ? 20 : 0)) / (show_date ? 14 : 7);
    layout->scale = max_width_scale < max_height_scale ? max_width_scale : max_height_scale;
    if (layout->scale < 1) {
        layout->scale = 1;
    }

    const int text_width = (int)((longest_length * 6u - 1u) * (unsigned)layout->scale);
    const int text_height = 7 * layout->scale;
    const int status_height = 7;
    layout->width = text_width;
    layout->height = (show_date ? (text_height * 2u + 20u) : text_height) + 20u + status_height;
    layout->x = (DISPLAY_WIDTH - (unsigned)text_width) / 2u;
    layout->y = (DISPLAY_HEIGHT - (unsigned)layout->height) / 2u;
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

void display_draw_startup(display_framebuffer_t *framebuffer, uint8_t colour) {
    display_draw_background(framebuffer);
    display_draw_rect(framebuffer, 40, 40, DISPLAY_WIDTH - 80u, DISPLAY_HEIGHT - 80u, colour);

    const char *title = "PICO CLOCK";
    const char *subtitle = "SYNCING";
    const int title_scale = 4;
    const int subtitle_scale = 2;
    const int title_width = display_measure_text_width(title, title_scale);
    const int subtitle_width = display_measure_text_width(subtitle, subtitle_scale);
    const int title_x = (DISPLAY_WIDTH - (unsigned)title_width) / 2u;
    const int subtitle_x = (DISPLAY_WIDTH - (unsigned)subtitle_width) / 2u;

    display_draw_text(framebuffer, title_x, 160, title, title_scale, colour);
    display_draw_text(framebuffer, subtitle_x, 330, subtitle, subtitle_scale, colour);
}

void display_draw_time(display_framebuffer_t *framebuffer, const char *time_buffer, const char *date_buffer, bool show_date, const char *status_buffer, uint8_t colour) {
    const size_t time_length = strlen(time_buffer);
    if (time_length == 0u) {
        return;
    }

    display_draw_background(framebuffer);

    display_layout_t layout;
    display_prepare_layout(time_buffer, date_buffer, show_date, &layout);

    const int time_x = layout.x + (((int)strlen(time_buffer) * 6u - 1u) * layout.scale - (int)(time_length * 6u - 1u) * layout.scale) / 2;
    const int time_y = layout.y;

    display_draw_text(framebuffer, time_x, time_y, time_buffer, layout.scale, colour);
    if (show_date && date_buffer != NULL && strlen(date_buffer) != 0u) {
        const int date_width = (int)((strlen(date_buffer) * 6u - 1u) * (unsigned)layout.scale);
        const int date_x = layout.x + (((int)strlen(time_buffer) * 6u - 1u) * layout.scale - date_width) / 2;
        const int date_y = time_y + 7 * layout.scale + 20;
        display_draw_text(framebuffer, date_x, date_y, date_buffer, layout.scale, colour);
    }

    if (status_buffer != NULL && status_buffer[0] != '\0') {
        const int status_width = display_measure_text_width(status_buffer, 1);
        const int status_x = layout.x + (((int)strlen(time_buffer) * 6u - 1u) * layout.scale - status_width) / 2;
        const int status_y = (show_date && date_buffer != NULL && strlen(date_buffer) != 0u)
            ? (time_y + 7 * layout.scale + 20 + 7 * layout.scale + 10)
            : (time_y + 7 * layout.scale + 10);
        display_draw_text(framebuffer, status_x, status_y, status_buffer, 1, colour);
    }
}
