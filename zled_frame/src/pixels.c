#include "pixels.h"
#include "network.h"

#include <zephyr/kernel.h>

#include <string.h>

#include <zephyr/logging/log.h>
#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(pixels); // TODO: how does it work?

int display_preset_pattern(const struct device *const strip);
int display_network_image(const struct device *const strip);
int display_calibration_pattern(const struct device *const strip);

struct led_rgb pixels[STRIP_NUM_PIXELS];

size_t cursor = 0, color = 0;
// TODO: later can trigger it from the web?
const int mode = 2; // 0 - preset pattern, 1 - network, 2 - calibration pattern

static const struct led_rgb colors[] = {
    RGB(0xff, 0x00, 0x00), /* red */
    RGB(0x00, 0xff, 0x00), /* green */
    RGB(0x00, 0x00, 0xff), /* blue */
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
            k_sleep(K_MSEC(5000));
        }
        else if (mode == 1)
        {
            display_network_image(strip);
        }
        else if (mode == 2)
        {
            display_calibration_pattern(strip);
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

int display_calibration_pattern(const struct device *const strip)
{
    // several patterns here - first rising brightness, then falling, then rainbow
    int rc;

    memset(pixels, 0x00, sizeof(pixels));

    // rising brightness
    for (int i = 0; i < STRIP_NUM_PIXELS; ++i)
    {
        pixels[i] = RGB(i, i, i);
    }
    rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    if (rc)
    {
        return rc;
    }
    k_sleep(K_MSEC(5000));

    // falling brightness
    for (int i = 0; i < STRIP_NUM_PIXELS; ++i)
    {
        pixels[i] = RGB(STRIP_NUM_PIXELS - i, STRIP_NUM_PIXELS - i, STRIP_NUM_PIXELS - i);
    }
    rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    if (rc)
    {
        return rc;
    }
    k_sleep(K_MSEC(5000));

    // rainbow
    for (int i = 0; i < STRIP_NUM_PIXELS; ++i)
    {
        pixels[i] = RGB(i, STRIP_NUM_PIXELS - i, STRIP_NUM_PIXELS - i);
    }
    rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    if (rc)
    {
        return rc;
    }
    k_sleep(K_MSEC(5000));

    return 0;
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
        LOG_ERR("Received image - semaphore taken");
        //  Copy the received image to the pixels array
        // print the received image
        /*
        for (int i = 0; i < STRIP_NUM_PIXELS / 16; i++)
        {
            LOG_ERR("Received image - pixel %d: %d %d %d", i, received_image[i * 3], received_image[i * 3 + 1], received_image[i * 3 + 2]);
            if (i % 16)
                k_sleep(K_MSEC(3)); // sleep to prevent logs from being dropped
        }
        */
        memset(pixels, 0x00, sizeof(pixels));
        // TODO: optimize the algo
        // convert pixel data to led_rgb
        size_t index = 0;
        for (int i = 0; i < STRIP_LINE_LENGTH; ++i)
        {
            for (int j = 0; j < STRIP_LINE_LENGTH; ++j)
            {
                // since the led strip is sequential, we have to reverse every second line
                if (i % 2 == 0)
                {
                    pixels[index] = RGB(received_image[index * 3], received_image[index * 3 + 1], received_image[index * 3 + 2]);
                }
                else
                {
                    size_t reversed_index = i * STRIP_LINE_LENGTH + STRIP_LINE_LENGTH - j - 1;
                    pixels[index] = RGB(received_image[reversed_index * 3], received_image[reversed_index * 3 + 1], received_image[reversed_index * 3 + 2]);
                }
                if (index < 16)
                    LOG_ERR("Set pixel %d: %d %d %d", index, pixels[index].r, pixels[index].g, pixels[index].b);
                ++index;
            }
        }

        // Update the LED strip with the new image
        rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

        if (rc)
        {
            return rc;
        }
        LOG_ERR("Finished image drawing");
    }

    return 0;
}