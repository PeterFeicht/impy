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
extern I2C_HandleTypeDef hi2c1;

// Interrupt Handlers ---------------------------------------------------------

/**
 * This function handles the System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * @brief This function handles I2C1 event interrupt.
 */
void I2C1_EV_IRQHandler(void)
{
    HAL_NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

/**
 * @brief This function handles USB On The Go FS global interrupt.
 */
void OTG_FS_IRQHandler(void)
{
    HAL_NVIC_ClearPendingIRQ(OTG_FS_IRQn);
    HAL_PCD_IRQHandler(&hpcd_FS);
}
