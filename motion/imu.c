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

static float imu_half_scale;
static float k_acc, kd_gyr, kr_gyr, k_mag;

static struct bmi2_dev bmi270_handle;

static int8_t imu_feature_config_accel(struct bmi2_dev *dev);
static int8_t imu_feature_config_gyro(struct bmi2_dev *dev);
static int8_t imu_feature_config_no_motion(struct bmi2_dev *dev);
static int8_t imu_interrupt_config(struct bmi2_dev *dev);

/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 */
float imu_lsb_to_mps2(int16_t val)
{
    return k_acc * val;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 */
float imu_lsb_to_dps(int16_t val)
{
    return kd_gyr * val;
}

float imu_lsb_to_rps(int16_t val)
{
    return kr_gyr * val;
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
        NVIC_EnableIRQ(GPIO_IRQn);

        int_cfg.pin_type = BMI2_INT1;
        int_cfg.pin_cfg[0].lvl = BMI2_INT_ACTIVE_LOW;
        int_cfg.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
        int_cfg.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
        status = bmi2_set_int_pin_config(&int_cfg, dev);
    }

    return status;
}

imu_status_t imu_setup(struct bmi2_dev *bmi)
{
    imu_status_t res = IMU_STATUS_OK;
    int8_t status;

    bmi2_interface_init(bmi, BMI2_SPI_INTF);

    status = bmi270_init(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    uint8_t regdata = BMI2_ASDA_PUPSEL_10K;
    status = bmi2_set_regs(BMI2_AUX_IF_TRIM, &regdata, 1, bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    uint8_t sensors[4] = {BMI2_ACCEL, BMI2_GYRO, BMI2_NO_MOTION, BMI2_SIG_MOTION};
    status = bmi270_sensor_enable(sensors, 4, bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    status = imu_feature_config_accel(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    status = imu_feature_config_gyro(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    status = imu_feature_config_no_motion(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    struct bmi2_sens_int_config sensor_int_no_motion = {.type = BMI2_NO_MOTION,
                                                        .hw_int_pin = BMI2_INT1};
    status = bmi270_map_feat_int(&sensor_int_no_motion, 1, bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    status = imu_interrupt_config(bmi);
    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        res = IMU_STATUS_ERROR;
        goto error;
    }

    imu_half_scale = ((float)(1 << (bmi->resolution - 1)));
    k_acc = (float)(GRAVITY_EARTH * ACCEL_RANGE) / imu_half_scale;
    kd_gyr = (float)GYRO_RANGE / imu_half_scale;
    kr_gyr = (float)GYRO_RANGE / imu_half_scale * (float)M_PI / 180.0f;

error:
    bmi2_interface_deinit(bmi);

    return res;
}

void imu_int1_register(struct bmi2_dev *bmi, am_hal_gpio_handler_t handler)
{
    am_hal_gpio_interrupt_register(AM_BSP_GPIO_IMU_INT1, handler);
}

void imu_sample(struct bmi2_dev *bmi, imu_context_t *imu_context)
{
    int8_t status = BMI2_OK;
    struct bmi2_sensor_data sensor_data[2];

    // Get the raw sensor data for accelerometer, gyro and magnetometer
    sensor_data[0].type = BMI2_ACCEL;
    sensor_data[1].type = BMI2_GYRO;

    taskENTER_CRITICAL();
    bmi2_interface_init(bmi, BMI2_SPI_INTF);
    status = bmi2_get_sensor_data(sensor_data, 2, bmi);
    bmi2_interface_deinit(bmi);
    taskEXIT_CRITICAL();

    if (status != BMI2_OK)
    {
        bmi2_error_codes_print_result(status);
        return;
    }

    imu_context->timestamp++;
    imu_context->ax = sensor_data[0].sens_data.acc.x;
    imu_context->ay = sensor_data[0].sens_data.acc.y;
    imu_context->az = sensor_data[0].sens_data.acc.z;
    imu_context->gx = sensor_data[1].sens_data.gyr.x;
    imu_context->gy = sensor_data[1].sens_data.gyr.y;
    imu_context->gz = sensor_data[1].sens_data.gyr.z;
}