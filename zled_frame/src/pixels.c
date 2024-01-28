#include "pixels.h"
#include "network.h"

#include <zephyr/kernel.h>

#include <string.h>

#include <zephyr/logging/log.h>
#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(pixels); // TODO: how does it work?

struct led_rgb pixels[STRIP_NUM_PIXELS];

size_t cursor = 0, color = 0;
const int mode = 1; // 0 - preset pattern, 1 - network

static const struct led_rgb colors[] = {
    RGB(0x0f, 0x00, 0x00), /* red */
    RGB(0x00, 0x0f, 0x00), /* green */
    RGB(0x00, 0x00, 0x0f), /* blue */
};

#define STACKSIZE 1024
#define PRIORITY 7

K_THREAD_STACK_DEFINE(pixel_update_thread_stack, STACKSIZE);
struct k_thread pixel_update_thread_data;

void pixel_update_thread(void *arg1, void *arg2, void *arg3)
{
    const struct device *strip = arg1;

    while (1)
    {
        if (mode == 0)
        {
            display_preset_pattern(strip);
        }
        else
        {
            display_network_image(strip);
        }
        k_sleep(K_MSEC(1)); // Sleep for a short time to prevent busy waiting
    }
}

void start_pixel_update_thread(const struct device *strip)
{
    k_thread_create(&pixel_update_thread_data, pixel_update_thread_stack,
                    K_THREAD_STACK_SIZEOF(pixel_update_thread_stack),
                    pixel_update_thread,
                    (void *)strip, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
}

int display_preset_pattern(const struct device *const strip)
{
    int rc;

    memset(&pixels, 0x00, sizeof(pixels));
    memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));
    rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

    if (rc)
    {
        return rc;
    }

    cursor++;
    if (cursor >= STRIP_NUM_PIXELS)
    {
        cursor = 0;
        color++;
        if (color == ARRAY_SIZE(colors))
        {
            color = 0;
        }
    }
    return 0;
}

int display_network_image(const struct device *const strip)
{
    int rc;

    // Wait for the semaphore indicating a new image is ready
    if (k_sem_take(&image_semaphore, K_NO_WAIT) == 0)
    {
        LOG_ERR("Received image - semaphore taken"); // <- this should run as oneshot
        //  Copy the received image to the pixels array
        //  TODO: uncomment it only when I am sure the proper data is transferred
        //  memcpy(pixels, received_image, sizeof(pixels));

        // Update the LED strip with the new image
        rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

        if (rc)
        {
            return rc;
        }
        LOG_ERR("Finished processing bro"); // <- this should run as oneshot
    }

    return 0;
}