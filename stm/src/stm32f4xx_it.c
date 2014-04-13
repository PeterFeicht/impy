/**
 * @file    stm32f4xx_it.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Interrupt service routines.
 * 
 * This file contains some of the interrupt service routines that mostly only call HAL functions.
 */

// Includes -------------------------------------------------------------------
#include "stm32f4xx_hal.h"

// External variables ---------------------------------------------------------

// Interrupt Handlers ---------------------------------------------------------

/**
 * This function handles the System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
