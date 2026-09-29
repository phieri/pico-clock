#ifndef PICO_CLOCK_DISPLAY_OUTPUT_H
#define PICO_CLOCK_DISPLAY_OUTPUT_H

#include "display.h"

void display_output_start(const display_framebuffer_t *initial_frame);
void display_output_present(const display_framebuffer_t *frame);

#endif
