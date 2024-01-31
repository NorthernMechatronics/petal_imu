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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <am_bsp.h>
#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include <am_bsp.h>
#include <bmi270.h>
#include <bmi270_hal.h>

#include "imu.h"

#define GRAVITY_EARTH (9.80665f)

#define BMI2_RESOLVE(V) V

#define BMI2_ACC_RANGE_RESOLVE_(V)  BMI2_RESOLVE(BMI2_ACC_RANGE_##V##G)
#define BMI2_ACC_RANGE_RESOLVE(var) BMI2_ACC_RANGE_RESOLVE_(var)

#define BMI2_GYR_RANGE_RESOLVE_(V)  BMI2_RESOLVE(BMI2_GYR_RANGE_##V)
#define BMI2_GYR_RANGE_RESOLVE(var) BMI2_GYR_RANGE_RESOLVE_(var)

#define ACCEL_RANGE      16
#define ACCEL_RANGE_BMI2 BMI2_ACC_RANGE_RESOLVE(ACCEL_RANGE)

#define GYRO_RANGE      2000
#define GYRO_RANGE_BMI2 BMI2_GYR_RANGE_RESOLVE(GYRO_RANGE)

/*
 * Configure for 100Hz sampling rate
 * Clock Source: HFRC 12kHz
 * Clock Period: 1 / 12e3 = 83us
 * Sampling Period: 1 / 100 = 10.0ms
 * Sampling Clock Tick Count: 10.0ms / 83us = 120
 */
#define SAMPLING_CLK            AM_HAL_CTIMER_HFRC_12KHZ
#define SAMPLING_TIMER_NUM      2
#define SAMPLING_TIMER_SEG      AM_HAL_CTIMER_TIMERA
#define SAMPLING_TIMER_INT      AM_HAL_CTIMER_INT_TIMERA2C0
#define SAMPLING_PERIOD_TICK    (120)

static SemaphoreHandle_t imu_context_mutex;
static imu_context_t imu_context;
static uint32_t imu_context_count;
static struct bmi2_dev bmi270_handle;
static uint32_t imu_sampling_force_on;

static int8_t imu_feature_config_accel(struct bmi2_dev *dev);
static int8_t imu_feature_config_gyro(struct bmi2_dev *dev);
static int8_t imu_feature_config_no_motion(struct bmi2_dev *dev);
static int8_t imu_interrupt_config(struct bmi2_dev *dev);
static void imu_setup_sensor(struct bmi2_dev *bmi);
static void imu_setup_sampling(void);
static void imu_sample(struct bmi2_dev *bmi);

static float imu_half_scale, k_acc, kd_gyr, kr_gyr, k_mag;

