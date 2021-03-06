/**
 * @file    ad5933.h
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   This file contains register definitions and control function prototypes for the AD5933 driver.
 */

#ifndef AD5933_H_
#define AD5933_H_

// Includes -------------------------------------------------------------------
#include <stdint.h>
#include "stm32f4xx_hal.h"

// Exported type definitions --------------------------------------------------
/**
 * The possible states the AD5933 driver can be in.
 */
typedef enum
{
    AD_UNINIT = 0,                  //!< Driver has not been initialized
    AD_IDLE,                        //!< Driver has been initialized and is ready to start a measurement
    AD_FINISH_TEMP,                 //!< Driver has finished with a temperature measurement
    AD_FINISH_CALIB,                //!< Driver has finished with a calibration measurement
    AD_FINISH_IMPEDANCE,            //!< Driver has finished with an impedance measurement
    AD_CALIBRATE,                   //!< Driver is doing a calibration measurement
    AD_MEASURE_TEMP,                //!< Driver is doing a temperature measurement
    AD_MEASURE_IMPEDANCE,           //!< Driver is doing an impedance measurement
    AD_MEASURE_IMPEDANCE_AUTORANGE  //!< Driver is doing an impedance measurement with autoranging
} AD5933_Status;

/**
 * The possible outcomes of an operation.
 */
typedef enum
{
    AD_OK = 0,      //!< Indicates success
    AD_BUSY,        //!< Indicates that the driver is currently busy doing a measurement
    AD_ERROR        //!< Indicates an error condition
} AD5933_Error;

/**
 * Contains parameters of one sweep.
 */
typedef struct
{
    uint32_t Start_Freq;        //!< Start frequency for the sweep in Hz
    uint32_t Freq_Increment;    //!< Frequency increment for the sweep in Hz
    uint16_t Num_Increments;    //!< Number of frequency points for the sweep
    uint16_t Settling_Cycles;   //!< Number of settling cycles before a measurement
    uint16_t Settling_Mult;     //!< Settling time multiplier (one of the {@link AD5933_SETTL_MULT} values)
    uint16_t Averages;          //!< The number of averages for each frequency point
} AD5933_Sweep;

/**
 * Contains settings for the voltage range of the AD5933.
 */
typedef struct
{
    uint16_t  PGA_Gain;         //!< PGA gain setting (one of the {@link AD5933_GAIN} values)
    uint16_t  Voltage_Range;    //!< Voltage range setting (one of the {@link AD5933_VOLTAGE} values)
    uint16_t  Attenuation;      //!< The output voltage attenuation (one of the values in `board_config`)
    uint32_t  Feedback_Value;   //!< Value of the feedback resistor (one of the values in `board_config`)
} AD5933_RangeSettings;

/**
 * Contains raw impedance data as measured by the AD5933 (that is, the DFT values for the current flowing through the
 * unknown impedance).
 */
typedef struct
{
    uint32_t Frequency;     //!< Frequency of the data point in Hz
    int16_t  Real;          //!< Raw real data of the impedance
    int16_t  Imag;          //!< Raw imaginary data of the impedance
} AD5933_ImpedanceData;

/**
 * Contains an impedance in polar format (magnitude and angle).
 */
typedef struct
{
    uint32_t Frequency;     //!< Frequency of the data point in Hz
    float    Magnitude;     //!< Magnitude of the polar representation in Ohms
    float    Angle;         //!< Angle of the polar representation in rad
} AD5933_ImpedancePolar;

/**
 * Contains an impedance in Cartesian format (real and imaginary part).
 */
typedef struct
{
    uint32_t Frequency;     //!< Frequency of the data point in Hz
    float    Real;          //!< Real part of the impedance in Ohms
    float    Imag;          //!< Imaginary part of the impedance in Ohms
} AD5933_ImpedanceCartesian;

/**
 * Contains specifications for the frequency range of a calibration measurement.
 */
typedef struct
{
    uint32_t impedance;     //!< Impedance for the calibration
    uint32_t freq1;         //!< Lower frequency of the range that should be calibrated
    uint32_t freq2;         //!< Upper frequency of the range that should be calibrated
    uint8_t  is_2point;     //!< Whether a two point calibration should be performed
} AD5933_CalibrationSpec;

#define AD5933_NUM_CLOCKS   4   //!< The number of different AD5933 clocks that can be used

