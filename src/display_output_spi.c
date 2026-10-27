#include "display_output.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define LCD_DC_PIN 8u
#define LCD_CS_PIN 9u
#define LCD_SCK_PIN 10u
#define LCD_MOSI_PIN 11u
#define LCD_RESET_PIN 12u
#define LCD_BACKLIGHT_PIN 13u

#define LCD_SPI spi1
#define LCD_SPI_BAUD_HZ 40000000u

static void lcd_write_command(uint8_t command, const uint8_t *data, size_t length) {
    gpio_put(LCD_CS_PIN, false);
    gpio_put(LCD_DC_PIN, false);
    spi_write_blocking(LCD_SPI, &command, 1u);
    if (length > 0u) {
        gpio_put(LCD_DC_PIN, true);
        spi_write_blocking(LCD_SPI, data, length);
    }
    gpio_put(LCD_CS_PIN, true);
}

static void lcd_set_window(void) {
    const uint8_t columns[] = {
        0x00u, 0x00u,
        (uint8_t)((DISPLAY_WIDTH - 1u) >> 8u),
        (uint8_t)(DISPLAY_WIDTH - 1u)
    };
    const uint8_t rows[] = {
        0x00u, 0x00u,
        (uint8_t)((DISPLAY_HEIGHT - 1u) >> 8u),
        (uint8_t)(DISPLAY_HEIGHT - 1u)
    };
    lcd_write_command(0x2Au, columns, sizeof(columns));
    lcd_write_command(0x2Bu, rows, sizeof(rows));
    lcd_write_command(0x2Cu, NULL, 0u);
}

static void lcd_init(void) {
    spi_init(LCD_SPI, LCD_SPI_BAUD_HZ);
    spi_set_format(LCD_SPI, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(LCD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);

    gpio_init(LCD_DC_PIN);
    gpio_init(LCD_CS_PIN);
    gpio_init(LCD_RESET_PIN);
    gpio_init(LCD_BACKLIGHT_PIN);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_set_dir(LCD_RESET_PIN, GPIO_OUT);
    gpio_set_dir(LCD_BACKLIGHT_PIN, GPIO_OUT);
    gpio_put(LCD_CS_PIN, true);
    gpio_put(LCD_BACKLIGHT_PIN, true);

    gpio_put(LCD_RESET_PIN, true);
    sleep_ms(10u);
    gpio_put(LCD_RESET_PIN, false);
    sleep_ms(10u);
    gpio_put(LCD_RESET_PIN, true);
    sleep_ms(120u);

    lcd_write_command(0x01u, NULL, 0u);
    sleep_ms(150u);
    lcd_write_command(0x11u, NULL, 0u);
    sleep_ms(120u);
    const uint8_t pixel_format = 0x55u;
    const uint8_t orientation = 0x60u;
    lcd_write_command(0x3Au, &pixel_format, 1u);
    lcd_write_command(0x36u, &orientation, 1u);
    lcd_write_command(0x21u, NULL, 0u);
    lcd_write_command(0x13u, NULL, 0u);
    lcd_write_command(0x29u, NULL, 0u);
    sleep_ms(100u);
}

void display_output_start(const display_framebuffer_t *initial_frame) {
    lcd_init();
    display_output_present(initial_frame);
}

void display_output_present(const display_framebuffer_t *frame) {
    uint8_t row[DISPLAY_WIDTH * 2u];
    lcd_set_window();

    gpio_put(LCD_CS_PIN, false);
    gpio_put(LCD_DC_PIN, true);
    for (uint y = 0; y < DISPLAY_HEIGHT; ++y) {
        const uint8_t *packed_row = &frame->pixels[y * DISPLAY_WIDTH / 8u];
        for (uint x = 0; x < DISPLAY_WIDTH; ++x) {
            const uint8_t on = (packed_row[x / 8u] >> (x % 8u)) & 0x01u;
            const uint8_t pixel = on ? 0xFFu : 0x00u;
            row[x * 2u] = pixel;
            row[x * 2u + 1u] = pixel;
        }
        spi_write_blocking(LCD_SPI, row, sizeof(row));
    }
    gpio_put(LCD_CS_PIN, true);
}
