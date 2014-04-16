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



// Exported functions ---------------------------------------------------------



// ----------------------------------------------------------------------------

#endif /* EEPROM_H_ */
