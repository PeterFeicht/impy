/**
 * @file    eeprom.c
 * @author  Peter Feichtinger
 * @date    16.04.2014
 * @brief   This file contains functions to manage the EEPROM chip.
 */

// Includes -------------------------------------------------------------------
#include <assert.h>
#include "eeprom.h"

// Check structure size constants, buffer data without the checksum needs to be aligned to 32 bits for CRC calculation
_Static_assert((EEPROM_CONFIG_SIZE & 3) == 0, "Configuration buffer not aligned");
_Static_assert((EEPROM_SETTINGS_SIZE & 3) == 0, "Settings buffer not aligned");
// Check structure sizes, in case the compiler aligns something we don't want it to
_Static_assert(sizeof(EEPROM_ConfigurationBuffer) == EEPROM_CONFIG_SIZE, "Bad EEPROM_ConfigurationBuffer definition");
_Static_assert(sizeof(EEPROM_SettingsBuffer) == EEPROM_SETTINGS_SIZE, "Bad EEPROM_SettingsBuffer definition");

// Private function prototypes ------------------------------------------------
__STATIC_INLINE uint8_t EE_IsBusy(void);

// Private variables ----------------------------------------------------------
static volatile EEPROM_Status status = EE_UNINIT;
static I2C_HandleTypeDef *i2cHandle = NULL;
static CRC_HandleTypeDef *crcHandle = NULL;
static uint8_t e2_state;
static EEPROM_ConfigurationBuffer buf_config;


// Private functions ----------------------------------------------------------

/**
 * Gets a value indicating whether the driver is busy or can start a new transfer.
 */
__STATIC_INLINE uint8_t EE_IsBusy(void)
{
    EEPROM_Status tmp = status;
    return (tmp != EE_FINISH &&
            tmp != EE_IDLE);
}

// Exported functions ---------------------------------------------------------

/**
 * Gets the current driver status.
 */
EEPROM_Status GetStatus(void)
{
    return status;
}

/**
 * Initializes the driver with the specified I2C handle for communication and E2 bit state.
 * 
 * @param i2c Pointer to an I2C handle structure that is to be used for communication with the EEPROM
 * @param crc Pointer to a CRC handle structure used for CRC calculation
 * @param e2_set Indicates the state of the E2 pin of the device
 * @return {@link EEPROM_Error} code
 */
EEPROM_Error EE_Init(I2C_HandleTypeDef *i2c, CRC_HandleTypeDef *crc, uint8_t e2_set)
{
    assert_param(i2c != NULL);
    assert_param(crc != NULL);
    
    i2cHandle = i2c;
    crcHandle = crc;
    e2_state = (e2_set ? EEPROM_M24C08_ADDR_E2 : 0);
    return EE_OK;
}

/**
 * Resets the driver to initialization state.
 * 
 * @return {@code EE_ERROR} if the driver has not been initialized, {@code EE_OK} otherwise
 */
EEPROM_Error EE_Reset(void)
{
    if(status == EE_UNINIT)
    {
        return EE_ERROR;
    }
    
    status = EE_IDLE;
    return EE_OK;
}

/**
 * Reads the configuration data from the EEPROM.
 * 
 * @param buffer Structure pointer receiving the data, the buffer is not altered if the read fails
 * @return {@link EEPROM_Error} code
 */
EEPROM_Error EE_ReadConfiguration(EEPROM_ConfigurationBuffer *buffer)
{
    assert_param(buffer != NULL);
    
    if(EE_IsBusy())
    {
        return EE_BUSY;
    }
    status = EE_READ;
    
    HAL_StatusTypeDef ret;
    uint8_t dev_addr = MAKE_ADDRESS(EEPROM_CONFIG_OFFSET, e2_state);
    uint32_t crc;
    
    // Read buffer from EEPROM
    ret = HAL_I2C_Mem_Read(i2cHandle, dev_addr, EEPROM_CONFIG_OFFSET, 1,
            (uint8_t *)&buf_config, sizeof(EEPROM_ConfigurationBuffer), EEPROM_I2C_TIMEOUT);
    if(ret != HAL_OK)
    {
        status = EE_IDLE;
        return EE_ERROR;
    }
    
    // Check integrity
    crc = HAL_CRC_Calculate(crcHandle, (uint32_t *)&buf_config, sizeof(EEPROM_ConfigurationBuffer) - 4);
    if(crc == buf_config.checksum)
    {
        *buffer = buf_config;
        return EE_OK;
    }
    else
    {
        return EE_ERROR;
    }
}

// ----------------------------------------------------------------------------