/**
 * Contains raw measurement data at different frequencies used for calibration.
 */
typedef struct
{
    uint32_t impedance;                             //!< Impedance used for the gain factor calibration
    AD5933_ImpedanceData point1[AD5933_NUM_CLOCKS]; //!< Calibration data for the first point in the clock ranges
    AD5933_ImpedanceData point2[AD5933_NUM_CLOCKS]; //!< Calibration data for the second point in the clock ranges
    uint8_t is_2point;                              //!< Whether this is single or two point calibration data
} AD5933_GainFactorData;

/**
 * Contains conversion factors used to convert measured data in a {@link AD5933_ImpedanceData} structure to a
 * {@link AD5933_ImpedancePolar} structure.
 */
typedef struct
{
    struct {
        float freq1;                //!< Frequency of the first calibration point, will be NaN for an unused point
        float offset;               //!< Calculated gain factor at first frequency
        float slope;                //!< Calculated gain factor slope for a two point calibration
        float phaseOffset;          //!< Calculated system phase at first frequency
        float phaseSlope;           //!< Calculated system phase slope for a two point calibration
    } ranges[AD5933_NUM_CLOCKS];    //!< Gain factor values for the different clock ranges
    uint8_t is_2point;              //!< Whether this is single or two point gain factor data
} AD5933_GainFactor;

// Macros ---------------------------------------------------------------------

#ifndef LOBYTE
#define LOBYTE(x)  ((uint8_t)(x & 0x00FF))
#endif
#ifndef HIBYTE
#define HIBYTE(x)  ((uint8_t)((x & 0xFF00) >> 8))
#endif
#define NUMEL(X)    (sizeof(X) / sizeof(X[0]))

// Constants ------------------------------------------------------------------

/**
 * I2C Address of the AD5933, cannot be changed
 */
#define AD5933_ADDR                         ((uint8_t)(0x0D << 1))

/**
 * Timeout in ms for I2C communication
 */
#define AD5933_I2C_TIMEOUT                  0x200

/**
 * The number of averages per frequency point used for calibration measurements
 */
#define AD5933_CALIB_AVERAGES               16

/**
 * Maximum sweep frequency
 */
#define AD5933_FREQ_MAX                     100000

/**
 * Minimum sweep frequency
 */
#define AD5933_FREQ_MIN                     10

/**
 * @defgroup AD5933_CLK Clock Source Configuration
 * @{
 */
#define AD5933_CLK_FREQ_INT                 0xFFFB40        //!< Internal clock frequency of the AD5933 (16.776MHz)
#define AD5933_CLK_FREQ_EXT_H               0x196E6A        //!< External high speed clock frequency (1.666MHz)
#define AD5933_CLK_FREQ_EXT_M               0x028B0A        //!< External medium speed clock frequency (166.666kHz)
#define AD5933_CLK_FREQ_EXT_L               0x00411A        //!< External low speed clock frequency (16.666kHz)

#define AD5933_CLK_LIM_INT                  10000           //!< Lowest sweep frequency for internal clock
#define AD5933_CLK_LIM_EXT_H                1000            //!< Lowest sweep frequency for high speed external clock
#define AD5933_CLK_LIM_EXT_M                100             //!< Lowest sweep frequency for medium speed external clock
#define AD5933_CLK_LIM_EXT_L                10              //!< Lowest sweep frequency for low speed external clock

#define AD5933_CLK_TIM_CHANNEL              TIM_CHANNEL_1   //!< Timer channel for external clock generation
#define AD5933_CLK_PSC_H                    0               //!< Timer prescaler value for high speed clock
#define AD5933_CLK_PSC_M                    9               //!< Timer prescaler value for medium speed clock
#define AD5933_CLK_PSC_L                    99              //!< Timer prescaler value for low speed clock
/** @} */

/**
 * @defgroup AD5933_COUPLING_GPIO Coupling Capacitor Charging Switch GPIO Definition
 * @{
 */
#define AD5933_COUPLING_GPIO_PORT           GPIOE
#define AD5933_COUPLING_GPIO_CLK_EN()       __GPIOE_CLK_ENABLE()
#define AD5933_COUPLING_GPIO_PIN            GPIO_PIN_4
/** @} */

