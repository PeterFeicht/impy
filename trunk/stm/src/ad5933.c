/**
 * @file    ad5933.c
 * @author  Peter Feichtinger
 * @date    15.04.2014
 * @brief   This file contains functions to manage the AD5933 Impedance Converter Network Analyzer chip.
 */

// Includes -------------------------------------------------------------------
#include <math.h>
#include <assert.h>
#include "ad5933.h"
#include "main.h"

// Private type definitions ---------------------------------------------------
/**
 * Specifies the possible AD5933 clock sources.
 */
typedef enum
{
    AD_INTERNAL = 3,    //!< Internal clock
    AD_EXT_H = 2,       //!< External high speed clock (~1.666MHz)
    AD_EXT_M = 1,       //!< External medium speed clock (~166.666kHz)
    AD_EXT_L = 0        //!< External low speed clock (~16.666kHz)
} AD5933_ClockSource;

// Private function prototypes ------------------------------------------------
// AD5933 communication
static HAL_StatusTypeDef AD5933_SetAddress(uint8_t MemAddress);
static HAL_StatusTypeDef AD5933_Write8(uint8_t MemAddress, uint8_t value);
static HAL_StatusTypeDef AD5933_Write16(uint8_t MemAddress, uint16_t value);
static HAL_StatusTypeDef AD5933_Write24(uint8_t MemAddress, uint32_t value);
static HAL_StatusTypeDef AD5933_Read16(uint8_t MemAddress, uint16_t *destination);
static HAL_StatusTypeDef AD5933_WriteFunction(uint16_t code);
static uint8_t AD5933_ReadStatus();
// Misc
static uint32_t AD5933_CalcFrequencyReg(uint32_t freq, uint32_t clock);
static AD5933_Error AD5933_StartMeasurement(const AD5933_RangeSettings *range, uint32_t freq_start, uint32_t freq_step,
        uint16_t num_incr, uint16_t settl);
static void AD5933_SetClock(uint32_t freq_start, uint32_t freq_step);
static AD5933_ClockSource AD5933_GetClockSource(uint32_t freq);
static void AD5933_DoClockChange(uint32_t freq_start, uint32_t freq_step, uint32_t increments);
// Timer callbacks
static AD5933_Status AD5933_CallbackTemp(void);
static AD5933_Status AD5933_CallbackImpedance(void);
static AD5933_Status AD5933_CallbackCalibrate(void);

// Private variables ----------------------------------------------------------
static volatile AD5933_Status status = AD_UNINIT;
static I2C_HandleTypeDef *i2cHandle = NULL;
static TIM_HandleTypeDef *timHandle = NULL;
static AD5933_Sweep sweep_spec;             //!< Local copy of the sweep specification
static AD5933_RangeSettings range_spec;     //!< Local copy of the range specification
static volatile uint16_t sweep_count;       //!< The number of measured points
static volatile uint32_t sweep_freq;        //!< The current frequency
static volatile uint16_t avg_count;         //!< The averages recorded
static volatile int32_t sum_real;           //!< Sum of the real values for averaging
static volatile int32_t sum_imag;           //!< Sum of the imaginary values for averaging
static volatile uint16_t wait_coupl;        //!< Time to wait for coupling capacitor to charge, or 0 to not wait
static volatile uint32_t wait_tick;         //!< SysTick value where we started waiting
/**
 * Current clock source to determine if a change is needed during a sweep
 */
static volatile AD5933_ClockSource clk_source;
/**
 * Pointer to buffer that receives the result of a running calibration measurement
 */
static AD5933_GainFactorData *pGainData;
/**
 * Pointer to variable that receives the result of a running temperature measurement
 */
static float *pTemperature = NULL;
/**
 * Pointer to buffer that receives the results of a running frequency sweep
 */
static AD5933_ImpedanceData *pBuffer;

// Private functions ----------------------------------------------------------

