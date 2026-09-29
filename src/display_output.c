#include "display_output.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "dvi_timing.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tmds_encode.h"

// Waveshare PICO-DVI-LCD uses GP8/9 for clock and GP10-15 for TMDS data.
static const struct dvi_serialiser_cfg waveshare_dvi_cfg = {
    .pio = pio0,
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {10, 12, 14},
    .pins_clk = 8,
    .invert_diffpairs = true
};

static struct dvi_inst dvi;
static const display_framebuffer_t *volatile active_frame;
static const display_framebuffer_t *volatile pending_frame;

static void display_video_worker(void) {
    dvi_register_irqs_this_core(&dvi, DMA_IRQ_0);
    dvi_start(&dvi);

    while (true) {
        // Switch buffers only between complete frames, never during scanout.
        if (pending_frame != NULL) {
            active_frame = pending_frame;
            __dmb();
            pending_frame = NULL;
        }
        const display_framebuffer_t *frame = active_frame;
        for (uint y = 0; y < DISPLAY_HEIGHT; ++y) {
            const uint32_t *pixels = (const uint32_t *)&frame->pixels[y * DISPLAY_WIDTH / 8u];
            uint32_t *tmds;
            queue_remove_blocking_u32(&dvi.q_tmds_free, &tmds);
            tmds_encode_1bpp(pixels, tmds, DISPLAY_WIDTH);
            queue_add_blocking_u32(&dvi.q_tmds_valid, &tmds);
        }
    }
}

void display_output_start(const display_framebuffer_t *initial_frame) {
    active_frame = initial_frame;
    pending_frame = NULL;

    // The Waveshare examples use this voltage and clock for 800x480 DVI.
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(dvi_timing_800x480p_60hz.bit_clk_khz, true);

    dvi.timing = &dvi_timing_800x480p_60hz;
    dvi.ser_cfg = waveshare_dvi_cfg;
    dvi_init(&dvi, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    multicore_launch_core1(display_video_worker);
}

void display_output_present(const display_framebuffer_t *frame) {
    __dmb();
    pending_frame = frame;
    while (pending_frame != NULL) {
        tight_loop_contents();
    }
    __dmb();
}
