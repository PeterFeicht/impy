/**
 * @file    eeprom.h
 * @author  Peter Feichtinger
 * @date    16.04.2014
 * @brief   This file contains constants and control function prototypes for the EEPROM driver.
 */

#ifndef EEPROM_H_
#define EEPROM_H_

// Includes -------------------------------------------------------------------
#include <stdint.h>
#include "stm32f4xx_hal.h"

// Exported type definitions --------------------------------------------------
typedef enum
{
    EE_UNINIT = 0,      //!< Driver has not been initialized
    EE_IDLE,            //!< Driver has been initialized and is ready to start a transfer
    EE_FINISH,          //!< Driver has finished with a transfer
    EE_READ,            //!< Driver is doing a read operation
    EE_WRITE            //!< Driver is doing a write operation
} EEPROM_Status;

typedef enum
{
    EE_OK = 0,      //!< Indicates success
    EE_BUSY,        //!< Indicates that the driver is currently busy doing a transfer
    EE_ERROR        //!< Indicates an error condition
} EEPROM_Error;

typedef struct
{
    uint32_t Address;   //!< Address of the data on the EEPROM
    uint32_t Length;    //!< Length of the buffer in bytes
    uint8_t  *pData;    //!< Pointer to data buffer
} EEPROM_Data;

// Macros ---------------------------------------------------------------------

#define LOBYTE(x)  ((uint8_t)(x & 0x00FF))
#define HIBYTE(x)  ((uint8_t)((x & 0xFF00) >> 8))

// Constants ------------------------------------------------------------------

/**
 * @defgroup EEPROM_M24C08_ADDRESS M24C08 Address definitions
 * 
 * The address byte that is sent to an M24C08 consists of 4 parts:
 *  + The device identifier {@link EEPROM_M24C08_ADDR} (4 bits)
 *  + An address bit {@link EEPROM_M24C08_ADDR_E2} that is either set or not, depending on the value of the {@code E2}
 *    pin on the M24C08 device
 *  + The two most significant bits of the memory address to be read/written {@link EEPROM_M24C08_BYTE_ADDR_H}
 *  + The standard I2C read/write bit
 * @{
 */
#define EEPROM_M24C08_ADDR                  0xA0        //!< M24C08 device identifier
#define EEPROM_M24C08_ADDR_E2               0x08        //!< M24C08 E2 address bit
/**
 * Bitmask for the M24C08 I2C Address bits that are used for the memory address. The value needs to be shifted left by
 * one from the actual address, since the I2C read/write bit is between these two address bits and the low byte.
 */
#define EEPROM_M24C08_BYTE_ADDR_H           0x06
/** @} */

/**
 * Timeout in ms for I2C communication
 */
#define EEPROM_I2C_TIMEOUT                  0x200

// Exported functions ---------------------------------------------------------

EEPROM_Status GetStatus(void);
EEPROM_Error EEPROM_Init(I2C_HandleTypeDef *i2c, uint8_t e2_set);
EEPROM_Error EEPROM_Reset(void);
EEPROM_Error EEPROM_ReadByte(uint16_t address, uint8_t *destination);
EEPROM_Error EEPROM_ReadPage(uint16_t address, uint8_t *buffer, uint8_t size);
EEPROM_Error EEPROM_WriteByte(uint16_t address, uint8_t value);
EEPROM_Error EEPROM_WritePage(uint16_t address, uint8_t *buffer, uint8_t size);
void EEPROM_TIM_PeriodElapsedCallback(void);

// ----------------------------------------------------------------------------

#endif /* EEPROM_H_ */
