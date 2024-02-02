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
#include <math.h>

#include <am_mcu_apollo.h>
#include <am_bsp.h>
#include <am_util.h>

#include <arm_math.h>

#include "imu.h"

#include "alg_shotdetect.h"

#include "application.h"
#include "application_task.h"

bool application_alg_shotdetect_step(imu_context_t *imu_context, alg_shotdetect_context_t *alg_shotdetect_context)
{
    // Strictly speaking, we don't really need to compute the actual force.
    // This is presented for the sake of proper physical representation.
    // One can skip the conversion of the readings to m/s^2 and
    // the square root operation.  Of course, the threshold has to be
    // adjusted accordingly to properly reflect the change in the numerical
    // value.
    float32_t force = 0.0f;
    float32_t ax = imu_lsb_to_mps2(imu_context->ax);
    float32_t ay = imu_lsb_to_mps2(imu_context->ay);
    float32_t az = imu_lsb_to_mps2(imu_context->az);
    arm_sqrt_f32(
        (float32_t)( ax * ax + ay * ay + az * az),
        &force
    );

    alg_shotdetect_sample(alg_shotdetect_context, force);
    return alg_shotdetect_step(alg_shotdetect_context);
}