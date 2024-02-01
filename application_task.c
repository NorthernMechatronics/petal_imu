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

static uint32_t led_blink_period_ms;

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

static void application_calibration_state_toggle()
{
    if (led_blink_period_ms == LED_BLINK_NORMAL)
    {
        led_blink_period_ms = LED_BLINK_QUICK;
    }
    else
    {
        led_blink_period_ms = LED_BLINK_NORMAL;
    }

    xTimerChangePeriod(application_timer_handle, pdMS_TO_TICKS(led_blink_period_ms), portMAX_DELAY);
}

static void application_setup_task()
{
    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED0, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED0, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED1, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED1, AM_HAL_GPIO_OUTPUT_CLEAR);

#if defined(BSP_NM180100EVB) || defined(BSP_NM180410)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED2, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED2, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED3, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED3, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED4, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED4, AM_HAL_GPIO_OUTPUT_CLEAR);
#endif

#if defined(BSP_NM180410)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_IO_EN, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_IO_EN, AM_HAL_GPIO_OUTPUT_SET);
#endif

    application_lfs_init();
    application_lfs_load_cal(&mag_cal);
    application_lfs_deinit();

    am_util_stdio_printf("Calibration Data: %d\r\n", mag_cal.initialised);

    button_sequence_register(1, 0B0, application_calibration_state_toggle);
    xTimerStart(application_timer_handle, portMAX_DELAY);
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
                break;

            case APP_MSG_SAMPLING_TRIGGER:
                application_sensors_read(&imu_context, &mag_context);
                break;

            case APP_MSG_SAMPLING_START:
                application_sensors_start();
                am_util_stdio_printf("Motion detected.  Sampling...\r\n");
                break;

            case APP_MSG_SAMPLING_STOP:
                application_sensors_stop();
                am_util_stdio_printf("No motion detected.  Sampling Paused...\r\n");
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