/**
 * Sets the AD5933 address pointer to the specified address.
 * 
 * @param MemAddress The address to set
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_SetAddress(uint8_t MemAddress)
{
    return HAL_I2C_Mem_Write(i2cHandle, AD5933_ADDR, AD5933_CMD_SET_ADDRESS, 1, &MemAddress, 1, AD5933_I2C_TIMEOUT);
}

/**
 * Writes an 8-bit value to an AD5933 device register.
 * 
 * @param MemAddress Register address to write to
 * @param value Value to write
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_Write8(uint8_t MemAddress, uint8_t value)
{
    return HAL_I2C_Mem_Write(i2cHandle, AD5933_ADDR, MemAddress, 1, &value, 1, AD5933_I2C_TIMEOUT);
}

/**
 * Writes a 16-bit value to an AD5933 device register with the correct endianness.
 * 
 * @param MemAddress Register address to write to
 * @param value Value to write
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_Write16(uint8_t MemAddress, uint16_t value)
{
    HAL_StatusTypeDef ret = AD5933_SetAddress(MemAddress);
    if(ret != HAL_OK)
    {
        return ret;
    }
    
    // AD5933 block write operation: transfer block write command, byte count and data
    uint8_t data[4];
    data[0] = AD5933_CMD_BLOCK_WRITE;
    data[1] = 2;
    data[2] = HIBYTE(value);
    data[3] = LOBYTE(value);
    
    return HAL_I2C_Master_Transmit(i2cHandle, AD5933_ADDR, data, sizeof(data), AD5933_I2C_TIMEOUT);
}

/**
 * Writes a 24-bit value to an AD5933 device register with the correct endianness.
 * 
 * @param MemAddress Register address to write to
 * @param value Value to write
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_Write24(uint8_t MemAddress, uint32_t value)
{
    HAL_StatusTypeDef ret = AD5933_SetAddress(MemAddress);
    if(ret != HAL_OK)
    {
        return ret;
    }
    
    // AD5933 block write operation: transfer block write command, byte count and data
    uint8_t data[5];
    data[0] = AD5933_CMD_BLOCK_WRITE;
    data[1] = 3;
    data[2] = (uint8_t)((value >> 16) & 0xFF);
    data[3] = (uint8_t)((value >> 8) & 0xFF);
    data[4] = (uint8_t)(value & 0xFF);
    
    return HAL_I2C_Master_Transmit(i2cHandle, AD5933_ADDR, data, sizeof(data), AD5933_I2C_TIMEOUT);
}

/**
 * Reads a 16-bit value from an AD5933 device register with the correct endianness.
 * 
 * @param MemAddress Register address to read from
 * @param destination The address where the value is written to
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_Read16(uint8_t MemAddress, uint16_t *destination)
{
    HAL_StatusTypeDef ret = AD5933_SetAddress(MemAddress);
    if(ret != HAL_OK)
    {
        return ret;
    }
    
    // AD5933 block read operation: transfer block read command and byte count, after start condition read data
    uint16_t tmp = 0;
    uint16_t cmd = ((uint16_t)AD5933_CMD_BLOCK_READ << 8) | 2;
    ret = HAL_I2C_Mem_Read(i2cHandle, AD5933_ADDR, cmd, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&tmp, 2, AD5933_I2C_TIMEOUT);
#ifdef __ARMEB__
    *destination = tmp;
#else
    *destination = __REV16(tmp);
#endif
    return ret;
}

/**
 * Writes the specified function code to the AD5933 control register, together with the current range settings.
 * 
 * @param code One of the {@link AD5933_FUNCTION} codes
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_WriteFunction(uint16_t code)
{
    uint16_t data = code | range_spec.Voltage_Range | range_spec.PGA_Gain;
    return AD5933_Write8(AD5933_CTRL_H_ADDR, HIBYTE(data));
}

/**
 * Reads the status register from the AD5933 device.
 * 
 * @return Contents of the status register
 */
static uint8_t AD5933_ReadStatus()
{
    uint8_t data = 0;
    
    AD5933_SetAddress(AD5933_STATUS_ADDR);
    HAL_I2C_Master_Receive(i2cHandle, AD5933_ADDR, &data, 1, AD5933_I2C_TIMEOUT);
    return data;
}

/**
 * Calculates the frequency register value corresponding to the specified output and clock frequencies.
 * 
 * @param freq The output frequency to calculate the register value for
 * @param clock The clock frequency used for the AD5933
 * @return The value that can be written to the device register
 */
static uint32_t AD5933_CalcFrequencyReg(uint32_t freq, uint32_t clock)
{
    uint64_t tmp = (1 << 27) * 4 * (uint64_t)freq;
    return (uint32_t)(tmp / clock);
}

/**
 * Sends the necessary commands to the AD5933 to initiate a frequency sweep.
 * 
 * @param range Pointer to voltage, gain, attenuation and feedback settings
 * @param freq_start Start frequency in Hz
 * @param freq_step Frequency step in Hz
 * @param num_incr Number of increments
 * @param settl Settling time register value
 * @return {@link AD5933_Error} code
 */
