/**
 * @file    main.h
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Main header file.
 */

#ifndef MAIN_H_
#define MAIN_H_

// Includes -------------------------------------------------------------------
#include "stm32f4xx_hal.h"
#include "mx_init.h"

// Constants ------------------------------------------------------------------
#define LED_PORT        GPIOD
#define LED_ORANGE      GPIO_PIN_13
#define LED_GREEN       GPIO_PIN_12
#define LED_RED         GPIO_PIN_14
#define LED_BLUE        GPIO_PIN_15

#endif /* MAIN_H_ */
