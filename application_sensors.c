/*
 *  BSD 3-Clause License
 *
 * Copyright (c) 2024, Northern Mechatronics, Inc.
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

#include "imu.h"
#include "mag.h"

#include "application.h"
#include "application_task.h"

/*
 * We use a hardware timer to minimize OS timer latency.  This is
 * useful for algorithms that are sensitive to clock jitter.
 * 
 * Configure for 100Hz sampling rate
 * Clock Source: HFRC 12kHz
 * Clock Period: 1 / 12e3 = 83us
 */
#define SAMPLING_CLK                AM_HAL_CTIMER_HFRC_12KHZ
#define SAMPLING_TIMER_NUM          2
#define SAMPLING_TIMER_SEG          AM_HAL_CTIMER_TIMERA
#define SAMPLING_TIMER_INT          AM_HAL_CTIMER_INT_TIMERA2C0
#define SAMPLING_CLOCK_PERIOD_US    (83)

static struct bmi2_dev bmi270_handle;
static struct bmm350_dev bmm350_handle;

static void sensor_sample_trigger(void)
{
    // To avoid potential bus contention, we perform the sensor data read in the
    // application task as we need to access the SPI and I2C bus.
    application_msg_t message = { .message = APP_MSG_SAMPLING_TRIGGER, .size = 0, .payload = NULL };
    application_send_message(&message);
}

static void sensor_int1_handler()
{
    uint32_t state;
    application_msg_t message = { 0 };

    // The IMU is configured to generate an interrupt when there is no motion.
    // We stop sampling when there is no motion and resume when motion is detected.
    // 
    // See imu.c for configuration details.
    am_hal_gpio_state_read(AM_BSP_GPIO_IMU_INT1, AM_HAL_GPIO_INPUT_READ, &state);
    if (state)
    {
        message.message = APP_MSG_SAMPLING_START;
    }
    else
    {
        message.message = APP_MSG_SAMPLING_STOP;
    }
    application_send_message(&message);
}


static void application_setup_sensors_sampling_clock(uint32_t sampling_period_ms)
{
    am_hal_ctimer_config_single(
        SAMPLING_TIMER_NUM,
        SAMPLING_TIMER_SEG,
        AM_HAL_CTIMER_FN_PWM_REPEAT |
        AM_HAL_CTIMER_INT_ENABLE |
        SAMPLING_CLK
    );

    uint32_t sampling_period_tick = sampling_period_ms * 1000 / SAMPLING_CLOCK_PERIOD_US;

    am_hal_ctimer_period_set(
        SAMPLING_TIMER_NUM,
        SAMPLING_TIMER_SEG,
        sampling_period_tick, 1);

    am_hal_ctimer_int_register(SAMPLING_TIMER_INT, sensor_sample_trigger);
    am_hal_ctimer_int_enable(SAMPLING_TIMER_INT);
    NVIC_EnableIRQ(CTIMER_IRQn);
}

void application_setup_sensors(uint32_t sampling_period_ms)
{
    imu_status_t imu_status = imu_setup(&bmi270_handle);
    mag_status_t mag_status = mag_setup(&bmm350_handle);
    if (imu_status || mag_status)
    {
        am_util_stdio_printf("\r\nSensor initialization failed.\r\n");
        vTaskSuspend(xTaskGetCurrentTaskHandle());
    }

    imu_int1_register(&bmi270_handle, sensor_int1_handler);
    application_setup_sensors_sampling_clock(sampling_period_ms);
}

void application_sensors_read(imu_context_t *imu_context, mag_context_t *mag_context, mag_cal_t *mag_cal)
{
    imu_sample(&bmi270_handle, imu_context);
    mag_sample(&bmm350_handle, mag_context);
    if (mag_cal)
    {
        mag_context->mx -= mag_cal->ox;
        mag_context->my -= mag_cal->oy;
        mag_context->mz -= mag_cal->oz;
        mag_context->mx *= mag_cal->sx;
        mag_context->my *= mag_cal->sy;
        mag_context->mz *= mag_cal->sz;
    }
}

void application_sensors_start()
{
    am_hal_ctimer_start(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
}

void application_sensors_stop()
{
    am_hal_ctimer_stop(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
}