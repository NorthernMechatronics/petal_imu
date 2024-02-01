/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2022, Northern Mechatronics, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <math.h>

#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

#include "am_bsp.h"

#ifdef RAT_LORAWAN_ENABLE
#include "lorawan.h"
#endif

#ifdef RAT_BLE_ENABLE
#include "ble.h"
#endif

#include "imu.h"
#include "mag.h"

#include "button.h"

#include "application.h"
#include "application_task.h"
#include "application_task_cli.h"

#define LED_BLINK_NORMAL    500
#define LED_BLINK_QUICK      100

typedef enum {
    APP_STATE_NORMAL,
    APP_STATE_CALIBRATION
} application_state_t;

static application_state_t application_state;

static uint32_t led_blink_period_ms;
static uint32_t sampling_always_on;

static TaskHandle_t application_task_handle;
static QueueHandle_t application_queue_handle;
static TimerHandle_t application_timer_handle;

static imu_context_t imu_context;
static mag_context_t mag_context;
static mag_cal_t mag_cal;

static void application_led_timer_callback(TimerHandle_t timer)
{
    application_msg_t message = { .message = APP_MSG_LED_STATUS, .size = 0, .payload = NULL };
    application_send_message(&message);
}

static void application_calibration_start()
{
    application_msg_t message = { .message = APP_MSG_CALIBRATE_START, .size = 0, .payload = NULL };
    application_send_message(&message);
    led_blink_period_ms = LED_BLINK_QUICK;
    xTimerChangePeriod(application_timer_handle, pdMS_TO_TICKS(led_blink_period_ms), portMAX_DELAY);
}

static void application_calibration_stop()
{
    application_msg_t message = { .message = APP_MSG_CALIBRATE_STOP, .size = 0, .payload = NULL };
    application_send_message(&message);
    led_blink_period_ms = LED_BLINK_NORMAL;
    xTimerChangePeriod(application_timer_handle, pdMS_TO_TICKS(led_blink_period_ms), portMAX_DELAY);
}

static void application_toggle_always_on()
{
    sampling_always_on ^= 1;
    am_util_stdio_printf("Sampling Always On: %d\r\n", sampling_always_on);
}

static void application_setup_task()
{
    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED0, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED0, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED1, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED1, AM_HAL_GPIO_OUTPUT_CLEAR);

// Uncomment if running on the Petal Development Board.
// Note that these LEDs do not exist on the Petal Core or the
// Petal IMU boards.
#if defined(BSP_NM180410)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED2, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED2, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED3, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED3, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED4, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED4, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_IO_EN, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_IO_EN, AM_HAL_GPIO_OUTPUT_SET);
#endif

    application_lfs_init();
    application_lfs_load_cal(&mag_cal);
    application_lfs_deinit();

    am_util_stdio_printf("Calibration State: %d\r\n", mag_cal.initialised);
    am_util_stdio_printf("Hard Iron Offsets: %4.2f, %4.2f, %4.2f\r\n",
        (double)mag_cal.ox,
        (double)mag_cal.oy,
        (double)mag_cal.oz);

    application_state = APP_STATE_NORMAL;
    sampling_always_on = 0;

    // three short presses to start calibration
    button_sequence_register(3, 0B000, application_calibration_start);
    // one long press to end calibration
    button_sequence_register(1, 0B1, application_calibration_stop);
    // two short presses to toggle always on
    button_sequence_register(2, 0B00, application_toggle_always_on);

    xTimerStart(application_timer_handle, portMAX_DELAY);
}

float application_sensors_heading(float x, float y)
{
    float heading = atan2f(y, x) * 180.0f / (float) M_PI;
    if (heading < 0.0f)
        heading += 360.0f;

    if (heading > 360.0f)
        heading -= 360.0f;

    return heading;
}