/**
 * @defgroup AD5933_ATTENUATION_GPIO Attenuation Mux GPIO Definition
 * @{
 */
#define AD5933_ATTENUATION_GPIO_PORT        GPIOE
#define AD5933_ATTENUATION_GPIO_CLK_EN()    __GPIOE_CLK_ENABLE()
#define AD5933_ATTENUATION_GPIO_0           GPIO_PIN_5
#define AD5933_ATTENUATION_GPIO_1           GPIO_PIN_6
/** @} */

/**
 * @defgroup AD5933_FEEDBACK_GPIO Feedback Mux GPIO Definition
 * @{
 */
#define AD5933_FEEDBACK_GPIO_PORT           GPIOB
#define AD5933_FEEDBACK_GPIO_CLK_EN()       __GPIOB_CLK_ENABLE()
#define AD5933_FEEDBACK_GPIO_0              GPIO_PIN_7
#define AD5933_FEEDBACK_GPIO_1              GPIO_PIN_4
#define AD5933_FEEDBACK_GPIO_2              GPIO_PIN_5
/** @} */

/**
 * @defgroup AD5933_LED Measurement Notification LED GPIO Definition
 * 
 * If `AD5933_LED_USE` is defined, the specified pin is set high when a sweep is started and reset when it
 * finishes. It can be used to notify the user about a running measurement.
 * The GPIO pin needs to be configured externally.
 * @{ 
 */
#define AD5933_LED_USE
#define AD5933_LED_GPIO_PORT                GPIOD
#define AD5933_LED_GPIO_PIN                 GPIO_PIN_12
/** @} */

/*****************************************************************************
 *                              REGISTER MAPPING                             *
 *****************************************************************************/

/**
 * Control Register High Byte (16-bit)
 * Read/Write
 * Default value: 0xA0 (Power down)
 *
 *  + D15:D12   Control function, see {@link AD5933_FUNCTION}
 *  + D11       Not used
 *  + D10:D9    Output voltage range setting, see {@link AD5933_VOLTAGE}
 *  + D8        PGA gain setting, see {@link AD5933_GAIN}
 */
#define AD5933_CTRL_H_ADDR                  0x80

/**
 * Control Register Low Byte (16-bit)
 * Read/Write
 * Default value: 0x00
 *
 *  + D7:D5     Reserved, set to 0
 *  + D4        Reset
 *  + D3        Clock source setting, see {@link AD5933_CLOCK}
 *  + D2:D0     Reserved, set to 0
 */
#define AD5933_CTRL_L_ADDR                  0x81

/**
 * Start Frequency Register High Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 *
 * The value for this register is calculated by <i>2^27 * (4 * freq / clk)</i> where <i>clk</i> is the system clock
 * frequency and <i>freq</i> the desired frequency value.
 */
#define AD5933_START_FREQ_H_ADDR            0x82

/**
 * Start Frequency Register Mid Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 *
 * The value for this register is calculated by <i>2^27 * (4 * freq / clk)</i> where <i>clk</i> is the system clock
 * frequency and <i>freq</i> the desired frequency value.
 */
#define AD5933_START_FREQ_M_ADDR            0x83

/**
 * Start Frequency Register Low Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 *
 * The value for this register is calculated by <i>2^27 * (4 * freq / clk)</i> where <i>clk</i> is the system clock
 * frequency and <i>freq</i> the desired frequency value.
 */
#define AD5933_START_FREQ_L_ADDR            0x84

/**
 * Frequency Increment Register High Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 *
 * The value for this register is calculated by <i>2^27 * (4 * freq / clk)</i> where <i>clk</i> is the system clock
 * frequency and <i>freq</i> the desired frequency value.
 */
#define AD5933_FREQ_INCR_H_ADDR             0x85

/**
 * Frequency Increment Register Mid Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 *
 * The value for this register is calculated by <i>2^27 * (4 * freq / clk)</i> where <i>clk</i> is the system clock
 * frequency and <i>freq</i> the desired frequency value.
 */
#define AD5933_FREQ_INCR_M_ADDR             0x86

/**
 * Frequency Increment Register Low Byte (24-bit unsigned)
 * Read/Write
 * Default value: none
 *
 * The value for this register is calculated by <i>2^27 * (4 * freq / clk)</i> where <i>clk</i> is the system clock
 * frequency and <i>freq</i> the desired frequency value.
 */