static AD5933_Error AD5933_StartMeasurement(const AD5933_RangeSettings *range, uint32_t freq_start, uint32_t freq_step,
        uint16_t num_incr, uint16_t settl)
{
    uint8_t portAtt = 0;
    uint8_t portFb = 0;
    uint8_t j;
    
    static const GPIO_PinState SET = GPIO_PIN_SET;
    static const GPIO_PinState RESET = GPIO_PIN_RESET;
    
    // Find attenuation port with desired value
    for(j = 0; j < NUMEL(board_config.attenuations); j++)
    {
        if(board_config.attenuations[j] == range->Attenuation)
        {
            portAtt = j;
            break;
        }
    }
    if(j == NUMEL(board_config.attenuations) || board_config.attenuations[j] == 0)
    {
        return AD_ERROR;
    }
    
    // Find feedback port with desired value
    for(j = 0; j < NUMEL(board_config.feedback_resistors); j++)
    {
        if(board_config.feedback_resistors[j] == range->Feedback_Value)
        {
            portFb = j;
            break;
        }
    }
    if(j == NUMEL(board_config.feedback_resistors) || board_config.feedback_resistors[j] == 0)
    {
        return AD_ERROR;
    }
    
    if(freq_start < AD5933_FREQ_MIN || (freq_start + freq_step * num_incr) > AD5933_FREQ_MAX)
    {
        return AD_ERROR;
    }
    
    // Set attenuator and feedback mux
    HAL_GPIO_WritePin(AD5933_ATTENUATION_GPIO_PORT, AD5933_ATTENUATION_GPIO_0, ((portAtt & (1 << 0)) ? SET : RESET));
    HAL_GPIO_WritePin(AD5933_ATTENUATION_GPIO_PORT, AD5933_ATTENUATION_GPIO_1, ((portAtt & (1 << 1)) ? SET : RESET));
    HAL_GPIO_WritePin(AD5933_FEEDBACK_GPIO_PORT, AD5933_FEEDBACK_GPIO_0, ((portFb & (1 << 0)) ? SET : RESET));
    HAL_GPIO_WritePin(AD5933_FEEDBACK_GPIO_PORT, AD5933_FEEDBACK_GPIO_1, ((portFb & (1 << 1)) ? SET : RESET));
    HAL_GPIO_WritePin(AD5933_FEEDBACK_GPIO_PORT, AD5933_FEEDBACK_GPIO_2, ((portFb & (1 << 2)) ? SET : RESET));
    
    range_spec = *range;
    sweep_count = 0;
    sweep_freq = freq_start;
    avg_count = 0;
    sum_real = 0;
    sum_imag = 0;
    AD5933_WriteFunction(AD5933_FUNCTION_STANDBY);
    
    // Send sweep parameters and set clock
    AD5933_SetClock(freq_start, freq_step);
    AD5933_Write16(AD5933_NUM_INCR_H_ADDR, num_incr);
    AD5933_Write16(AD5933_SETTL_H_ADDR, settl);
    
    // Switch output on
    AD5933_WriteFunction(AD5933_FUNCTION_INIT_FREQ);
    
    // Start charging coupling capacitor, this is always needed, assuming the output was previously switched off
    HAL_GPIO_WritePin(AD5933_COUPLING_GPIO_PORT, AD5933_COUPLING_GPIO_PIN, GPIO_PIN_RESET);
    wait_coupl = board_config.coupling_tau * 4;
    wait_tick = HAL_GetTick();

    return AD_OK;
}

/**
 * Sets the clock needed for the specified frequency and programs the AD5933 registers.
 * 
 * @param freq_start The start frequency, this will determine the clock range needed
 * @param freq_step The frequency step
 */
static void AD5933_SetClock(uint32_t freq_start, uint32_t freq_step)
{
    uint32_t clk;
    uint8_t ctrl;
    
    assert_param(freq_start >= AD5933_FREQ_MIN);
    
    if(freq_start >= AD5933_CLK_LIM_INT)
    {
        // Internal clock can be used
        HAL_TIM_OC_Stop(timHandle, AD5933_CLK_TIM_CHANNEL);
        clk = AD5933_CLK_FREQ_INT;
        ctrl = AD5933_CLOCK_INTERNAL;
        clk_source = AD_INTERNAL;
    }
    else
    {
        uint16_t psc;
        if(freq_start >= AD5933_CLK_LIM_EXT_H)
        {
            psc = AD5933_CLK_PSC_H;
            clk = AD5933_CLK_FREQ_EXT_H;
            clk_source = AD_EXT_H;
        }
        else if(freq_start >= AD5933_CLK_LIM_EXT_M)
        {
            psc = AD5933_CLK_PSC_M;
            clk = AD5933_CLK_FREQ_EXT_M;
            clk_source = AD_EXT_M;
        }
        else
        {
            psc = AD5933_CLK_PSC_L;
            clk = AD5933_CLK_FREQ_EXT_L;
            clk_source = AD_EXT_L;
        }
        
        HAL_TIM_OC_Stop(timHandle, AD5933_CLK_TIM_CHANNEL);
        timHandle->Instance->PSC = psc;
        HAL_TIM_OC_Start(timHandle, AD5933_CLK_TIM_CHANNEL);
        ctrl = AD5933_CLOCK_EXTERNAL;
    }
    
    AD5933_Write8(AD5933_CTRL_L_ADDR, ctrl);
    AD5933_Write24(AD5933_START_FREQ_H_ADDR, AD5933_CalcFrequencyReg(freq_start, clk));
    AD5933_Write24(AD5933_FREQ_INCR_H_ADDR, AD5933_CalcFrequencyReg(freq_step, clk));
}

