#include "pixels.h"

#include <string.h>

struct led_rgb pixels[STRIP_NUM_PIXELS];

size_t cursor = 0, color = 0;

static const struct led_rgb colors[] = {
    RGB(0x0f, 0x00, 0x00), /* red */
    RGB(0x00, 0x0f, 0x00), /* green */
    RGB(0x00, 0x00, 0x0f), /* blue */
};

// TODO: this will later be used to update just a couple of pixels
int update_pixels(const struct device * const strip)
{
    int rc;

    memset(&pixels, 0x00, sizeof(pixels));
    memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));
    rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

    if (rc) {
        return rc;
    }

    cursor++;
    if (cursor >= STRIP_NUM_PIXELS) {
        cursor = 0;
        color++;
        if (color == ARRAY_SIZE(colors)) {
            color = 0;
        }
    }
    return 0;
}
