/**
 * @file    ad5933.c
 * @author  Peter Feichtinger
 * @date    15.04.2014
 * @brief   This file contains functions to manage the AD5933 Impedance Converter Network Analyzer chip.
 */

// Includes -------------------------------------------------------------------
#include <math.h>
#include "ad5933.h"

// Private function prototypes ------------------------------------------------
static HAL_StatusTypeDef AD5933_SetAddress(uint8_t MemAddress);
static HAL_StatusTypeDef AD5933_Write8(uint8_t MemAddress, uint8_t value);
static HAL_StatusTypeDef AD5933_Write16(uint8_t MemAddress, uint16_t value);
static HAL_StatusTypeDef AD5933_Write24(uint8_t MemAddress, uint32_t value);
static HAL_StatusTypeDef AD5933_Read16(uint8_t MemAddress, uint16_t *destination);
static HAL_StatusTypeDef AD5933_WriteFunction(uint16_t code);
static uint8_t AD5933_ReadStatus();
static uint32_t AD5933_CalcFrequencyReg(uint32_t freq);
static AD5933_Error AD5933_StartMeasurement(const AD5933_RangeSettings *range, uint32_t freq_start, uint32_t freq_step,
        uint16_t num_incr, uint16_t settl);

// Private variables ----------------------------------------------------------
static volatile AD5933_Status status = AD_UNINIT;
static I2C_HandleTypeDef *i2cHandle = NULL;
static AD5933_Sweep sweep_spec;             //!< Local copy of the sweep specification
static AD5933_RangeSettings range_spec;     //!< Local copy of the range specification
static volatile uint16_t sweep_count;       //!< Variable to keep track of the number of measured points
static volatile uint16_t wait_coupl;        //!< Time to wait for coupling capacitor to charge, or 0 to not wait
static volatile uint32_t wait_tick;         //!< SysTick value where we started waiting
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
 * Writes an 8-bit value to a AD5933 device register.
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
 * Writes a 16-bit value to a AD5933 device register with the correct endianness.
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
 * Writes a 24-bit value to a AD5933 device register with the correct endianness.
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
 * Reads a 16-bit value from a AD5933 device register with the correct endianness.
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
 * Writes the specified function code to the control register, together with the current range settings.
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
 * Calculates the frequency register value corresponding to the specified frequency at internal clock frequency.
 * 
 * @param freq The frequency to calculate the register value for
 * @return The value that can be written to the device register
 */