/**
 * Determines the clock source needed for the specified frequency.
 * 
 * @param freq The frequency
 * @return The clock source needed to measure the frequency
 */
static AD5933_ClockSource AD5933_GetClockSource(uint32_t freq)
{
    assert_param(freq >= AD5933_FREQ_MIN);
    
    if(freq >= AD5933_CLK_LIM_INT)
    {
        return AD_INTERNAL;
    }
    else if(freq >= AD5933_CLK_LIM_EXT_H)
    {
        return AD_EXT_H;
    }
    else if(freq >= AD5933_CLK_LIM_EXT_M)
    {
        return AD_EXT_M;
    }
    else // freq >= AD5933_CLK_LIM_EXT_L
    {
        return AD_EXT_L;
    }
}

/**
 * Changes the AD5933 clock source and sets the specified start frequency and number of increments.
 * 
 * A clock change is a new sweep to the AD5933, so the start frequency and number of increments needs to be set again
 * to the new values.
 * 
 * @param freq_start The new start frequency, that is the next frequency to be measured
 * @param freq_step The frequency step
 * @param increments The new number of increments, this is the total number less the number of already measured steps
 */
static void AD5933_DoClockChange(uint32_t freq_start, uint32_t freq_step, uint32_t increments)
{
    /*
     * For a clock change we need to set new values for almost everything, but we don't need to charge
     * the coupling capacitor, so that's a plus:
     *  + Set the frequency registers, obviously
     *  + Set the number of increments, since to the AD5933 we're starting a new sweep
     *  + Start a new sweep
     */
    AD5933_WriteFunction(AD5933_FUNCTION_STANDBY);
    AD5933_SetClock(freq_start, freq_step);
    AD5933_Write16(AD5933_NUM_INCR_H_ADDR, increments);
    AD5933_WriteFunction(AD5933_FUNCTION_INIT_FREQ);
    // Sometimes the AD5933 will lock up, waiting here seems to prevent this
    HAL_Delay(5);
    AD5933_WriteFunction(AD5933_FUNCTION_START_SWEEP);
}

/**
 * Timer callback when measuring temperature.
 * 
 * @return The (new) AD5933 status
 */
static AD5933_Status AD5933_CallbackTemp(void)
{
    if(AD5933_ReadStatus() & AD5933_STATUS_VALID_TEMP)
    {
        uint16_t data;
        AD5933_Read16(AD5933_TEMP_H_ADDR, &data);
        // Convert data to temperature value
        if(data & AD5933_TEMP_SIGN_BIT)
        {
            *pTemperature = ((int16_t)data - (1 << 14)) / 32.0f;
        }
        else
        {
            *pTemperature = data / 32.0f;
        }
        status = AD_FINISH_TEMP;
    }
    
    return status;
}

/**
 * Timer callback when measuring impedance.
 * 
 * @return The (new) AD5933 status
 */
static AD5933_Status AD5933_CallbackImpedance(void)
{
    uint8_t dev_status = AD5933_ReadStatus();
    
    if(dev_status & AD5933_STATUS_VALID_IMPEDANCE)
    {
        int16_t tmp_real, tmp_imag;
        AD5933_Read16(AD5933_REAL_H_ADDR, (uint16_t *)&tmp_real);
        AD5933_Read16(AD5933_IMAG_H_ADDR, (uint16_t *)&tmp_imag);
        sum_real += tmp_real;
        sum_imag += tmp_imag;
        avg_count++;
        
        if(avg_count == sweep_spec.Averages)
        {
            // Finished with frequency point, save average to result buffer
            AD5933_ImpedanceData *buf = pBuffer + sweep_count;
            buf->Real = sum_real / sweep_spec.Averages;
            buf->Imag = sum_imag / sweep_spec.Averages;
            buf->Frequency = sweep_freq;
            sweep_count++;
            sweep_freq += sweep_spec.Freq_Increment;
            
            // Finish or measure next step
            if(dev_status & AD5933_STATUS_SWEEP_COMPLETE)
            {
                status = AD_FINISH_IMPEDANCE;
#ifdef AD5933_LED_USE
                HAL_GPIO_WritePin(AD5933_LED_GPIO_PORT, AD5933_LED_GPIO_PIN, GPIO_PIN_RESET);
#endif
            }
            else
            {
                if(clk_source != AD5933_GetClockSource(sweep_freq))
                {
                    AD5933_DoClockChange(sweep_freq, sweep_spec.Freq_Increment,
                            sweep_spec.Num_Increments - sweep_count);
                }
                else
                {
                    AD5933_WriteFunction(AD5933_FUNCTION_INCREMENT_FREQ);
                }
                avg_count = 0;
                sum_real = 0;
                sum_imag = 0;
            }
        }
        else
        {
            AD5933_WriteFunction(AD5933_FUNCTION_REPEAT_FREQ);
        }
    }
    
    return status;
}

/**
 * Timer callback when calibrating.
 * 
 * @return The (new) AD5933 status
 */
