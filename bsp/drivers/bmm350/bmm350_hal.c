/**
* Copyright (c) 2023 Bosch Sensortec GmbH. All rights reserved.
*
* BSD-3-Clause
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* @file  common.c
*
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <am_mcu_apollo.h>
#include <am_util.h>
#include <am_bsp.h>

#include "bmm350.h"
#include "bmm350_hal.h"

/******************************************************************************/
/*!                Structure definition                                       */
#define BMM350_IOM_MODULE  (1)

/******************************************************************************/
/*!                Static variable definition                                 */
static void    *iom_handle;
static uint8_t dev_addr;

static am_hal_iom_config_t iom_i2c_config =
{
    .eInterfaceMode  = AM_HAL_IOM_I2C_MODE,
    .ui32ClockFreq   = AM_HAL_IOM_400KHZ,
};

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to COINES platform
 */
BMM350_INTF_RET_TYPE bmm350_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    am_hal_iom_transfer_t  transaction;
    uint8_t device_addr = *(uint8_t*)intf_ptr;

    transaction.uPeerInfo.ui32I2CDevAddr = device_addr;
    transaction.ui8Priority = 1;
    transaction.eDirection = AM_HAL_IOM_RX;
    transaction.ui32InstrLen = 1;
    transaction.ui32Instr = reg_addr;
    transaction.ui32NumBytes = length;
    transaction.pui32RxBuffer = (uint32_t *)reg_data;
    transaction.ui8RepeatCount = 0;
    transaction.ui32PauseCondition = 0;
    transaction.ui32StatusSetClr = 0;
    transaction.bContinue = false;

    return am_hal_iom_blocking_transfer(iom_handle, &transaction);
}

/*!
 * I2C write function map to COINES platform
 */
BMM350_INTF_RET_TYPE bmm350_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    am_hal_iom_transfer_t  transaction;
    uint8_t device_addr = *(uint8_t*)intf_ptr;

    transaction.uPeerInfo.ui32I2CDevAddr = device_addr;
    transaction.ui8Priority = 1;
    transaction.eDirection = AM_HAL_IOM_TX;
    transaction.ui32InstrLen = 1;
    transaction.ui32Instr = reg_addr;
    transaction.ui32NumBytes = length;
    transaction.pui32TxBuffer = (uint32_t *)reg_data;
    transaction.ui8RepeatCount = 0;
    transaction.ui32PauseCondition = 0;
    transaction.ui32StatusSetClr = 0;
    transaction.bContinue = false;

    return am_hal_iom_blocking_transfer(iom_handle, &transaction);
}

/*!
 * Delay function map to COINES platform
 */
void bmm350_delay(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    am_util_delay_us(period);
}

/*!
 *  @brief Prints the execution status of the APIs.
 */
void bmm350_error_codes_print_result(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
        case BMM350_OK:
            break;

        case BMM350_E_NULL_PTR:
            am_util_stdio_printf("%s Error [%d] : Null pointer\r\n", api_name, rslt);
            break;
        case BMM350_E_COM_FAIL:
            am_util_stdio_printf("%s Error [%d] : Communication fail\r\n", api_name, rslt);
            break;
        case BMM350_E_DEV_NOT_FOUND:
            am_util_stdio_printf("%s Error [%d] : Device not found\r\n", api_name, rslt);
            break;
        case BMM350_E_INVALID_CONFIG:
            am_util_stdio_printf("%s Error [%d] : Invalid configuration\r\n", api_name, rslt);
            break;
        case BMM350_E_BAD_PAD_DRIVE:
            am_util_stdio_printf("%s Error [%d] : Bad pad drive\r\n", api_name, rslt);
            break;
        case BMM350_E_RESET_UNFINISHED:
            am_util_stdio_printf("%s Error [%d] : Reset unfinished\r\n", api_name, rslt);
            break;
        case BMM350_E_INVALID_INPUT:
            am_util_stdio_printf("%s Error [%d] : Invalid input\r\n", api_name, rslt);
            break;
        case BMM350_E_SELF_TEST_INVALID_AXIS:
            am_util_stdio_printf("%s Error [%d] : Self-test invalid axis selection\r\n", api_name, rslt);
            break;
        case BMM350_E_OTP_BOOT:
            am_util_stdio_printf("%s Error [%d] : OTP boot\r\n", api_name, rslt);
            break;
        case BMM350_E_OTP_PAGE_RD:
            am_util_stdio_printf("%s Error [%d] : OTP page read\r\n", api_name, rslt);
            break;
        case BMM350_E_OTP_PAGE_PRG:
            am_util_stdio_printf("%s Error [%d] : OTP page prog\r\n", api_name, rslt);
            break;
        case BMM350_E_OTP_SIGN:
            am_util_stdio_printf("%s Error [%d] : OTP sign\r\n", api_name, rslt);
            break;
        case BMM350_E_OTP_INV_CMD:
            am_util_stdio_printf("%s Error [%d] : OTP invalid command\r\n", api_name, rslt);
            break;
        case BMM350_E_OTP_UNDEFINED:
            am_util_stdio_printf("%s Error [%d] : OTP undefined\r\n", api_name, rslt);
            break;
        case BMM350_E_ALL_AXIS_DISABLED:
            am_util_stdio_printf("%s Error [%d] : All axis are disabled\r\n", api_name, rslt);
            break;
        case BMM350_E_PMU_CMD_VALUE:
            am_util_stdio_printf("%s Error [%d] : Unexpected PMU CMD value\r\n", api_name, rslt);
            break;
        default:
            am_util_stdio_printf("%s Error [%d] : Unknown error code\r\n", api_name, rslt);
            break;
    }
}

/*!
 *  @brief Function to select the interface.
 */
int8_t bmm350_interface_init(struct bmm350_dev *dev)
{
    int8_t rslt = BMM350_OK;

    if (dev != NULL)
    {
        dev_addr = BMM350_I2C_ADSEL_SET_LOW;
        dev->intf_ptr = &dev_addr;
        dev->read = bmm350_i2c_read;
        dev->write = bmm350_i2c_write;
        dev->delay_us = bmm350_delay;

        am_hal_iom_initialize(BMM350_IOM_MODULE, &iom_handle);
        am_hal_iom_power_ctrl(iom_handle, AM_HAL_SYSCTRL_WAKE, false);
        am_hal_iom_configure(iom_handle, &iom_i2c_config);
        am_hal_iom_enable(iom_handle);

        am_hal_gpio_pinconfig(AM_BSP_GPIO_MAG_SDA, g_AM_BSP_GPIO_MAG_SDA);
        am_hal_gpio_pinconfig(AM_BSP_GPIO_MAG_SCL, g_AM_BSP_GPIO_MAG_SCL);
    }
    else
    {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

void bmm350_interface_deinit(struct bmm350_dev *dev)
{
    if (dev)
    {
        am_hal_gpio_pinconfig(AM_BSP_GPIO_MAG_SDA, g_AM_HAL_GPIO_DISABLE);
        am_hal_gpio_pinconfig(AM_BSP_GPIO_MAG_SCL, g_AM_HAL_GPIO_DISABLE);

        am_hal_iom_disable(iom_handle);
        am_hal_iom_power_ctrl(iom_handle, AM_HAL_SYSCTRL_DEEPSLEEP, false);
        am_hal_iom_uninitialize(iom_handle);
    }
}
