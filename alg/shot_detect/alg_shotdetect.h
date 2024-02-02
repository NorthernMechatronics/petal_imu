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

#ifndef _ALG_SHOTDETECT_H_
#define _ALG_SHOTDETECT_H_

#include <stdint.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>

#include <arm_math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ALG_SHOTDETECT_IDLE,
    ALG_SHOTDETECT_IN_PROGRESS,
    ALG_SHOTDETECT_COMPLETED,
} alg_shotdetect_state_t;

typedef struct alg_shotdetect_context_s
{
    alg_shotdetect_state_t state;
    uint32_t trigger_threshold;
    uint32_t idle_threshold;
    uint32_t sampling_period_ms;
    float32_t *reference_signal;
    float32_t *sampled_signal;
    float32_t *convolved_signal;
    uint32_t signal_length;
    float32_t energy_transferred;
} alg_shotdetect_context_t;

void alg_shotdetect_sample(alg_shotdetect_context_t *cfg, float32_t new_sample);
bool alg_shotdetect_step(alg_shotdetect_context_t *cfg);

#ifdef __cplusplus
}
#endif

#endif