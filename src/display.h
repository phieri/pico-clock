#ifndef PICO_CLOCK_DISPLAY_H
#define PICO_CLOCK_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(PICO_CLOCK_DISPLAY_SPI)
#define DISPLAY_WIDTH 320u
#define DISPLAY_HEIGHT 240u
#else
#define DISPLAY_WIDTH 800u
#define DISPLAY_HEIGHT 480u
#endif

typedef struct {
    uint8_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8u];
    uint16_t width;
    uint16_t height;
} display_framebuffer_t;

void display_init(display_framebuffer_t *framebuffer);
void display_clear(display_framebuffer_t *framebuffer, uint8_t value);
void display_draw_pixel(display_framebuffer_t *framebuffer, int x, int y, uint8_t value);
void display_draw_rect(display_framebuffer_t *framebuffer, int x, int y, int width, int height, uint8_t value);
void display_draw_text(display_framebuffer_t *framebuffer, int x, int y, const char *text, int scale, uint8_t value);
void display_draw_startup(display_framebuffer_t *framebuffer, uint8_t colour);
void display_draw_time(display_framebuffer_t *framebuffer, const char *time_buffer, const char *date_buffer, bool show_date, uint8_t colour);

#endif