#define AD5933_FREQ_INCR_L_ADDR             0x87

/**
 * Number of Increments Register High Byte (9-bit unsigned)
 * Read/Write
 * Default value: none
 *
 *  + D16:D9    Don't care
 *  + D8        MSB of value
 */
#define AD5933_NUM_INCR_H_ADDR              0x88

/**
 * Number of Increments Register Low Byte (9-bit unsigned)
 * Read/Write
 * Default value: none
 *
 *  + D7:D0     LSB of value
 */
#define AD5933_NUM_INCR_L_ADDR              0x89

/**
 * Number of Settling Time Cycles Register High Byte (2+9-bit unsigned)
 * Read/Write
 * Default value: none
 *
 *  + D15:D11   Don't care
 *  + D10:D9    Settling time multiplier, see {@link AD5933_SETTL_MULT}
 *  + D8        MSB of settling time cycles
 */
#define AD5933_SETTL_H_ADDR                 0x8A

/**
 * Number of Settling Time Cycles Register Low Byte (2+9-bit unsigned)
 * Read/Write
 * Default value: none
 *
 *  + D7:D0     LSB of settling time cycles
 */
#define AD5933_SETTL_L_ADDR                 0x8B

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
 * See also {@link AD5933_STATUS}
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
#define AD5933_TEMP_L_ADDR                  0x93

/**
 * Real Data Register High Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD5933_REAL_H_ADDR                  0x94

/**
 * Real Data Register Low Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD5933_REAL_L_ADDR                  0x95

/**
 * Imaginary Data Register High Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD5933_IMAG_H_ADDR                  0x96

/**
 * Imaginary Data Register Low Byte (16-bit signed)
 * Read only
 * Default value: none
 */
#define AD5933_IMAG_L_ADDR                  0x97

/*****************************************************************************
 *                              REGISTER VALUES                              *
 *****************************************************************************/

/**
 * @defgroup AD5933_FUNCTION Control Register Function Values
 *
 * Control functions (Control register D15:D12)
 * @{
 */
#define AD5933_FUNCTION_INIT_FREQ           ((uint16_t)(0x01 << 12))    //!< Initialize with start frequency
#define AD5933_FUNCTION_START_SWEEP         ((uint16_t)(0x02 << 12))    //!< Start frequency sweep
#define AD5933_FUNCTION_INCREMENT_FREQ      ((uint16_t)(0x03 << 12))    //!< Increment frequency
#define AD5933_FUNCTION_REPEAT_FREQ         ((uint16_t)(0x04 << 12))    //!< Repeat frequency
#define AD5933_FUNCTION_MEASURE_TEMP        ((uint16_t)(0x09 << 12))    //!< Measure temperature
#define AD5933_FUNCTION_POWER_DOWN          ((uint16_t)(0x0A << 12))    //!< Power-down mode
#define AD5933_FUNCTION_STANDBY             ((uint16_t)(0x0B << 12))    //!< Standby mode
/** @} */

/**
 * @defgroup AD5933_VOLTAGE Control Register Voltage Range Values
 *
 * Output voltage range (Control register D10:D9)
 * @{
 */
#define AD5933_VOLTAGE_2                    ((uint16_t)(0x00 << 9))     //!< Range 1: 2V p-p
#define AD5933_VOLTAGE_1                    ((uint16_t)(0x03 << 9))     //!< Range 2: 1V p-p
#define AD5933_VOLTAGE_0_4                  ((uint16_t)(0x02 << 9))     //!< Range 3: 400mV p-p
#define AD5933_VOLTAGE_0_2                  ((uint16_t)(0x01 << 9))     //!< Range 4: 200mV p-p
/** @} */

/**
 * @defgroup AD5933_GAIN Control Register PGA Gain Settings
 *
 * PGA gain setting (Control register D8)
 * @{
 */
#define AD5933_GAIN_1                       ((uint16_t)(0x01 << 8))     //!< PGA gain x1
#define AD5933_GAIN_5                       ((uint16_t)(0x00 << 8))     //!< PGA gain x5
/** @} */

/**
 * @defgroup AD5933_CLOCK Control Register Clock Source Settings
 *
 * Clock source (Control register D3)
 * @{
 */