static uint32_t AD5933_CalcFrequencyReg(uint32_t freq)
{
    uint64_t tmp = (1 << 27) * 4 * (uint64_t)freq;
    return (uint32_t)(tmp / AD5933_CLK_FREQ);
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
    uint32_t start_reg;
    uint32_t step_reg;
    
    static const uint16_t attenuation[] = {
        AD5933_ATTENUATION_PORT_0,
        AD5933_ATTENUATION_PORT_1,
        AD5933_ATTENUATION_PORT_2,
        AD5933_ATTENUATION_PORT_3
    };
    static const uint32_t feedback[] = {
        AD5933_FEEDBACK_PORT_0,
        AD5933_FEEDBACK_PORT_1,
        AD5933_FEEDBACK_PORT_2,
        AD5933_FEEDBACK_PORT_3,
        AD5933_FEEDBACK_PORT_4,
        AD5933_FEEDBACK_PORT_5,
        AD5933_FEEDBACK_PORT_6,
        AD5933_FEEDBACK_PORT_7
    };
    static const GPIO_PinState SET = GPIO_PIN_SET;
    static const GPIO_PinState RESET = GPIO_PIN_RESET;
    
    // Find attenuation port with desired value
    for(j = 0; j < NUMEL(attenuation); j++)
    {
        if(attenuation[j] == range->Attenuation)
        {
            portAtt = j;
            break;
        }
    }
    if(j == NUMEL(attenuation) || attenuation[j] == 0)
    {
        return AD_ERROR;
    }
    
    // Find feedback port with desired value
    for(j = 0; j < NUMEL(feedback); j++)
    {
        if(feedback[j] == range->Feedback_Value)
        {
            portFb = j;
            break;
        }
    }
    if(j == NUMEL(feedback) || feedback[j] == 0)
    {
        return AD_ERROR;
    }
    
    // Calculate and check register values
    // TODO use external clock if necessary
    start_reg = AD5933_CalcFrequencyReg(freq_start);
    step_reg = AD5933_CalcFrequencyReg(freq_step);
    if(start_reg < AD5933_MIN_FREQ || (start_reg + step_reg * num_incr) > AD5933_MAX_FREQ)
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
    AD5933_WriteFunction(AD5933_FUNCTION_STANDBY);
    
    // Send sweep parameters
    AD5933_Write24(AD5933_START_FREQ_H_ADDR, start_reg);
    AD5933_Write24(AD5933_FREQ_INCR_H_ADDR, step_reg);
    AD5933_Write16(AD5933_NUM_INCR_H_ADDR, num_incr);
    AD5933_Write16(AD5933_SETTL_H_ADDR, settl);
    
    // Switch output on
    AD5933_WriteFunction(AD5933_FUNCTION_INIT_FREQ);
    
    // Start charging coupling capacitor, this is always needed, assuming the output was previously switched off
    HAL_GPIO_WritePin(AD5933_COUPLING_GPIO_PORT, AD5933_COUPLING_GPIO_PIN, GPIO_PIN_RESET);
    wait_coupl = AD5933_COUPLING_TAU * 4;
    wait_tick = HAL_GetTick();

    return AD_OK;
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
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_Init(I2C_HandleTypeDef *i2c)
{
    GPIO_InitTypeDef init;
    
    if(i2c == NULL)
    {
        return AD_ERROR;
    }
    
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
    HAL_Delay(5);
    AD5933_Write8(AD5933_CTRL_L_ADDR, LOBYTE(AD5933_CTRL_RESET));
    status = AD_IDLE;
    
    return AD_OK;
}

/**
 * Resets the AD5933 and the driver to initialization state.
 * 
 * @return {@code AD_ERROR} if the driver has not been initialized, {@code AD_OK} otherwise
 */
AD5933_Error AD5933_Reset(void)
{
    if(status == AD_UNINIT)
    {
        return AD_ERROR;
    }
    
    // Reset first (low byte) and then put in standby mode
    uint16_t data = AD5933_FUNCTION_STANDBY | AD5933_CTRL_RESET;
    AD5933_Write8(AD5933_CTRL_L_ADDR, LOBYTE(data));
    AD5933_Write8(AD5933_CTRL_H_ADDR, HIBYTE(data));
    status = AD_IDLE;
    
    return AD_OK;
}

/**
 * Initiates a frequency sweep over the specified range with the specified output buffer.
 * 
 * Note that if the number of frequency increments in {@code sweep} is zero, two points are measured regardless
 * because the AD5933 insists on doing so.
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
    
    if(status == AD_UNINIT || sweep == NULL || buffer == NULL || range == NULL)
    {
        return AD_ERROR;
    }
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
    sweep_count = 0;
    
    data = sweep->Settling_Cycles | sweep->Settling_Mult;
    ret = AD5933_StartMeasurement(range, sweep->Start_Freq, sweep->Freq_Increment, sweep->Num_Increments, data);
    
    if(ret != AD_ERROR)
    {
        status = AD_MEASURE_IMPEDANCE;
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
    if(status == AD_UNINIT || destination == NULL)
    {
        return AD_ERROR;
    }
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
 * Initiates a frequency measurement of one or two points and saves the data to the specified structure.
 * 
 * Note that the specified structure needs the frequencies, and whether or not a two point calibration should be
 * performed, to already be set. The structure should not be changed during the measurement.
 * 
 * @param data Structure where measurements are written to
 * @param range The specifications for PGA gain, voltage range, external attenuation and feedback resistor
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_Calibrate(AD5933_GainFactorData *data, const AD5933_RangeSettings *range)
{
    uint32_t freq_step = 10;
    AD5933_Error ret;
    
    if(status == AD_UNINIT || data == NULL || range == NULL)
    {
        return AD_ERROR;
    }
    if(AD5933_IsBusy())
    {
        return AD_BUSY;
    }
    
    pGainData = data;
    sweep_count = 0;
    
    if(data->is_2point)
    {
        if(data->freq2 <= data->freq1)
        {
            return AD_ERROR;
        }
        
        freq_step = data->freq2 - data->freq1;
    }
    
    // Number of increments doesn't really matter, we check sweep_count in the callback anyway
    ret = AD5933_StartMeasurement(range, data->freq1, freq_step, 1, 10);
    
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
    uint16_t data;
    uint8_t  dev_status;
    
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
            if(AD5933_ReadStatus() & AD5933_STATUS_VALID_TEMP)
            {
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
            break;
            
        case AD_MEASURE_IMPEDANCE:
            dev_status = AD5933_ReadStatus();
            if(dev_status & AD5933_STATUS_VALID_IMPEDANCE)
            {
                AD5933_ImpedanceData *buf = pBuffer + sweep_count;
                
                // Read data and save it
                AD5933_Read16(AD5933_REAL_H_ADDR, (uint16_t *)&buf->Real);
                AD5933_Read16(AD5933_IMAG_H_ADDR, (uint16_t *)&buf->Imag);
                buf->Frequency = sweep_spec.Start_Freq + sweep_count * sweep_spec.Freq_Increment;
                sweep_count++;
                
                // Finish or measure next step
                if(dev_status & AD5933_STATUS_SWEEP_COMPLETE)
                {
                    status = AD_FINISH_IMPEDANCE;
                }
                else
                {
                    AD5933_WriteFunction(AD5933_FUNCTION_INCREMENT_FREQ);
                }
            }
            break;
            
        case AD_CALIBRATE:
            if(AD5933_ReadStatus() & AD5933_STATUS_VALID_IMPEDANCE)
            {
                if(sweep_count == 0)
                {
                    // First point, read data and do second point if necessary
                    AD5933_Read16(AD5933_REAL_H_ADDR, (uint16_t *)&pGainData->real1);
                    AD5933_Read16(AD5933_IMAG_H_ADDR, (uint16_t *)&pGainData->imag1);
                    
                    if(pGainData->is_2point)
                    {
                        AD5933_WriteFunction(AD5933_FUNCTION_INCREMENT_FREQ);
                        sweep_count++;
                    }
                    else
                    {
                        status = AD_FINISH_CALIB;
                    }
                }
                else
                {
                    // Second point, read data and finish
                    AD5933_Read16(AD5933_REAL_H_ADDR, (uint16_t *)&pGainData->real2);
                    AD5933_Read16(AD5933_IMAG_H_ADDR, (uint16_t *)&pGainData->imag2);
                    status = AD_FINISH_CALIB;
                }
            }
            break;
    }
    
    return status;
}

/**
 * Calculates gain factor values from calibration measurement data.
 * 
 * The data can be one or two point calibration measurements.
 * The gain factor should be calibrated if any of the following parameters change:
 *  + Current-to-voltage gain setting resistor RFB
 *  + Output voltage range
 *  + PGA gain setting
 *  + Temperature (gain varies up to 1% over 150°C temperature, probably not relevant in a lab environment)
 * 
 * @param data The measurement data to calculate the gain factor from
 * @param gf Pointer to a gain factor structure to be populated
 */
void AD5933_CalculateGainFactor(const AD5933_GainFactorData *data, AD5933_GainFactor *gf)
{
    // Gain factor is calculated by 1/(Magnitude * Impedance), with Magnitude being sqrt(Real^2 + Imag^2)
    float magnitude = hypotf(data->real1, data->imag1);
    float gain2;
    // System phase can be directly calculated from real and imaginary parts
    float phase = atan2f((float)data->imag1, (float)data->real1);
    
    gf->is_2point = data->is_2point;
    gf->freq1 = data->freq1;
    gf->offset = 1.0f / (magnitude * (float)data->impedance);
    gf->phaseOffset = phase;
    
    if(data->is_2point)
    {
        magnitude = hypotf(data->real2, data->imag2);
        gain2 = 1.0f / (magnitude * (float)data->impedance);
        gf->slope = (gain2 - gf->offset) / (float)(data->freq2 - data->freq1);
        
        phase = atan2f((float)data->imag2, (float)data->real2);
        gf->phaseSlope = (phase - gf->phaseOffset) / (float)(data->freq2 - data->freq1);
    }
    else
    {
        gf->slope = 0.0f;
        gf->phaseSlope = 0.0f;
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
    // Actual impedance is calculated by 1/(Magnitude * Gain Factor), with Magnitude being sqrt(Real^2 + Imag^2)
    float magnitude = hypotf(data->Real, data->Imag);
    float gain_2point = gain->offset;
    
    if(gain->is_2point)
    {
        gain_2point += gain->slope * (data->Frequency - gain->freq1);
    }
    
    return 1.0f / (gain_2point * magnitude);
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
    float phase = atan2f(data->Imag, data->Real);
    float phase_2point = gain->phaseOffset;
    
    if(gain->is_2point)
    {
        phase_2point += gain->phaseSlope * (data->Frequency - gain->freq1);
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
 * @return The corresponding output voltage in mV, or {@code 0} for invalid values
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

// ----------------------------------------------------------------------------
