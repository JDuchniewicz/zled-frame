#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)

#define RGB(_r, _g, _b)                 \
    {                                   \
        .r = (_r), .g = (_g), .b = (_b) \
    }

void start_pixel_update_thread(const struct device *strip);

int display_preset_pattern(const struct device *const strip);
int display_network_image(const struct device *const strip);

extern struct led_rgb pixels[STRIP_NUM_PIXELS];
extern size_t cursor, color;
