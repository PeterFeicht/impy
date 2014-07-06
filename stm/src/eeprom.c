/**
 * @file    eeprom.c
 * @author  Peter Feichtinger
 * @date    16.04.2014
 * @brief   This file contains functions to manage the EEPROM chip.
 */

// Includes -------------------------------------------------------------------
#include <assert.h>
#include <stddef.h>
#include "eeprom.h"

// Check structure size constants, buffer data without the checksum needs to be aligned to 32 bits for CRC calculation
_Static_assert((EEPROM_CONFIG_SIZE & 3) == 0, "Configuration buffer not aligned");
_Static_assert((EEPROM_SETTINGS_SIZE & 3) == 0, "Settings buffer not aligned");
// Check structure sizes, in case the compiler aligns something we don't want it to
_Static_assert(sizeof(EEPROM_ConfigurationBuffer) == EEPROM_CONFIG_SIZE, "Bad EEPROM_ConfigurationBuffer definition");
_Static_assert(sizeof(EEPROM_SettingsBuffer) == EEPROM_SETTINGS_SIZE, "Bad EEPROM_SettingsBuffer definition");

// Private function prototypes ------------------------------------------------
static HAL_StatusTypeDef EE_Read(uint16_t address, uint8_t *buffer, uint16_t length);
static HAL_StatusTypeDef EE_Write(uint16_t address, uint8_t *buffer, uint16_t length);
__STATIC_INLINE uint8_t EE_IsBusy(void);
static HAL_StatusTypeDef EE_FindLatestSettings(uint16_t *result);

// Private macros -------------------------------------------------------------
#define CRC_SIZE(X)         ((sizeof(X) - 4) >> 2)

// Private variables ----------------------------------------------------------
static volatile EEPROM_Status status = EE_UNINIT;
static I2C_HandleTypeDef *i2cHandle = NULL;
static CRC_HandleTypeDef *crcHandle = NULL;
static uint8_t e2_state;                        //!< State of the E2 pin on the device
static EEPROM_ConfigurationBuffer buf_config;   //!< Temporary configuration buffer for reading and writing
static EEPROM_SettingsBuffer buf_settings;      //!< Temporary settings buffer for reading and writing
static uint8_t *write_buf;                      //!< The next address to write from
static uint16_t write_addr;                     //!< The next address to write to
static uint16_t write_len;                      //!< The number of bytes remaining

// Private functions ----------------------------------------------------------
 
/**
 * Reads an amount of data from the EEPROM.
 * 
 * @param address The address to read from
 * @param buffer Pointer to buffer receiving the data
 * @param length Number of bytes to read
 * @return HAL status code
 */
static HAL_StatusTypeDef EE_Read(uint16_t address, uint8_t *buffer, uint16_t length)
{
    assert_param(length > 0 && address + length <= EEPROM_SIZE);
    
    uint8_t dev_addr = MAKE_ADDRESS(address, e2_state);
    return HAL_I2C_Mem_Read(i2cHandle, dev_addr, address, 1, buffer, length, EEPROM_I2C_TIMEOUT);
}

/**
 * Writes an amount of data to the EEPROM, if the write cannot be completed in one go, the first page is written and
 * {@code write_addr} and {@code write_len} are set.
 * 
 * @param address The address to write to
 * @param buffer Pointer to buffer with data to write
 * @param length Number of bytes to write
 * @return HAL status code
 */
static HAL_StatusTypeDef EE_Write(uint16_t address, uint8_t *buffer, uint16_t length)
{
    assert_param(length > 0 && address + length <= EEPROM_SIZE);
    
    uint16_t len = (length <= EEPROM_PAGE_SIZE ? length : EEPROM_PAGE_SIZE);
    if((address & EEPROM_PAGE_MASK) != ((address + len - 1) & EEPROM_PAGE_MASK))
    {
        // Write spans multiple pages, only write bytes in first page
        len = ((address + len) & EEPROM_PAGE_MASK) - address;
    }
    
    HAL_StatusTypeDef ret =
            HAL_I2C_Mem_Write(i2cHandle, MAKE_ADDRESS(address, e2_state), address, 1, buffer, len, EEPROM_I2C_TIMEOUT);
    
    if(ret == HAL_OK)
    {
        write_buf = buffer + len;
        write_addr = address + len;
        write_len = length - len;
    }
    return ret;
}

