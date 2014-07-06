/**
 * @file    eeprom.h
 * @author  Peter Feichtinger
 * @date    16.04.2014
 * @brief   This file contains constants, type definitions and control function prototypes for the EEPROM driver.
 * 
 * The different types of data structures stored on the EEPROM are defined here as well:
 *  + {@link EEPROM_ConfigurationBuffer} stores configuration data for a specific board that is not likely to change,
 *    such as soldered resistor values or the Ethernet MAC address.
 *  + {@link EEPROM_SettingsBuffer} stores the current sweep settings and related things.
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
    EE_WRITE_CONFIG,    //!< Driver is writing configuration data
    EE_WRITE_SETTINGS   //!< Driver is writing settings data
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

typedef struct
{
    struct _peripherals
    {
        unsigned int sram : 1;              //!< Whether external SRAM is populated
        unsigned int flash : 1;             //!< Whether external flash memory is populated
        unsigned int eth : 1;               //!< Whether the Ethernet interface is populated
        unsigned int usbh : 1;              //!< Whether the USB host port is populated
        unsigned int reserved : 28;         //!< Reserved for future use, padding to 32 bits (set to 0)
    } peripherals;                          //!< Bitfield for populated peripherals
    uint16_t attenuations[4];               //!< Possible attenuation values, {@code 0} for unpopulated ports
    uint32_t feedback_resistors[8];         //!< Feedback resistor values, {@code 0} for unpopulated ports
    uint32_t calibration_values[6];         //!< Calibration resistor values, {@code 0} for unpopulated ports
    uint16_t coupling_tau;                  //!< Time constant of the coupling capacitor RC network in ms
    uint8_t  eth_mac[6];                    //!< Ethernet MAC address, 48 bits with MSB first
    uint8_t  reserved[48];                  //!< Reserved for future use, padding to 128 bytes (set to 0)
    uint32_t checksum;                      //!< CRC32 checksum of the buffer
} EEPROM_ConfigurationBuffer;

typedef struct
{
    /* Sweep */
    uint32_t start_freq;                    //!< Sweep start frequency in Hz
    uint32_t stop_freq;                     //!< Sweep stop frequency in Hz
    uint32_t feedback;                      //!< Feedback resistor value in Ohm
    uint16_t num_steps;                     //!< Number of frequency increments
    uint16_t settling_cycles;               //!< Number of settling cycles, register value
    uint16_t averages;                      //!< Number of averages per frequency point
    uint16_t voltage;                       //!< Output voltage range, register value
    uint16_t attenuation;                   //!< Output voltage attenuation
    uint16_t pad1;                          //!< Padding for 32 bit alignment (set to 0)
    /* Console */
    uint32_t format_spec;                   //!< Console format specification
    /* ETH */
    uint32_t ip_address;                    //!< Ethernet IP address
    /* Bitfield */
    struct _flags
    {
        /* Sweep */
        unsigned int pga_enabled : 1;       //!< Whether the x5 gain is enabled
        unsigned int autorange : 1;         //!< Whether autoranging is enabled
        /* ETH */
        unsigned int dhcp : 1;              //!< Whether DHCP is enabled
        unsigned int netmask : 5;           //!< The number of bits set in the IP network mask
        /* Metadata */
        unsigned int reserved : 24;         //!< Reserved for future use, padding to 32 bits (set to 0)
    } flags;                                //!< Bitfield for flags and small values
    /* Metadata */
    uint8_t reserved[22];                   //!< Reserved for future use, padding to 64 bytes  (set to 0)
    uint16_t serial;                        //!< Buffer serial number for EEPROM wear leveling, should not be modified
    uint32_t checksum;                      //!< CRC32 checksum of the buffer
} EEPROM_SettingsBuffer;

// Macros ---------------------------------------------------------------------

#ifndef LOBYTE
#define LOBYTE(x)  ((uint8_t)(x & 0x00FF))
#endif
#ifndef HIBYTE
#define HIBYTE(x)  ((uint8_t)((x & 0xFF00) >> 8))
#endif
#define MAKE_ADDRESS(_addr, _e2)            ((uint8_t)EEPROM_M24C08_ADDR | \
                                             ((_e2) ? EEPROM_M24C08_ADDR_E2 : 0) | \
                                             (((uint8_t)(_addr) >> 7) & EEPROM_M24C08_BYTE_ADDR_H))

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
 * Bitmask for the M24C08 I2C Address bits that are used for the high byte of the memory address. The address bits are
 * either the high byte shifted left by one or the 16 bit address shifted right by 7 and then masked.
 */
#define EEPROM_M24C08_BYTE_ADDR_H           0x06
/** @} */

/**
 * Timeout in ms for I2C communication
 */
#define EEPROM_I2C_TIMEOUT                  0x200

/**
 * EEPROM size in bytes
 */
#define EEPROM_SIZE                         0x400

/**
 * Mask for the page address, a page write can only write to addresses which have the same page address
 */
#define EEPROM_PAGE_MASK                    0x03F0

/**
 * Page size of the EEPROM, only one page can be written at a time
 */
#define EEPROM_PAGE_SIZE                    0x10

/**
 * Configuration data offset, that is the first address of the configuration data space
 */
#define EEPROM_CONFIG_OFFSET                0

/**
 * Size of the configuration data section in bytes
 */
#define EEPROM_CONFIG_SIZE                  128

/**
 * Data offset, that is the first address for arbitrary data
 */
#define EEPROM_DATA_OFFSET                  0x80

/**
 * Size of the data section in bytes, this is the EEPROM size minus the data offset
 */
#define EEPROM_DATA_SIZE                    (EEPROM_SIZE - EEPROM_DATA_OFFSET)

/**
 * Size of the settings buffer in bytes
 */
#define EEPROM_SETTINGS_SIZE                64

// Exported functions ---------------------------------------------------------

EEPROM_Status GetStatus(void);
EEPROM_Error EE_Init(I2C_HandleTypeDef *i2c, CRC_HandleTypeDef *crc, uint8_t e2_set);
EEPROM_Error EE_Reset(void);
EEPROM_Error EE_ReadConfiguration(EEPROM_ConfigurationBuffer *buffer);
EEPROM_Error EE_WriteConfiguration(EEPROM_ConfigurationBuffer *buffer);
EEPROM_Error EE_ReadSettings(EEPROM_SettingsBuffer *buffer);
EEPROM_Error EE_WriteSettings(EEPROM_SettingsBuffer *buffer);
EEPROM_Status EE_TimerCallback(void);

// ----------------------------------------------------------------------------

#endif /* EEPROM_H_ */
