#ifndef PICO_CLOCK_DISPLAY_H
#define PICO_CLOCK_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#define DISPLAY_WIDTH 1024u
#define DISPLAY_HEIGHT 600u

typedef struct {
    uint8_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    uint16_t width;
    uint16_t height;
} display_framebuffer_t;

void display_init(display_framebuffer_t *framebuffer);
void display_clear(display_framebuffer_t *framebuffer, uint8_t value);
void display_draw_pixel(display_framebuffer_t *framebuffer, int x, int y, uint8_t value);
void display_draw_rect(display_framebuffer_t *framebuffer, int x, int y, int width, int height, uint8_t value);
void display_draw_text(display_framebuffer_t *framebuffer, int x, int y, const char *text, int scale, uint8_t value);
void display_draw_time(display_framebuffer_t *framebuffer, const char *time_buffer, uint8_t colour);

#endif
