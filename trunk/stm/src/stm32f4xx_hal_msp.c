/**
 * @file    stm32f4xx_hal_msp.c
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   Provides MSP initialization code.
 */

// Includes -------------------------------------------------------------------
#include "stm32f4xx_hal.h"

// Exported functions ---------------------------------------------------------

/**
 * Initializes the I2C MSP.
 * @param hi2c I2C handle
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    if(hi2c->Instance == I2C1)
    {
        __I2C1_CLK_ENABLE();
        
        /*
         * GPIO configuration:
         *  PB6: I2C1_SCL
         *  PB9: I2C1_SDA
         */
        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        // Set I2C interrupt to the lowest priority
        HAL_NVIC_SetPriority(I2C1_EV_IRQn, 7, 0);
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    }
}

/**
 * De-initializes the I2C MSP.
 * @param hi2c I2C handle
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
    if(hi2c->Instance == I2C1)
    {
        __I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_9);
        HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
    }
}

// ----------------------------------------------------------------------------