static AD5933_Status AD5933_CallbackCalibrate(void)
{
    uint8_t dev_status = AD5933_ReadStatus();
    
    if(dev_status & AD5933_STATUS_VALID_IMPEDANCE)
    {
        int16_t tmp_real, tmp_imag;
        AD5933_Read16(AD5933_REAL_H_ADDR, (uint16_t *)&tmp_real);
        AD5933_Read16(AD5933_IMAG_H_ADDR, (uint16_t *)&tmp_imag);
        sum_real += tmp_real;
        sum_imag += tmp_imag;
        avg_count++;
        
        if(avg_count == AD5933_CALIB_AVERAGES)
        {
            uint32_t range = sweep_count;
            if(dev_status & AD5933_STATUS_SWEEP_COMPLETE)
            {
                // Second point measured
                pGainData->point2[range].Real = sum_real / AD5933_CALIB_AVERAGES;
                pGainData->point2[range].Imag = sum_imag / AD5933_CALIB_AVERAGES;
            }
            else
            {
                // First point measured
                pGainData->point1[range].Real = sum_real / AD5933_CALIB_AVERAGES;
                pGainData->point1[range].Imag = sum_imag / AD5933_CALIB_AVERAGES;
                
                if(pGainData->is_2point)
                {
                    AD5933_WriteFunction(AD5933_FUNCTION_INCREMENT_FREQ);
                    avg_count = 0;
                    sum_real = 0;
                    sum_imag = 0;
                    return status;
                }
            }
            
            // Current clock range finished, check for next one
            sweep_count = ++range;
            if(range < AD5933_NUM_CLOCKS && pGainData->point1[range].Frequency != 0)
            {
                uint32_t step = 10;
                if(pGainData->is_2point)
                {
                    step = pGainData->point2[range].Frequency - pGainData->point1[range].Frequency;
                }
                AD5933_DoClockChange(pGainData->point1[range].Frequency, step, 1);
                avg_count = 0;
                sum_real = 0;
                sum_imag = 0;
            }
            else
            {
                status = AD_FINISH_CALIB;
            }
        }
        else
        {
            AD5933_WriteFunction(AD5933_FUNCTION_REPEAT_FREQ);
        }
    }
    
    return status;
}

// Exported functions ---------------------------------------------------------

/**
 * Gets the current driver status.
 */
AD5933_Status AD5933_GetStatus(void)
{
    return status;
}

/**
 * Gets a value indicating whether the driver is busy or can start a new measurement.
 */
uint8_t AD5933_IsBusy(void)
{
    AD5933_Status tmp = status;
    return (tmp != AD_FINISH_CALIB &&
            tmp != AD_FINISH_TEMP &&
            tmp != AD_FINISH_IMPEDANCE &&
            tmp != AD_IDLE);
}

/**
 * Initializes the driver with the specified I2C handle for communication.
 * 
 * @param i2c Pointer to an I2C handle structure that is to be used for communication with the AD5933
 * @param tim Pointer to a timer handle structure that is to be used for the external clock source
 * @return `AD_OK`
 */
AD5933_Error AD5933_Init(I2C_HandleTypeDef *i2c, TIM_HandleTypeDef *tim)
{
    GPIO_InitTypeDef init;
    
    assert_param(i2c != NULL);
    assert_param(tim != NULL);
    
    // Configure attenuation and feedback mux GPIO pins
    init.Pin = AD5933_ATTENUATION_GPIO_0 | AD5933_ATTENUATION_GPIO_1;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Speed = GPIO_SPEED_MEDIUM;
    init.Pull = GPIO_NOPULL;
    AD5933_ATTENUATION_GPIO_CLK_EN();
    HAL_GPIO_Init(AD5933_ATTENUATION_GPIO_PORT, &init);
    
    init.Pin = AD5933_FEEDBACK_GPIO_0 | AD5933_FEEDBACK_GPIO_1 | AD5933_FEEDBACK_GPIO_2;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Speed = GPIO_SPEED_MEDIUM;
    init.Pull = GPIO_NOPULL;
    AD5933_FEEDBACK_GPIO_CLK_EN();
    HAL_GPIO_Init(AD5933_FEEDBACK_GPIO_PORT, &init);
    
    // Configure coupling capacitor charge switch GPIO pin
    init.Pin = AD5933_COUPLING_GPIO_PIN;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Speed = GPIO_SPEED_MEDIUM;
    init.Pull = GPIO_NOPULL;
    AD5933_COUPLING_GPIO_CLK_EN();
    HAL_GPIO_Init(AD5933_COUPLING_GPIO_PORT, &init);
    HAL_GPIO_WritePin(AD5933_COUPLING_GPIO_PORT, AD5933_COUPLING_GPIO_PIN, GPIO_PIN_SET);
    
    i2cHandle = i2c;
    timHandle = tim;
    HAL_Delay(5);
    AD5933_Write8(AD5933_CTRL_L_ADDR, LOBYTE(AD5933_CTRL_RESET));
    status = AD_IDLE;
    
    return AD_OK;
}

