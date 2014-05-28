/**
 * @file    main.h
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Main header file.
 */

#ifndef MAIN_H_
#define MAIN_H_

// Includes -------------------------------------------------------------------
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "mx_init.h"
#include "console.h"
#include "ad5933.h"

// Exported type definitions --------------------------------------------------
typedef enum
{
    BOARD_OK = 0,
    BOARD_BUSY,
    BOARD_ERROR
} Board_Error;

typedef enum
{
    TEMP_AD5933
} Board_TemperatureSource;

typedef struct
{
    AD5933_Status ad_status;
    uint16_t point;
} Board_Status;

// Constants ------------------------------------------------------------------
#define LED_PORT        GPIOD
#define LED_ORANGE      GPIO_PIN_13
#define LED_GREEN       GPIO_PIN_12
#define LED_RED         GPIO_PIN_14
#define LED_BLUE        GPIO_PIN_15

/**
 * The minimum port number that can be used for measurements.
 */
#define PORT_MIN        0
/**
 * The maximum port number that can be used for measurements.
 */
#define PORT_MAX        9

// Exported functions ---------------------------------------------------------
Board_Error Board_SetStartFreq(uint32_t freq);
Board_Error Board_SetStopFreq(uint32_t freq);
Board_Error Board_SetFreqSteps(uint16_t steps);
Board_Error Board_SetSettlingCycles(uint16_t cycles);
Board_Error Board_SetVoltageRange(uint16_t range);
Board_Error Board_SetPGA(uint8_t enable);
Board_Error Board_SetAutorange(uint8_t enable);

uint32_t Board_GetStartFreq(void);
uint32_t Board_GetStopFreq(void);
uint16_t Board_GetFreqSteps(void);
uint16_t Board_GetSettlingCycles(void);
AD5933_RangeSettings* Board_GetRangeSettings(void);
uint8_t Board_GetAutorange(void);

void Board_GetStatus(Board_Status *result);
AD5933_ImpedancePolar* Board_GetDataPolar(uint32_t *count);
Board_Error Board_StartSweep(uint8_t port);
Board_Error Board_StopSweep(void);
uint8_t Board_GetPort(void);
Board_Error Board_MeasureSingleFrequency(uint8_t port, uint32_t freq, AD5933_ImpedancePolar *result);
float Board_MeasureTemperature(Board_TemperatureSource what);


// ----------------------------------------------------------------------------

#endif /* MAIN_H_ */
