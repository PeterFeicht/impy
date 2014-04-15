/**
 * @file    ad5933.h
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   This file contains register definitions and control function prototypes for the AD5933 driver.
 */

#ifndef AD5933_H_
#define AD5933_H_

// Exported type definitions --------------------------------------------------
typedef struct
{
    uint32_t Start_Freq;        //!< Start frequency for the sweep in Hz
    uint32_t Freq_Increment;    //!< Frequency increment for the sweep in Hz
    uint16_t Num_Increments;    //!< Number of frequency points for the sweep
    uint16_t Settling_Cycles;   //!< Number of settling cycles before a measurement
} AD5933_SweepTypeDef;

typedef struct
{
    int16_t Real;   //!< Scaled real part of the impedance
    int16_t Imag;   //!< Scaled imaginary part of the impedance
} AD5933_ImpedanceTypeDef;

// Constants ------------------------------------------------------------------

/**
 * I2C Address of the AD9533, cannot be changed.
 */
#define AD9533_ADDR                         0x0D

/*****************************************************************************
 *                              REGISTER MAPPING                             *
 *****************************************************************************/

/**
 * Control Register High Byte (16-bit)
 * Read/Write
 * Default value: 0xA0 (Power down)
 * 
 *  + D15:D12   Control function, see {@link AD9533_FUNCTION}
 *  + D11       Not used
 *  + D10:D9    Output voltage range setting, see {@link AD5933_VOLTAGE}
 *  + D8        PGA gain setting, see {@link AD9533_GAIN}
 */
#define AD5933_CTRL_H_ADDR                  0x80

/**
 * Control Register Low Byte (16-bit)
 * Read/Write
 * Default value: 0x00
 * 
 *  + D7:D5     Reserved, set to 0
 *  + D4        Reset
 *  + D3        Clock source setting, see {@link AD9533_CLOCK}
 *  + D2:D0     Reserved, set to 0
 */
#define AD5933_CTRL_L_ADDR                  0x81

/**
 * Start Frequency Register High Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 */
#define AD9533_START_FREQ_H_ADDR            0x82

/**
 * Start Frequency Register Mid Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 */
#define AD9533_START_FREQ_M_ADDR            0x83

/**
 * Start Frequency Register Low Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 */
#define AD9533_START_FREQ_L_ADDR            0x84

/**
 * Frequency Increment Register High Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 */
#define AD9533_FREQ_INCR_H_ADDR             0x85

/**
 * Frequency Increment Register Mid Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 */
#define AD9533_FREQ_INCR_M_ADDR             0x86

/**
 * Frequency Increment Register Low Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 */
#define AD9533_FREQ_INCR_L_ADDR             0x87

/**
 * Number of Increments Register High Byte (9-bit unsigned)
 * Read/Write
 * Default value: none
 * 
 *  + D16:D9    Don't care
 *  + D8        MSB of value
 */
#define AD9533_NUM_INCR_H_ADDR              0x88

/**
 * Number of Increments Register Low Byte (9-bit unsigned)
 * Read/Write
 * Default value: none
 * 
 *  + D7:D0     LSB of value
 */
#define AD9533_NUM_INCR_L_ADDR              0x89

/**
 * Number of Settling Time Cycles Register High Byte (2+9-bit unsigned)
 * Read/Write
 * Default value: none
 * 
 *  + D15:D11   Don't care
 *  + D10:D9    Settling time multiplier, see {@link AD9533_SETTL_MULT}
 *  + D8        MSB of settling time cycles
 */
#define AD9533_SETTL_H_ADDR                 0x8A

/**
 * Number of Settling Time Cycles Register Low Byte (2+9-bit unsigned)
 * Read/Write
 * Default value: none
 * 
 *  + D7:D0     LSB of settling time cycles
 */
#define AD9533_SETTL_L_ADDR                 0x8B

/**
 * Status Register
 * Read only
 * Default value: 0x00
 * 
 *  + D7:D3     Reserved
 *  + D2        Frequency sweep complete
 *  + D1        Valid real/imaginary data
 *  + D0        Valid temperature measurement
 * 
 * See also {@link AD9533_STATUS}
 */
#define AD5933_STATUS_ADDR                  0x8F

/**
 * Temperature Data Register High Byte (14-bit signed)
 * Read only
 * Default value: none
 * 
 *  + D15:D14   Don't care
 *  + D13       Sign bit
 *  + D12:D8    MSB of temperature value
 */