/**
 * Resets the AD5933 and the driver to initialization state.
 * 
 * @return `AD_OK`
 */
AD5933_Error AD5933_Reset(void)
{
    assert(status != AD_UNINIT);
    
    // Reset first (low byte) and then put in standby mode
    uint16_t data = AD5933_FUNCTION_STANDBY | AD5933_CTRL_RESET;
    AD5933_Write8(AD5933_CTRL_L_ADDR, LOBYTE(data));
    AD5933_Write8(AD5933_CTRL_H_ADDR, HIBYTE(data));
    status = AD_IDLE;
    
#ifdef AD5933_LED_USE
    HAL_GPIO_WritePin(AD5933_LED_GPIO_PORT, AD5933_LED_GPIO_PIN, GPIO_PIN_RESET);
#endif
    
    return AD_OK;
}

/**
 * Initiates a frequency sweep over the specified range with the specified output buffer.
 * 
 * The number of frequency increments in `sweep` is the number of times the frequency is incremented, so one more
 * points than this value are measured. The minimum value is `1` since the AD5933 insists on measuring at least
 * two points.
 * 
 * @param sweep The specifications to use for the sweep
 * @param range The specifications for PGA gain, voltage range, external attenuation and feedback resistor
 * @param buffer Pointer to a buffer where measurement data is written (needs to be large enough for the specified
 *               number of samples)
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_MeasureImpedance(const AD5933_Sweep *sweep, const AD5933_RangeSettings *range,
        AD5933_ImpedanceData *buffer)
{
    uint16_t data;
    AD5933_Error ret;
    
    assert_param(sweep != NULL);
    assert_param(buffer != NULL);
    assert_param(range != NULL);
    assert(status != AD_UNINIT);
    
    if(AD5933_IsBusy())
    {
        return AD_BUSY;
    }
    
    // Although a frequency increment of 0 would be valid for the AD5933, it doesn't make much sense
    if(sweep->Freq_Increment == 0 || sweep->Num_Increments > AD5933_MAX_NUM_INCREMENTS)
    {
        return AD_ERROR;
    }
    
    pBuffer = buffer;
    sweep_spec = *sweep;
    
    data = sweep->Settling_Cycles | sweep->Settling_Mult;
    ret = AD5933_StartMeasurement(range, sweep->Start_Freq, sweep->Freq_Increment, sweep->Num_Increments, data);
    
    if(ret != AD_ERROR)
    {
        status = AD_MEASURE_IMPEDANCE;
        
#ifdef AD5933_LED_USE
        HAL_GPIO_WritePin(AD5933_LED_GPIO_PORT, AD5933_LED_GPIO_PIN, GPIO_PIN_SET);
#endif
    }
    return ret;
}

/**
 * Gets the number of data points already measured. This value only has meaning if a sweep is running.
 */
uint16_t AD5933_GetSweepCount(void)
{
    return sweep_count;
}

