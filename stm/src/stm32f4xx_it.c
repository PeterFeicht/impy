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
extern PCD_HandleTypeDef hpcd_FS;

// Interrupt Handlers ---------------------------------------------------------

/**
 * This function handles the System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * @brief This function handles USB On The Go FS global interrupt.
 */
void OTG_FS_IRQHandler(void)
{
    HAL_NVIC_ClearPendingIRQ(OTG_FS_IRQn);
    HAL_PCD_IRQHandler(&hpcd_FS);
}