/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 */
float lsb_to_mps2(int16_t val)
{
    return k_acc * val;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 */
float lsb_to_dps(int16_t val)
{
    return kd_gyr * val;
}

float lsb_to_rps(int16_t val)
{
    return kr_gyr * val;
}

static void imu_interrupt_handler_no_motion(void)
{
    uint32_t state;
    am_hal_gpio_state_read(AM_BSP_GPIO_IMU_INT1, AM_HAL_GPIO_INPUT_READ, &state);
    if (state)
    {
        am_hal_ctimer_start(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
        am_hal_gpio_state_write(AM_BSP_GPIO_LED1, AM_HAL_GPIO_OUTPUT_SET);
    }
    else
    {
        if (imu_sampling_force_on != true)
        {
            am_hal_ctimer_stop(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
            am_hal_gpio_state_write(AM_BSP_GPIO_LED1, AM_HAL_GPIO_OUTPUT_CLEAR);
        }
    }
}

static int8_t imu_feature_config_accel(struct bmi2_dev *dev)
{
    int8_t status = BMI2_OK;
    struct bmi2_sens_config config;

    config.type = BMI2_ACCEL;
    status = bmi270_get_sensor_config(&config, 1, dev);
    if (status == BMI2_OK)
    {
        config.cfg.acc.odr = BMI2_ACC_ODR_400HZ;
        config.cfg.acc.range = ACCEL_RANGE_BMI2;
        config.cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
        config.cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
        status = bmi270_set_sensor_config(&config, 1, dev);
    }

    return status;
}

static int8_t imu_feature_config_gyro(struct bmi2_dev *dev)
{
    int8_t status = BMI2_OK;
    struct bmi2_sens_config config;

    config.type = BMI2_GYRO;
    status = bmi270_get_sensor_config(&config, 1, dev);
    if (status == BMI2_OK)
    {
        /* Default is 200Hz.  Set to 400Hz */
        config.cfg.gyr.odr = BMI2_GYR_ODR_400HZ;
        config.cfg.gyr.range = GYRO_RANGE_BMI2;
        config.cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
        config.cfg.gyr.noise_perf = BMI2_PERF_OPT_MODE;
        config.cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
        status = bmi270_set_sensor_config(&config, 1, dev);
    }

    return status;
}

static int8_t imu_feature_config_no_motion(struct bmi2_dev *dev)
{
    int8_t status = BMI2_OK;
    struct bmi2_sens_config config;

    config.type = BMI2_NO_MOTION;
    status = bmi270_get_sensor_config(&config, 1, dev);
    if (status == BMI2_OK)
    {
        /* 1LSB equals 20ms. Default is 100ms. Set to 2 seconds. */
        config.cfg.no_motion.duration = 150;
        /* 1LSB equals to 0.48mg. Default is 83mg. Set to 16.8mg. */
        config.cfg.no_motion.threshold = 35;
        status = bmi270_set_sensor_config(&config, 1, dev);
    }

    return status;
}

static int8_t imu_interrupt_config(struct bmi2_dev *dev)
{
    int8_t status = BMI2_OK;

    struct bmi2_int_pin_config int_cfg;
    status = bmi2_get_int_pin_config(&int_cfg, dev);
    if (status == BMI2_OK)
    {
        uint64_t mask_int1;
        AM_HAL_GPIO_MASKBIT(mask_int1, AM_BSP_GPIO_IMU_INT1);
        am_hal_gpio_pinconfig(AM_BSP_GPIO_IMU_INT1, g_AM_BSP_GPIO_IMU_INT1);
        am_hal_gpio_interrupt_clear(mask_int1);
        am_hal_gpio_interrupt_enable(mask_int1);
        // FIXME
        // am_hal_gpio_interrupt_register(AM_BSP_GPIO_IMU_INT1, imu_interrupt_handler_no_motion);
        NVIC_EnableIRQ(GPIO_IRQn);

        int_cfg.pin_type = BMI2_INT1;
        int_cfg.pin_cfg[0].lvl = BMI2_INT_ACTIVE_LOW;
        int_cfg.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
        int_cfg.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
        status = bmi2_set_int_pin_config(&int_cfg, dev);
    }

    return status;
}

static void imu_setup_sensor(struct bmi2_dev *bmi)
{
    int8_t status;

    status = bmi270_init(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    uint8_t regdata = BMI2_ASDA_PUPSEL_10K;
    status = bmi2_set_regs(BMI2_AUX_IF_TRIM, &regdata, 1, bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    uint8_t sensors[4] = {BMI2_ACCEL, BMI2_GYRO, BMI2_NO_MOTION, BMI2_SIG_MOTION};
    status = bmi270_sensor_enable(sensors, 4, bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    status = imu_feature_config_accel(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    status = imu_feature_config_gyro(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    status = imu_feature_config_no_motion(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    struct bmi2_sens_int_config sensor_int_no_motion = {.type = BMI2_NO_MOTION,
                                                        .hw_int_pin = BMI2_INT1};
    status = bmi270_map_feat_int(&sensor_int_no_motion, 1, bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    status = imu_interrupt_config(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    memset(&imu_context, 0, sizeof(imu_context_t));
    imu_context_count = 0;
    imu_half_scale = ((float)(1 << (bmi->resolution - 1)));
    k_acc = (float)(GRAVITY_EARTH * ACCEL_RANGE) / imu_half_scale;
    kd_gyr = (float)GYRO_RANGE / imu_half_scale;
    kr_gyr = (float)GYRO_RANGE / imu_half_scale * (float)M_PI / 180.0f;
}

static void imu_setup_sampling()
{
    am_hal_ctimer_config_single(
        SAMPLING_TIMER_NUM,
        SAMPLING_TIMER_SEG,
        AM_HAL_CTIMER_FN_PWM_REPEAT |
        AM_HAL_CTIMER_INT_ENABLE |
        SAMPLING_CLK
    );

    am_hal_ctimer_period_set(
        SAMPLING_TIMER_NUM,
        SAMPLING_TIMER_SEG,
        SAMPLING_PERIOD_TICK, 1);

    // FIXME
    // am_hal_ctimer_int_register(SAMPLING_TIMER_INT, imu_sampling_handler);
    am_hal_ctimer_int_enable(SAMPLING_TIMER_INT);
    NVIC_EnableIRQ(CTIMER_IRQn);

    imu_sampling_force_on = false;
    am_hal_ctimer_start(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
}

static void imu_sample(struct bmi2_dev *bmi)
{
    int8_t status = BMI2_OK;
    struct bmi2_sensor_data sensor_data[2];

    // Get the raw sensor data for accelerometer, gyro and magnetometer
    sensor_data[0].type = BMI2_ACCEL;
    sensor_data[1].type = BMI2_GYRO;

    taskENTER_CRITICAL();
    bmi2_interface_init(&bmi270_handle, BMI2_SPI_INTF);
    status = bmi2_get_sensor_data(sensor_data, 2, bmi);
    bmi2_interface_deinit(&bmi270_handle);
    taskEXIT_CRITICAL();

    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    // Store the sample data
    xSemaphoreTake(imu_context_mutex, pdMS_TO_TICKS(5));

    imu_context.timestamp = imu_context_count++;

    imu_context.ax = sensor_data[0].sens_data.acc.x;
    imu_context.ay = sensor_data[0].sens_data.acc.y;
    imu_context.az = sensor_data[0].sens_data.acc.z;
    imu_context.gx = sensor_data[1].sens_data.gyr.x;
    imu_context.gy = sensor_data[1].sens_data.gyr.y;
    imu_context.gz = sensor_data[1].sens_data.gyr.z;

    xSemaphoreGive(imu_context_mutex);
}

void imu_context_read(imu_context_t *context)
{
    if (context)
    {
        xSemaphoreTake(imu_context_mutex, pdMS_TO_TICKS(10));

        *context = imu_context;

        xSemaphoreGive(imu_context_mutex);
    }
}

void imu_sampling_mode(imu_sampling_mode_t mode)
{
    switch(mode)
    {
    case IMU_SAMPLING_MODE_AUTO:
        imu_sampling_force_on = false;
        am_hal_ctimer_start(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
        break;

    case IMU_SAMPLING_MODE_OFF:
        imu_sampling_force_on = false;
        am_hal_ctimer_stop(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
        break;

    case IMU_SAMPLING_MODE_ON:
        imu_sampling_force_on = true;
        am_hal_ctimer_start(SAMPLING_TIMER_NUM, SAMPLING_TIMER_SEG);
        break;

    default:
        break;
    }
}