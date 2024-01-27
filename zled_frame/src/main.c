/*
 * Copyright (c) 2024 Jakub Duchniewicz
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include "pixels.h"
#include "network.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_ZLED_FRAME_LOG_LEVEL); // TODO: how does it work? Can have just one instance?

#define STRIP_NODE  DT_ALIAS(led_strip)

#define UPDATE_DELAY K_MSEC(50) /* in ms */

static const struct device * const strip = DEVICE_DT_GET(STRIP_NODE);

int main(void)
{
    int rc;

    if (device_is_ready(strip)) {
        LOG_INF("Found LED strip device %s", strip->name);
    } else {
        LOG_INF("LED strip device %s is not ready", strip->name);
        return 0;
    }

    LOG_INF("Starting networking threads");
	start_listener();

    LOG_INF("Displaying pattern on strip");

    // TODO: add network error propagation? Hard reset of the board?
    // Move the led updating code to another thread and get the data from network
    // cycle the colors on the strip
    while (1) {
        rc = update_pixels(strip);

        if (rc) {
            LOG_ERR("could not update strip: %d", rc);
        }
        k_sleep(UPDATE_DELAY);
    }

	return 0;
}