/**
 * Gets a value indicating whether the driver is busy or can start a new transfer.
 */
__STATIC_INLINE uint8_t EE_IsBusy(void)
{
    EEPROM_Status tmp = status;
    return (tmp != EE_FINISH &&
            tmp != EE_IDLE);
}

/**
 * Finds the EEPROM address of the latest settings buffer.
 * 
 * @param result Pointer to variable receiving the address
 * @return HAL status code
 */
static HAL_StatusTypeDef EE_FindLatestSettings(uint16_t *result)
{
    HAL_StatusTypeDef ret;
    uint16_t addr = EEPROM_DATA_OFFSET;
    uint16_t serial;
    
    ret = EE_Read(addr + offsetof(EEPROM_SettingsBuffer, serial), (uint8_t *)&serial, 2);
    if(ret != HAL_OK)
    {
        return ret;
    }
    
    // Look for latest buffer
    while(addr + 2 * EEPROM_SETTINGS_SIZE <= EEPROM_DATA_OFFSET + EEPROM_DATA_SIZE)
    {
        uint16_t tmp;
        ret = EE_Read(addr + EEPROM_SETTINGS_SIZE + offsetof(EEPROM_SettingsBuffer, serial), (uint8_t *)&tmp, 2);
        if(ret != HAL_OK)
        {
            return ret;
        }
        if(tmp != serial + 1)
        {
            break;
        }
        serial = tmp;
        addr += EEPROM_SETTINGS_SIZE;
    }
    
    *result = addr;
    return HAL_OK;
}

// Exported functions ---------------------------------------------------------

/**
 * Gets the current driver status.
 */
EEPROM_Status EE_GetStatus(void)
{
    return status;
}

/**
 * Initializes the driver with the specified I2C handle for communication and E2 bit state.
 * 
 * @param i2c Pointer to an I2C handle structure that is to be used for communication with the EEPROM
 * @param crc Pointer to a CRC handle structure used for CRC calculation
 * @param e2_set Indicates the state of the E2 pin of the device
 * @return {@code EE_OK}
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
    assert(status != EE_UNINIT);
    
    if(EE_IsBusy())
    {
        return EE_BUSY;
    }
    status = EE_READ;
    
    // Read buffer from EEPROM
    HAL_StatusTypeDef ret = EE_Read(EEPROM_CONFIG_OFFSET, (uint8_t *)&buf_config, sizeof(EEPROM_ConfigurationBuffer));
    if(ret != HAL_OK)
    {
        status = EE_IDLE;
        return EE_ERROR;
    }
    
    // Check integrity
    uint32_t crc = HAL_CRC_Calculate(crcHandle, (uint32_t *)&buf_config, CRC_SIZE(EEPROM_ConfigurationBuffer));
    if(crc == buf_config.checksum)
    {
        *buffer = buf_config;
        status = EE_FINISH;
        return EE_OK;
    }
    else
    {
        status = EE_IDLE;
        return EE_ERROR;
    }
}

/**
 * Writes configuration data to the EEPROM.
 * 
 * @param buffer Structure pointer to data to write, the checksum is set before writing
 * @return {@link EEPROM_Error} code
 */
EEPROM_Error EE_WriteConfiguration(EEPROM_ConfigurationBuffer *buffer)
{
    assert_param(buffer != NULL);
    assert(status != EE_UNINIT);
    
    if(EE_IsBusy())
    {
        return EE_BUSY;
    }
    
    buffer->checksum = HAL_CRC_Calculate(crcHandle, (uint32_t *)buffer, CRC_SIZE(EEPROM_ConfigurationBuffer));
    buf_config = *buffer;
    HAL_StatusTypeDef ret = EE_Write(EEPROM_CONFIG_OFFSET, (uint8_t *)&buf_config, sizeof(EEPROM_ConfigurationBuffer));
    
    if(ret == HAL_OK)
    {
        status = EE_WRITE_CONFIG;
        return EE_OK;
    }
    else
    {
        return EE_ERROR;
    }
}