/**
 * Initiates a device temperature measurement on the AD5933 with the specified destination address.
 * 
 * @param destination The address where the temperature is written to
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_MeasureTemperature(float *destination)
{
    assert_param(destination != NULL);
    assert(status != AD_UNINIT);
    
    if(AD5933_IsBusy())
    {
        return AD_BUSY;
    }
    
    pTemperature = destination;
    *pTemperature = NAN;
    AD5933_WriteFunction(AD5933_FUNCTION_MEASURE_TEMP);
    status = AD_MEASURE_TEMP;
    
    return AD_OK;
}

/**
 * Initiates an impedance measurement of one or two points in different clock ranges and saves the data to the
 * specified structure.
 * 
 * Note that the frequency values in `cal` determine which clock sources will be used for calibration. A gain
 * factor obtained with one frequency range can only be used with measurements in this range.
 * 
 * @param cal The specifications for calibration impedance, frequency range and whether a two point calibration
 *            should be performed (highly recommended)
 * @param range The specifications for PGA gain, voltage range, external attenuation and feedback resistor
 * @param data Structure where measurements are written to
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_Calibrate(const AD5933_CalibrationSpec *cal, const AD5933_RangeSettings *range,
        AD5933_GainFactorData *data)
{
    AD5933_Error ret = AD_ERROR;
    // Frequency limits for the different clock ranges, from low to high
    const uint32_t limits[AD5933_NUM_CLOCKS + 1] = {
        AD5933_CLK_LIM_EXT_L,
        AD5933_CLK_LIM_EXT_M,
        AD5933_CLK_LIM_EXT_H,
        AD5933_CLK_LIM_INT,
        AD5933_FREQ_MAX
    };
    
    assert_param(cal != NULL);
    assert_param(range != NULL);
    assert_param(data != NULL);
    assert(status != AD_UNINIT);
    
    if(AD5933_IsBusy())
    {
        return AD_BUSY;
    }
    if(cal->is_2point && cal->freq2 <= cal->freq1)
    {
        return AD_ERROR;
    }
    
    pGainData = data;
    data->impedance = cal->impedance;
    data->is_2point = cal->is_2point;
    
    for(uint32_t j = 0; j < AD5933_NUM_CLOCKS; j++)
    {
        // Check if clock range and sweep range intersect
        if(cal->freq1 < limits[j + 1] && cal->freq2 >= limits[j])
        {
            // limits[j] is the lower, limits[j + 1] the upper limit of the current range
            uint32_t lower = (cal->freq1 < limits[j] ? limits[j] : cal->freq1);
            uint32_t upper = (cal->freq2 >= limits[j + 1] ? limits[j + 1] - 1 : cal->freq2);
            if(cal->is_2point)
            {
                data->point1[j].Frequency = lower + ((upper - lower) >> 2);
                data->point2[j].Frequency = upper - ((upper - lower) >> 2);
            }
            else
            {
                data->point1[j].Frequency = (upper + lower) >> 1;
                data->point2[j].Frequency = 0;
            }
        }
        else
        {
            data->point1[j].Frequency = 0;
            data->point2[j].Frequency = 0;
        }
    }
    
    for(uint32_t j = 0; j < AD5933_NUM_CLOCKS; j++)
    {
        if(data->point1[j].Frequency)
        {
            ret = AD5933_StartMeasurement(range, data->point1[j].Frequency,
                    (data->is_2point ? data->point2[j].Frequency - data->point1[j].Frequency : 10), 1, 10);
            sweep_count = j;
            break;
        }
    }
    
    if(ret != AD_ERROR)
    {
        status = AD_CALIBRATE;
    }
    return ret;
}

/**
 * This function should be called periodically to update measurement data and driver status.
 * 
 * @return The (new) AD5933 status
 */
AD5933_Status AD5933_TimerCallback(void)
{
    switch(status)
    {
        case AD_MEASURE_IMPEDANCE:
        case AD_MEASURE_IMPEDANCE_AUTORANGE:
        case AD_CALIBRATE:
            if(wait_coupl)
            {
                if(HAL_GetTick() - wait_tick > wait_coupl)
                {
                    wait_coupl = 0;
                    HAL_GPIO_WritePin(AD5933_COUPLING_GPIO_PORT, AD5933_COUPLING_GPIO_PIN, GPIO_PIN_SET);
                    
                    // Start sweep
                    AD5933_WriteFunction(AD5933_FUNCTION_START_SWEEP);
                }
                return status;
            }
            break;
        default:
            break;
    }
    
    // TODO handle autoranging
    switch(status)
    {
        case AD_UNINIT:
        case AD_IDLE:
        case AD_FINISH_CALIB:
        case AD_FINISH_TEMP:
        case AD_FINISH_IMPEDANCE:
            break;
            
        case AD_MEASURE_TEMP:
            return AD5933_CallbackTemp();
            
        case AD_MEASURE_IMPEDANCE:
            return AD5933_CallbackImpedance();
            
        case AD_CALIBRATE:
            return AD5933_CallbackCalibrate();
    }
    
    return status;
}

// Conversion functions -------------------------------------------------------

/**
 * Calculates gain factor values from calibration measurement data.
 * 
 * The data can be one or two point calibration measurements.
 * The gain factor should be calibrated if any of the following parameters change:
 *  + Current-to-voltage gain setting resistor RFB (feedback resistor)
 *  + Output voltage range
 *  + PGA gain setting
 *  + Temperature (gain varies up to 1% over 150°C temperature, probably not relevant in a lab environment)
 * 
 * @param data The measurement data to calculate the gain factor from
 * @param gf Pointer to a gain factor structure to be populated
 */
void AD5933_CalculateGainFactor(const AD5933_GainFactorData *data, AD5933_GainFactor *gf)
{
    gf->is_2point = data->is_2point;
    
    for(uint32_t j = 0; j < AD5933_NUM_CLOCKS; j++)
    {
        if(data->point1[j].Frequency == 0)
        {
            gf->ranges[j].freq1 = NAN;
            continue;
        }

        // Gain factor is calculated by (Magnitude * Impedance), with Magnitude being sqrt(Real^2 + Imag^2)
        float magnitude = hypotf(data->point1[j].Real, data->point1[j].Imag);
        float gain2;
        // System phase can be directly calculated from real and imaginary parts
        float phase = atan2(data->point1[j].Imag, data->point1[j].Real);
        
        gf->ranges[j].freq1 = data->point1[j].Frequency;
        gf->ranges[j].offset = magnitude * (float)data->impedance;
        gf->ranges[j].phaseOffset = phase;
        
        if(data->is_2point)
        {
            magnitude = hypotf(data->point2[j].Real, data->point2[j].Imag);
            gain2 = magnitude * (float)data->impedance;
            gf->ranges[j].slope =
                    (gain2 - gf->ranges[j].offset) / (data->point2[j].Frequency - data->point1[j].Frequency);
            
            phase = atan2f((float)data->point2[j].Imag, (float)data->point2[j].Real) - gf->ranges[j].phaseOffset;
            if(phase > M_PI)
                phase -= M_TWOPI;
            else if(phase < -M_PI)
                phase += M_TWOPI;
            gf->ranges[j].phaseSlope = phase / (data->point2[j].Frequency - data->point1[j].Frequency);
        }
        else
        {
            gf->ranges[j].slope = 0.0f;
            gf->ranges[j].phaseSlope = 0.0f;
        }
    }
}

