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
#include "eeprom.h"

// Exported type definitions --------------------------------------------------
typedef enum
{
    BOARD_OK = 0,       //!< Indicates success
    BOARD_BUSY,         //!< Indicates that the board is currently busy and settings cannot be changed
    BOARD_ERROR         //!< Indicates an error condition
} Board_Error;

typedef enum
{
    TEMP_AD5933         //!< AD5933 internal chip temperature
} Board_TemperatureSource;

typedef struct
{
    AD5933_Status ad_status;    //!< Status code of the AD5933 driver
    uint16_t point;             //!< If a measurement is running, the number of data points already measured
    uint16_t totalPoints;       //!< The number of frequency steps to be measured
    uint8_t autorange;          //!< Whether autoranging is enabled
    uint8_t interrupted;        //!< Whether the last measurement was interrupted (false if a measurement is running)
    uint8_t validGainFactor;    //!< Whether a valid gain factor for the current range settings is present
    uint8_t validData;          //!< Whether valid measurement data is present
} Board_Status;

// Constants ------------------------------------------------------------------
#define BOARD_VERSION                   "1.0"

#define LED_PORT                        GPIOD
#define LED_ORANGE                      GPIO_PIN_13
#define LED_GREEN                       GPIO_PIN_12
#define LED_RED                         GPIO_PIN_14
#define LED_BLUE                        GPIO_PIN_15

#define BUTTON_PORT						GPIOA					//!< User button GPIO port
#define BUTTON_PIN						GPIO_PIN_0				//!< User button GPIO pin (active high)

#define SWITCH_MAIN_PORT				GPIOC					//!< Main power switch (Icc) GPIO port
#define SWITCH_MAIN_PIN					GPIO_PIN_15				//!< Main power switch (Icc) GPIO pin (active high)

#define SWITCH_USB_PORT					GPIOD					//!< USB host power switch GPIO port
#define SWITCH_USB_PIN					GPIO_PIN_8				//!< USB host power switch GPIO pin (active high)

/**
 * The maximum port number that can be used for measurements.
 */
#define PORT_MAX                        9

/**
 * The first calibration port number
 */
#define CAL_PORT_MIN                    10

/**
 * Timeout in ms for SPI communication
 */
#define BOARD_SPI_TIMEOUT               10

/**
 * @defgroup BOARD_SPI_SS_GPIO SPI Slave Select GPIO Definition
 * @{
 */
#define BOARD_SPI_SS_GPIO_PORT          GPIOD
#define BOARD_SPI_SS_GPIO_FLASH         GPIO_PIN_0
#define BOARD_SPI_SS_GPIO_SRAM          GPIO_PIN_1
#define BOARD_SPI_SS_GPIO_MUX           GPIO_PIN_2
/** @} */

/**
 * @defgroup ADG725 Communication Definition for the ADG725
 * @{
 */
#define ADG725_MASK_PORT                ((uint8_t)0x0F)
#define ADG725_CHIP_ENABLE_NOT          ((uint8_t)0x80)
#define ADG725_CHIP_CSA_NOT             ((uint8_t)0x40)
#define ADG725_CHIP_CSB_NOT             ((uint8_t)0x20)
/** @} */

// Set to 1 if the EEPROM is fitted, 0 otherwise
#define BOARD_HAS_EEPROM                1

/**
 * Whether the E2 pin on the EEPROM chip is pulled high.
 */
#define EEPROM_E2_PIN_SET               0

// Exported variables ---------------------------------------------------------
extern USBD_HandleTypeDef hUsbDevice;
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi3;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim10;
extern CRC_HandleTypeDef hcrc;
extern EEPROM_ConfigurationBuffer board_config;

// Exported functions ---------------------------------------------------------
Board_Error Board_SetStartFreq(uint32_t freq);
Board_Error Board_SetStopFreq(uint32_t freq);
Board_Error Board_SetFreqSteps(uint16_t steps);
Board_Error Board_SetSettlingCycles(uint16_t cycles, uint8_t multiplier);
Board_Error Board_SetVoltageRange(uint16_t voltage);
Board_Error Board_SetPGA(uint8_t enable);
Board_Error Board_SetAutorange(uint8_t enable);
Board_Error Board_SetFeedback(uint32_t ohms);
Board_Error Board_SetAverages(uint16_t value);

uint32_t Board_GetStartFreq(void);
uint32_t Board_GetStopFreq(void);
uint16_t Board_GetFreqSteps(void);
uint16_t Board_GetSettlingCycles(void);
const AD5933_RangeSettings* Board_GetRangeSettings(void);
uint8_t Board_GetAutorange(void);
uint16_t Board_GetAverages(void);

void Board_GetStatus(Board_Status *result);
void Board_Reset(void);
void Board_Standby(void);
const AD5933_ImpedancePolar* Board_GetDataPolar(uint32_t *count);
const AD5933_ImpedanceData* Board_GetDataRaw(uint32_t *count);
const AD5933_GainFactor* Board_GetGainFactor(void);
Board_Error Board_StartSweep(uint8_t port);
Board_Error Board_StopSweep(void);
uint8_t Board_GetPort(void);
Board_Error Board_MeasureSingleFrequency(uint8_t port, uint32_t freq, AD5933_ImpedancePolar *result);
float Board_MeasureTemperature(Board_TemperatureSource what);
Board_Error Board_Calibrate(uint32_t ohms);


// ----------------------------------------------------------------------------

#endif /* MAIN_H_ */
