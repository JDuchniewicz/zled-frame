#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_LINE_LENGTH (16)

#define RGB(_r, _g, _b)                 \
    (struct led_rgb)                    \
    {                                   \
        .r = (_r), .g = (_g), .b = (_b) \
    }

void start_pixel_update_thread(const struct device *strip);

extern struct led_rgb pixels[STRIP_NUM_PIXELS];
extern size_t cursor, color;
