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

#include <am_mcu_apollo.h>

#include "alg_shotdetect.h"

void alg_shotdetect_sample(alg_shotdetect_context_t *context, float32_t new_sample)
{
    int i;
    for (i = 0; i < context->signal_length - 1; i++)
    {
        context->sampled_signal[i] = context->sampled_signal[i + 1];
    }
    context->sampled_signal[i] = new_sample;
}

bool alg_shotdetect_step(alg_shotdetect_context_t *context)
{
    bool ret = false;

    arm_conv_f32(
        context->reference_signal, context->signal_length,
        context->sampled_signal, context->signal_length,
        context->convolved_signal
    );

    context->energy_transferred = 0.0f;
    uint32_t offset = context->signal_length >> 1;
    for (int i = offset; i < context->signal_length + offset; i++)
    {
        context->energy_transferred += context->convolved_signal[i];
    }

    switch (context->state)
    {
    case ALG_SHOTDETECT_IDLE:
        if (context->energy_transferred > context->trigger_threshold)
        {
            context->state = ALG_SHOTDETECT_IN_PROGRESS;
        }
        break;

    case ALG_SHOTDETECT_IN_PROGRESS:
        if (context->energy_transferred < context->trigger_threshold)
        {
            context->state = ALG_SHOTDETECT_COMPLETED;
        }
        break;
    
    case ALG_SHOTDETECT_COMPLETED:
        if (context->energy_transferred < context->idle_threshold)
        {
            context->state = ALG_SHOTDETECT_IDLE;
            ret = true;
        }
        break;

    default:
        break;
    }

    return ret;
}