/**
 * @file shift_register_cbk.c
 * @brief Callback implementations for the Shift Register module.
 *
 * This file contains the callback functions used by the Shift Register module
 * to interact with the underlying hardware. The functions defined here are
 * called by the Shift Register to perform specific actions such as setting
 * GPIO levels and PWM duty cycles.
 */

#include "shift_register.h"
#include "application.h"

/* ---------------------------------------------------------------------------- */
/*  Private macros and constants                                                */
/* ---------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------- */
/*  Private function prototypes                                                 */
/* ---------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------- */
/*  Public function implementations                                             */
/* ---------------------------------------------------------------------------- */

bool Sr_cbk_InitGpio(const uint8_t au8_pin, const Sr_GpioLevel_t ae_gpioLevel)
{
    App_InitGpio(g_app_handle, au8_pin, (uint8_t)(ae_gpioLevel));
    return true; // Return success
}

bool Sr_cbk_SetGpio(const uint8_t au8_pin, const Sr_GpioLevel_t ae_gpioLevel)
{
    App_SetGpio(g_app_handle, au8_pin, (uint8_t)(ae_gpioLevel));
    return true; // Return success
}

void Sr_cbk_DelayUs(const uint64_t au64_timeUs)
{
    if (au64_timeUs > 0U)
	{
		//DelayMicroseconds(us);
        for (uint64_t i = 0; i < au64_timeUs; i++)
        {
		    asm("nop");asm("nop");asm("nop");asm("nop");
            asm("nop");asm("nop");asm("nop");asm("nop");
            asm("nop");asm("nop");asm("nop");asm("nop");
            asm("nop");asm("nop");asm("nop");asm("nop");
            asm("nop");asm("nop");asm("nop");asm("nop");
            // asm("nop") is 4.17ns @240MHz
        }
	}
}

/* ---------------------------------------------------------------------------- */
/*  Private function implementations                                            */
/* ---------------------------------------------------------------------------- */