/**
 * Calculates the actual impedance magnitude from a measurement data point.
 * 
 * @param data The measurement point to calculate the magnitude from
 * @param gain Gain factor structure to use
 * @return The magnitude of the impedance in Ohms
 */
float AD5933_GetMagnitude(const AD5933_ImpedanceData *data, const AD5933_GainFactor *gain)
{
    uint32_t range = AD5933_GetClockSource(data->Frequency);
    // Actual impedance is calculated by (Gain Factor / Magnitude), with Magnitude being sqrt(Real^2 + Imag^2)
    float magnitude = hypotf(data->Real, data->Imag);
    float gain_2point = gain->ranges[range].offset;
    
    if(gain->is_2point)
    {
        gain_2point += gain->ranges[range].slope * (data->Frequency - gain->ranges[range].freq1);
    }
    
    return gain_2point / magnitude;
}

/**
 * Calculates the actual impedance phase from a measurement data point.
 * 
 * @param data The measurement point to calculate the phase from
 * @param gain Gain factor structure to use
 * @return The phase in radians (in the range of -pi to +pi)
 */
float AD5933_GetPhase(const AD5933_ImpedanceData *data, const AD5933_GainFactor *gain)
{
    uint32_t range = AD5933_GetClockSource(data->Frequency);
    float phase = atan2f(data->Imag, data->Real);
    float phase_2point = gain->ranges[range].phaseOffset;
    
    if(gain->is_2point)
    {
        phase_2point += gain->ranges[range].phaseSlope * (data->Frequency - gain->ranges[range].freq1);
    }
    
    phase -= phase_2point;
    
    // Make sure the corrected result is in the range of -pi to pi
    if(phase > M_PI)
        return phase - M_TWOPI;
    else if(phase < -M_PI)
        return phase + M_TWOPI;
    else
        return phase;
}

/**
 * Converts an impedance value from the polar to the Cartesian representation.
 * 
 * @param polar Pointer to a polar impedance structure
 * @param cart Pointer to a Cartesian structure to be populated
 */
void AD5933_ConvertPolarToCartesian(const AD5933_ImpedancePolar *polar, AD5933_ImpedanceCartesian *cart)
{
    float real;
    float imag;
#ifdef __GNUC__
    sincosf(polar->Angle, &imag, &real);
#else
    real = cosf(polar->Angle);
    imag = sinf(polar->Angle);
#endif
    
    cart->Frequency = polar->Frequency;
    cart->Real = polar->Magnitude * real;
    cart->Imag = polar->Magnitude * imag;
}

/**
 * Gets the corresponding voltage in mV for a voltage range register value (one of the {@link AD5933_VOLTAGE} values).
 * 
 * @param reg The voltage range register value
 * @return The corresponding output voltage in mV, or `0` for invalid values
 */
uint16_t AD5933_GetVoltageFromRegister(uint16_t reg)
{
    switch(reg)
    {
        case AD5933_VOLTAGE_0_2:
            return 200;
        case AD5933_VOLTAGE_0_4:
            return 400;
        case AD5933_VOLTAGE_1:
            return 1000;
        case AD5933_VOLTAGE_2:
            return 2000;
        default:
            return 0;
    }
}

// Debugging functions --------------------------------------------------------
#ifdef DEBUG

/**
 * Programs the AD5933 to output a single frequency.
 * 
 * @param freq The frequency in Hz
 * @param range The specifications for PGA gain, voltage range, external attenuation and feedback resistor
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_Debug_OutputFreq(uint32_t freq, const AD5933_RangeSettings *range)
{
    AD5933_Error ret;
    
    assert_param(range != NULL);
    assert(status != AD_UNINIT);
    
    if(AD5933_IsBusy())
    {
        return AD_BUSY;
    }
    
    ret = AD5933_StartMeasurement(range, freq, 1, 1, 10);
    HAL_Delay(10);
    HAL_GPIO_WritePin(AD5933_COUPLING_GPIO_PORT, AD5933_COUPLING_GPIO_PIN, GPIO_PIN_SET);
    
    return ret;
}

#endif

// ----------------------------------------------------------------------------