#define AD5933_TEMP_H_ADDR                  0x92

/**
 * Temperature Data Register Low Byte (14-bit signed)
 * Read only
 * Default value: none
 * 
 *  + D7:D0     LSB of temperature value
 */
#define AD9533_TEMP_L_ADDR                  0x93

/**
 * Real Data Register High Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD9533_REAL_H_ADDR                  0x94

/**
 * Real Data Register Low Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD9533_REAL_L_ADDR                  0x95

/**
 * Imaginary Data Register High Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD9533_IMAG_H_ADDR                  0x96

/**
 * Imaginary Data Register Low Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD9533_IMAG_L_ADDR                  0x97

/*****************************************************************************
 *                              REGISTER VALUES                              *
 *****************************************************************************/

/**
 * @defgroup AD9533_FUNCTION Control Register Function Values
 * 
 * Control functions (Control register D15:D12)
 * @{
 */
// 
#define AD9533_FUNCTION_INIT_FREQ           ((uint16_t)0x01)    //!< Initialize with start frequency
#define AD9533_FUNCTION_START_SWEEP         ((uint16_t)0x02)    //!< Start frequency sweep
#define AD9533_FUNCTION_INCREMENT_FREQ      ((uint16_t)0x03)    //!< Increment frequency
#define AD9533_FUNCTION_REPEAT_FREQ         ((uint16_t)0x04)    //!< Repeat frequency
#define AD9533_FUNCTION_MEASURE_TEMP        ((uint16_t)0x09)    //!< Measure temperature
#define AD9533_FUNCTION_POWER_DOWN          ((uint16_t)0x0A)    //!< Power-down mode
#define AD9533_FUNCTION_STANDBY             ((uint16_t)0x0B)    //!< Standby mode
/** @} */

/**
 * @defgroup AD5933_VOLTAGE Control Register Voltage Range Values
 * 
 * Output voltage range (Control register D10:D9)
 * @{
 */
#define AD5933_VOLTAGE_2                    ((uint16_t)0x00)    //!< Range 1: 2V p-p
#define AD5933_VOLTAGE_1                    ((uint16_t)0x03)    //!< Range 2: 1V p-p
#define AD5933_VOLTAGE_0_4                  ((uint16_t)0x02)    //!< Range 3: 400mV p-p
#define AD5933_VOLTAGE_0_2                  ((uint16_t)0x01)    //!< Range 4: 200mV p-p
/** @} */

/**
 * @defgroup AD9533_GAIN Control Register PGA Gain Settings
 * 
 * PGA gain setting (Control register D8)
 * @{
 */
#define AD9533_GAIN_1                       ((uint16_t)0x01)    //!< PGA gain x1
#define AD9533_GAIN_5                       ((uint16_t)0x00)    //!< PGA gain x5
/** @} */

/**
 * @defgroup AD9533_CLOCK Control Register Clock Source Settings
 * 
 * Clock source (Control register D3)
 * @{
 */
#define AD9533_CLOCK_INTERNAL               ((uint16_t)0x00)    //!< Internal system clock (~16.667MHz)
#define AD9533_CLOCK_EXTERNAL               ((uint16_t)0x01)    //!< External system clock
/** @} */

/**
 * @defgroup AD9533_SETTL_MULT Settling Time Register Multiplier Values
 * 
 * Settling time multiplier (Settling time register D10:D9)
 * @{
 */
#define AD9533_SETTL_MULT_1                 ((uint16_t)0x00)    //!< Settling time multiplier of 1
#define AD9533_SETTL_MULT_2                 ((uint16_t)0x01)    //!< Settling time multiplier of 2
#define AD9533_SETTL_MULT_4                 ((uint16_t)0x03)    //!< Settling time multiplier of 4
/** @} */

/**
 * @defgroup AD9533_STATUS Status Register Bits
 * 
 * Status register bitmasks
 * @{
 */
#define AD9533_STATUS_VALID_TEMP            ((uint8_t)0x01)     //!< Valid temperature measurement status bit
#define AD9533_STATUS_VALID_IMPEDANCE       ((uint8_t)0x02)     //!< Valid real/imaginary data status bit
#define AD9533_STATUS_SWEEP_COMPLETE        ((uint8_t)0x04)     //!< Frequency sweep complete status bit
/** @} */

// Exported functions ---------------------------------------------------------



// ----------------------------------------------------------------------------

#endif /* AD5933_H_ */