/**
 * Reads the latest settings data from the EEPROM.
 * 
 * @param buffer Structure pointer receiving the data, the buffer is not altered if the read fails
 * @return {@link EEPROM_Error} code
 */
EEPROM_Error EE_ReadSettings(EEPROM_SettingsBuffer *buffer)
{
    assert_param(buffer != NULL);
    assert(status != EE_UNINIT);
    
    if(EE_IsBusy())
    {
        return EE_BUSY;
    }
    status = EE_READ;

    uint16_t addr;
    HAL_StatusTypeDef ret = EE_FindLatestSettings(&addr);
    if(ret != HAL_OK)
    {
        status = EE_IDLE;
        return EE_ERROR;
    }
    
    do
    {
        // Read current buffer
        ret = EE_Read(addr, (uint8_t *)&buf_settings, sizeof(EEPROM_SettingsBuffer));
        if(ret != HAL_OK)
        {
            status = EE_IDLE;
            return EE_ERROR;
        }
        
        // Check integrity
        uint32_t crc = HAL_CRC_Calculate(crcHandle, (uint32_t *)&buf_settings, CRC_SIZE(EEPROM_SettingsBuffer));
        if(crc == buf_settings.checksum)
        {
            *buffer = buf_settings;
            status = EE_FINISH;
            return EE_OK;
        }
        
        // CRC failed, try the previous buffer
        addr -= EEPROM_SETTINGS_SIZE;
    } while(addr >= EEPROM_DATA_OFFSET);
    
    status = EE_IDLE;
    return EE_ERROR;
}

/**
 * Writes settings data to the EEPROM.
 * 
 * @param buffer Structure pointer to data to write, the checksum is set before writing
 * @return {@link EEPROM_Error} code
 */
EEPROM_Error EE_WriteSettings(EEPROM_SettingsBuffer *buffer)
{
    assert_param(buffer != NULL);
    assert(status != EE_UNINIT);
    
    if(EE_IsBusy())
    {
        return EE_BUSY;
    }
    
    uint16_t addr;
    HAL_StatusTypeDef ret = EE_FindLatestSettings(&addr);
    if(ret != HAL_OK)
    {
        status = EE_IDLE;
        return EE_ERROR;
    }
    addr += EEPROM_SETTINGS_SIZE;
    if(addr + EEPROM_SETTINGS_SIZE > EEPROM_DATA_OFFSET + EEPROM_DATA_SIZE)
    {
        addr = EEPROM_DATA_OFFSET;
    }
    
    buffer->checksum = HAL_CRC_Calculate(crcHandle, (uint32_t *)buffer, CRC_SIZE(EEPROM_SettingsBuffer));
    buf_settings = *buffer;
    ret = EE_Write(addr, (uint8_t *)&buf_settings, sizeof(EEPROM_SettingsBuffer));
    
    if(ret == HAL_OK)
    {
        status = EE_WRITE_SETTINGS;
        return EE_OK;
    }
    else
    {
        return EE_ERROR;
    }
}

/**
 * This function should be called periodically to update the driver status.
 * 
 * @return The (new) EEPROM status
 */
EEPROM_Status EE_TimerCallback(void)
{
    switch(status)
    {
        case EE_UNINIT:
        case EE_IDLE:
        case EE_FINISH:
            break;
            
        case EE_READ:
            // Reads are done in one go and blocking, nothing to do here
            break;
            
        case EE_WRITE_CONFIG:
        case EE_WRITE_SETTINGS:
            if(write_len > 0)
            {
                // Not finished, try to write
                EE_Write(write_addr, write_buf, write_len);
            }
            else
            {
                // Finished writing, wait for EEPROM to complete write cycle
                if(HAL_I2C_IsDeviceReady(i2cHandle, MAKE_ADDRESS(0, e2_state), 1, EEPROM_I2C_TIMEOUT) == HAL_OK)
                {
                    status = EE_FINISH;
                }
            }
            break;
    }
    
    return status;
}

// ----------------------------------------------------------------------------