static void application_task(void *parameter)
{
    application_task_cli_register();

#ifdef RAT_LORAWAN_ENABLE
    application_setup_lorawan();
#endif
    
#ifdef RAT_BLE_ENABLE
    application_setup_ble();
#endif

    application_setup_task();

    // Set sampling rate to 100Hz
    // Period is 1 / 100Hz = 10ms
    application_setup_sensors(10);
    application_sensors_start();
    while (1)
    {
        application_msg_t message;
        if (xQueueReceive(application_queue_handle, &message, portMAX_DELAY) == pdTRUE)
        {
            switch(message.message)
            {
            case APP_MSG_LED_STATUS:
                am_hal_gpio_state_write(AM_BSP_GPIO_LED0, AM_HAL_GPIO_OUTPUT_TOGGLE);
                if (application_state == APP_STATE_CALIBRATION)
                {
                    am_util_stdio_printf(
                        "Offsets: %4.2f, %4.2f, %4.2f  " \
                        "Min: %4.2f, %4.2f, %4.2f  " \
                        "Max: %4.2f, %4.2f, %4.2f  " \
                        "Range: %4.2f, %4.2f, %4.2f  " \
                        "      \r",
                        (double)mag_cal.ox,
                        (double)mag_cal.oy,
                        (double)mag_cal.oz,
                        (double)mag_cal.mx_min,
                        (double)mag_cal.my_min,
                        (double)mag_cal.mz_min,
                        (double)mag_cal.mx_max,
                        (double)mag_cal.my_max,
                        (double)mag_cal.mz_max,
                        (double)(mag_cal.mx_max - mag_cal.mx_min),
                        (double)(mag_cal.my_max - mag_cal.my_min),
                        (double)(mag_cal.mz_max - mag_cal.mz_min)
                        );
                } 
                else
                {
                }
                break;

            case APP_MSG_SAMPLING_TRIGGER:
                if (application_state == APP_STATE_CALIBRATION)
                {
                    application_sensors_read(&imu_context, &mag_context, NULL);
                    mag_calibrate_step(&mag_context, &mag_cal);
                }
                else
                {
                    application_sensors_read(&imu_context, &mag_context, &mag_cal);
                    // Run your custom algorithms here.
                }
                break;

            case APP_MSG_SAMPLING_START:
                application_sensors_start();
                if (sampling_always_on == 0)
                {
                    am_util_stdio_printf("Motion detected.  Sampling...\r\n");
                }
                break;

            case APP_MSG_SAMPLING_STOP:
                // Keep sampling if we are calibrating. 
                if ((application_state != APP_STATE_CALIBRATION) && (sampling_always_on == 0))
                {
                    application_sensors_stop();
                    am_util_stdio_printf("No motion detected.  Sampling paused...\r\n");
                }
                break;

            case APP_MSG_CALIBRATE_START:
                application_state = APP_STATE_CALIBRATION;
                memset(&mag_cal, 0, sizeof(mag_cal_t));

                am_util_stdio_printf("Calibration started.  Long press to exit when done.\r\n");
                break;

            case APP_MSG_CALIBRATE_STOP:
                mag_cal.initialised = 1;
                application_lfs_init();
                application_lfs_write_cal(&mag_cal);
                application_lfs_deinit();
                application_state = APP_STATE_NORMAL;

                am_util_stdio_printf("Calibration completed.\r\n");
                am_util_stdio_printf("Calibration State: %d\r\n", mag_cal.initialised);
                am_util_stdio_printf("Hard Iron Offsets: %4.2f, %4.2f, %4.2f\r\n",
                    (double)mag_cal.ox,
                    (double)mag_cal.oy,
                    (double)mag_cal.oz);
                break;
            }
        }
    }
}

void application_send_message(application_msg_t *message)
{
    if (xPortIsInsideInterrupt() == pdTRUE)
    {
        BaseType_t context_switch = pdFALSE;
        xQueueSendFromISR(application_queue_handle, message, &context_switch);
        portYIELD_FROM_ISR(context_switch);
    }
    else
    {
        xQueueSend(application_queue_handle, message, portMAX_DELAY);
    }
}

void application_task_create(uint32_t priority)
{
    led_blink_period_ms = LED_BLINK_NORMAL;
    xTaskCreate(application_task, "application", 512, 0, priority, &application_task_handle);
    application_queue_handle = xQueueCreate(10, sizeof(application_msg_t));
    application_timer_handle = xTimerCreate("LED Status", pdMS_TO_TICKS(led_blink_period_ms), pdTRUE, NULL, application_led_timer_callback);
}