#define AD5933_CLOCK_INTERNAL               ((uint16_t)(0x00 << 3))     //!< Internal system clock (~16.667MHz)
#define AD5933_CLOCK_EXTERNAL               ((uint16_t)(0x01 << 3))     //!< External system clock
/** @} */

/**
 * @defgroup AD5933_SETTL_MULT Settling Time Register Multiplier Values
 *
 * Settling time multiplier (Settling time register D10:D9)
 * @{
 */
#define AD5933_SETTL_MULT_1                 ((uint16_t)(0x00 << 9))     //!< Settling time multiplier of 1
#define AD5933_SETTL_MULT_2                 ((uint16_t)(0x01 << 9))     //!< Settling time multiplier of 2
#define AD5933_SETTL_MULT_4                 ((uint16_t)(0x03 << 9))     //!< Settling time multiplier of 4
/** @} */

/**
 * @defgroup AD5933_FLAGS Register Flags
 * @{
 */
#define AD5933_CTRL_RESET                   ((uint16_t)(0x01 << 4))     //!< Reset bit (Control register D4)
#define AD5933_TEMP_SIGN_BIT                ((uint16_t)(0x01 << 13))    //!< Temperature sign bit (D13)
/** @} */

/**
 * @defgroup AD5933_CMD Command codes
 * 
 * These are the command codes that need to be sent for the respective I2C transaction
 * @{
 */
#define AD5933_CMD_SET_ADDRESS              ((uint8_t)0xB0)     //!< Set address pointer command code
#define AD5933_CMD_BLOCK_WRITE              ((uint8_t)0xA0)     //!< Block write command code
#define AD5933_CMD_BLOCK_READ               ((uint8_t)0xA1)     //!< Block read command code
/** @} */

/**
 * @defgroup AD5933_STATUS Status Register Bits
 *
 * Status register bitmasks
 * @{
 */
#define AD5933_STATUS_VALID_TEMP            ((uint8_t)0x01)     //!< Valid temperature measurement status bit
#define AD5933_STATUS_VALID_IMPEDANCE       ((uint8_t)0x02)     //!< Valid real/imaginary data status bit
#define AD5933_STATUS_SWEEP_COMPLETE        ((uint8_t)0x04)     //!< Frequency sweep complete status bit
/** @} */

/*****************************************************************************
 *                           REGISTER VALUE RANGES                           *
 *****************************************************************************/

/**
 * Maximum number of settling time cycles (Settling time register D8:D0)
 * 
 * This can also be used to mask out the multiplier bits from a register value.
 */
#define AD5933_MAX_SETTL                    ((uint16_t)0x1FF)

/**
 * Maximum number of frequency increments (Number of increments register D8:D0)
 */
#define AD5933_MAX_NUM_INCREMENTS           ((uint16_t)0x1FF)

// Exported functions ---------------------------------------------------------

AD5933_Status AD5933_GetStatus(void);
uint8_t AD5933_IsBusy(void);
AD5933_Error AD5933_Init(I2C_HandleTypeDef *i2c, TIM_HandleTypeDef *tim);
AD5933_Error AD5933_Reset(void);
AD5933_Error AD5933_MeasureImpedance(const AD5933_Sweep *sweep, const AD5933_RangeSettings *range,
        AD5933_ImpedanceData *buffer);
uint16_t AD5933_GetSweepCount(void);
AD5933_Error AD5933_MeasureTemperature(float *destination);
AD5933_Error AD5933_Calibrate(const AD5933_CalibrationSpec *cal, const AD5933_RangeSettings *range,
        AD5933_GainFactorData *data);
AD5933_Status AD5933_TimerCallback(void);

void AD5933_CalculateGainFactor(const AD5933_GainFactorData *data, AD5933_GainFactor *gf);
float AD5933_GetMagnitude(const AD5933_ImpedanceData *data, const AD5933_GainFactor *gain);
float AD5933_GetPhase(const AD5933_ImpedanceData *data, const AD5933_GainFactor *gain);
void AD5933_ConvertPolarToCartesian(const AD5933_ImpedancePolar *polar, AD5933_ImpedanceCartesian *cart);

uint16_t AD5933_GetVoltageFromRegister(uint16_t reg);

#ifdef DEBUG
AD5933_Error AD5933_Debug_OutputFreq(uint32_t freq, const AD5933_RangeSettings *range);
#endif

// ----------------------------------------------------------------------------

#endif /* AD5933_H_ */
