/*
 *  BSD 3-Clause License
 *
 * Copyright (c) 2023, Northern Mechatronics, Inc.
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <am_bsp.h>
#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <task.h>

#include "mag.h"

mag_status_t mag_setup(struct bmm350_dev *bmm)
{
    mag_status_t res = MAG_STATUS_OK;
    int8_t rslt;

    bmm350_interface_init(bmm);
    rslt = bmm350_init(bmm);
    if (rslt != BMM350_OK)
    {
        bmm350_error_codes_print_result("bmm350_init", rslt);
        res = MAG_STATUS_ERROR;
        goto error;
    }

    rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_4, bmm);
    if (rslt != BMM350_OK)
    {
        bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);
        res = MAG_STATUS_ERROR;
        goto error;
    }

    rslt = bmm350_enable_axes(BMM350_X_EN, BMM350_Y_EN, BMM350_Z_EN, bmm);
    if (rslt != BMM350_OK)
    {
        bmm350_error_codes_print_result("bmm350_enable_axes", rslt);
        res = MAG_STATUS_ERROR;
        goto error;
    }

    rslt = bmm350_set_powermode(BMM350_NORMAL_MODE, bmm);
    if (rslt != BMM350_OK)
    {
        bmm350_error_codes_print_result("bmm350_set_powermode", rslt);
        res = MAG_STATUS_ERROR;
        goto error;
    }
error:
    bmm350_interface_deinit(bmm);
    return res;
}

void mag_sample(struct bmm350_dev *bmm, mag_context_t *context)
{
    int8_t rslt;
    struct bmm350_mag_temp_data mag_temp_data;

    taskENTER_CRITICAL();
    bmm350_interface_init(bmm);
    rslt = bmm350_get_compensated_mag_xyz_temp_data(&mag_temp_data, bmm);
    bmm350_interface_deinit(bmm);
    taskEXIT_CRITICAL();

    if (rslt != BMM350_OK)
    {
        bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);
        return;
    }

    context->timestamp++;
    context->mx = mag_temp_data.x;
    context->my = mag_temp_data.y;
    context->mz = mag_temp_data.z;
}

void mag_calibrate_step(mag_context_t *context, mag_cal_t *cal_data)
{
    if (context->mx > cal_data->mx_max)
    {
        cal_data->mx_max = context->mx;
    }
    if (context->my > cal_data->my_max)
    {
        cal_data->my_max = context->my;
    }
    if (context->mz > cal_data->mz_max)
    {
        cal_data->mz_max = context->mz;
    }

    if (context->mx < cal_data->mx_min)
    {
        cal_data->mx_min = context->mx;
    }
    if (context->my < cal_data->my_min)
    {
        cal_data->my_min = context->my;
    }
    if (context->mz < cal_data->mz_min)
    {
        cal_data->mz_min = context->mz;
    }

    cal_data->ox = (cal_data->mx_min + cal_data->mx_max) / 2;
    cal_data->oy = (cal_data->my_min + cal_data->my_max) / 2;
    cal_data->oz = (cal_data->mz_min + cal_data->mz_max) / 2;

    float xrange, yrange, zrange, max_range;
    xrange = cal_data->mx_max - cal_data->mx_min;
    yrange = cal_data->my_max - cal_data->my_min;
    zrange = cal_data->mz_max - cal_data->mz_min;
    max_range = xrange;
    if (yrange > max_range)
    {
        max_range = yrange;
    }
    if (zrange > max_range)
    {
        max_range = zrange;
    }

    cal_data->sx = max_range / xrange;
    cal_data->sy = max_range / yrange;
    cal_data->sz = max_range / zrange;
}