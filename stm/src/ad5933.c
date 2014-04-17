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
static HAL_StatusTypeDef AD5933_WriteReg(uint16_t MemAddress, uint8_t *pData, uint16_t Size);
static HAL_StatusTypeDef AD5933_ReadReg(uint16_t MemAddress, uint8_t *pData, uint16_t Size);
static uint8_t AD5933_ReadStatus();
static uint32_t AD5933_CalcFrequencyReg(uint32_t freq);
static void AD5933_StartMeasurement(AD5933_RangeSettings *range, uint32_t freq_start, uint32_t freq_step,
        uint16_t num_incr, uint16_t settl);

// Private variables ----------------------------------------------------------
static AD5933_Status status = AD_UNINIT;
static I2C_HandleTypeDef *i2cHandle = NULL;
static float *pTemperature = NULL;
static AD5933_ImpedanceData *pBuffer;
static AD5933_Sweep sweep_spec;
static AD5933_RangeSettings range_spec;
static uint16_t sweep_count;
static AD5933_GainFactorData *pGainData;

// Private functions ----------------------------------------------------------

/**
 * Writes data to a AD5933 device register. 
 * @param MemAddress Register address to write to
 * @param pData Pointer to data
 * @param Size Number of bytes to write
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_WriteReg(uint16_t MemAddress, uint8_t *pData, uint16_t Size)
{
    return HAL_I2C_Mem_Write(i2cHandle, AD5933_ADDR, MemAddress, 1, pData, Size, AD5933_I2C_TIMEOUT);
}

/**
 * Reads data from a AD5933 device register. 
 * @param MemAddress Register address to read from
 * @param pData Pointer to buffer
 * @param Size Number of bytes to read
 * @return HAL status code
 */
static HAL_StatusTypeDef AD5933_ReadReg(uint16_t MemAddress, uint8_t *pData, uint16_t Size)
{
    return HAL_I2C_Mem_Read(i2cHandle, AD5933_ADDR, MemAddress, 1, pData, Size, AD5933_I2C_TIMEOUT);
}

/**
 * Reads the status register from the AD5933 device.
 * @return Contents of the status register
 */
static uint8_t AD5933_ReadStatus()
{
    uint8_t data = 0;
    
    AD5933_ReadReg(AD5933_STATUS_ADDR, &data, 1);
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
    uint64_t tmp = (1 << 27) * 4 * freq;
    return (uint32_t)(tmp / AD5933_CLK_FREQ);
}

/**
 * Sends the necessary commands to the AD5933 to initiate a frequency sweep.
 * 
 * @param range Pointer to voltage and gain settings
 * @param freq_start Start frequency register value
 * @param freq_step Frequency step register value
 * @param num_incr Number of increments
 * @param settl Settling time register value
 */
static void AD5933_StartMeasurement(AD5933_RangeSettings *range, uint32_t freq_start, uint32_t freq_step,
        uint16_t num_incr, uint16_t settl)
{
    uint16_t data;
    
    // Put device in standby and send range settings
    data = AD5933_FUNCTION_STANDBY | range->PGA_Gain | range->Voltage_Range;
    AD5933_WriteReg(AD5933_CTRL_H_ADDR, (uint8_t *)&data, 1);
    
    // Send sweep parameters
    AD5933_WriteReg(AD5933_START_FREQ_H_ADDR, ((uint8_t *)&freq_start) + 1, 3);
    AD5933_WriteReg(AD5933_FREQ_INCR_H_ADDR, ((uint8_t *)&freq_step) + 1, 3);
    AD5933_WriteReg(AD5933_NUM_INCR_H_ADDR, (uint8_t *)&num_incr, 2);
    AD5933_WriteReg(AD5933_SETTL_H_ADDR, (uint8_t *)&settl, 2);
    
    // Switch output on and start sweep after some settling time
    data = AD5933_FUNCTION_INIT_FREQ | range->PGA_Gain | range->Voltage_Range;
    AD5933_WriteReg(AD5933_CTRL_H_ADDR, (uint8_t *)&data, 1);
    HAL_Delay(5);
    data = AD5933_FUNCTION_START_SWEEP | range->PGA_Gain | range->Voltage_Range;
    AD5933_WriteReg(AD5933_CTRL_H_ADDR, (uint8_t *)&data, 1);
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
 * Initializes the driver with the specified I2C handle for communication.
 * 
 * @param i2c Pointer to an I2C handle structure that is to be used for communication with the AD5933
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_Init(I2C_HandleTypeDef *i2c)
{
    if(i2c == NULL)
    {
        return AD_ERROR;
    }
    
    i2cHandle = i2c;
    HAL_Delay(5);
    AD5933_Reset();
    
    return AD_OK;
}

/**
 * Resets the AD5933 and the driver to initialization state.
 * 
 * @return {@code AD_ERROR} if the driver has not been initialized, {@code AD_OK} otherwise
 */
AD5933_Error AD5933_Reset(void)
{
    uint16_t data = AD5933_FUNCTION_POWER_DOWN | AD5933_CTRL_RESET;
    
    if(status == AD_UNINIT)
    {
        return AD_ERROR;
    }
    
    // Reset first (low byte) and then power down (high byte)
    // The pointer fu done here is the same as used by the SWAPBYTE macro in usbd_def.h, so I assume this has to work
    AD5933_WriteReg(AD5933_CTRL_L_ADDR, ((uint8_t *)&data) + 1, 1);
    AD5933_WriteReg(AD5933_CTRL_H_ADDR, ((uint8_t *)&data), 1);
    status = AD_IDLE;
    
    return AD_OK;
}

/**
 * Initiates a frequency sweep over the specified range with the specified output buffer.
 * 
 * @param sweep The specifications to use for the sweep
 * @param range The specifications for PGA gain and voltage range
 * @param buffer Pointer to a buffer where measurement data is written (needs to be large enough for the specified
 *               number of samples)
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_MeasureImpedance(AD5933_Sweep *sweep, AD5933_RangeSettings *range, AD5933_ImpedanceData *buffer)
{
    uint32_t freq_start;
    uint32_t freq_step;
    uint16_t data;
    
    if(status == AD_UNINIT || sweep == NULL || buffer == NULL || range == NULL)
    {
        return AD_ERROR;
    }
    if(status != AD_IDLE && status != AD_FINISH)
    {
        return AD_BUSY;
    }
    
    // Check for out of range values
    // TODO maybe allow 0 increments to measure a single frequency (depends on AD5933 interpretation of the value)
    if(sweep->Freq_Increment == 0 || sweep->Num_Increments > AD5933_MAX_NUM_INCREMENTS || sweep->Num_Increments == 0)
    {
        return AD_ERROR;
    }
    
    pBuffer = buffer;
    sweep_spec = *sweep;
    range_spec = *range;
    sweep_count = 0;
    
    freq_start = AD5933_CalcFrequencyReg(sweep->Start_Freq);
    freq_step = AD5933_CalcFrequencyReg(sweep->Freq_Increment);
    
    // Check sweep range
    if(freq_start < AD5933_MIN_FREQ || (freq_start + freq_step * sweep->Num_Increments) > AD5933_MAX_FREQ)
    {
        return AD_ERROR;
    }
    
    data = sweep->Settling_Cycles | sweep->Settling_Mult;
    AD5933_StartMeasurement(range, freq_start, freq_step, sweep->Num_Increments, data);
    
    status = AD_MEASURE_IMPEDANCE;
    return AD_OK;
}

/**
 * Initiates a device temperature measurement on the AD5933 with the specified destination address.
 * 
 * @param destination The address where the temperature is written to
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_MeasureTemperature(float *destination)
{
    uint8_t data;
    
    if(status == AD_UNINIT || destination == NULL)
    {
        return AD_ERROR;
    }
    if(status != AD_IDLE && status != AD_FINISH)
    {
        return AD_BUSY;
    }
    
    pTemperature = destination;
    *pTemperature = NAN;
    data = HIBYTE(AD5933_FUNCTION_MEASURE_TEMP);
    AD5933_WriteReg(AD5933_CTRL_H_ADDR, &data, 1);
    status = AD_MEASURE_TEMP;
    
    return AD_OK;
}

/**
 * Initiates a frequency measurement of one or two points and saves the data to the specified structure.
 * Note that the specified structure needs the frequencies, and whether or not a two point calibration should be
 * performed, to already be set. The structure should not be changed during the measurement.
 * 
 * @param data Structure where measurements are written to
 * @return {@link AD5933_Error} code
 */
AD5933_Error AD5933_Calibrate(AD5933_GainFactorData *data, AD5933_RangeSettings *range)
{
    uint32_t freq_start;
    uint32_t freq_step = 0x10;
    
    if(status == AD_UNINIT || data == NULL || range == NULL)
    {
        return AD_ERROR;
    }
    if(status != AD_IDLE && status != AD_FINISH)
    {
        return AD_BUSY;
    }
    
    pGainData = data;
    range_spec = *range;
    sweep_count = 0;
    
    freq_start = AD5933_CalcFrequencyReg(data->freq1);
    if(data->is_2point)
    {
        if(data->freq2 <= data->freq1)
        {
            return AD_ERROR;
        }
        
        freq_step = AD5933_CalcFrequencyReg(data->freq2 - data->freq1);
    }
    
    // Number of increments doesn't really matter, we check sweep_count in the callback anyway
    AD5933_StartMeasurement(range, freq_start, freq_step, 2, 100);
    
    return AD_OK;
}

/**
 * This function should be called periodically to update measurement data and driver status.
 */
void AD5933_TIM_PeriodElapsedCallback(void)
{
    uint16_t data;
    uint8_t  dev_status;
    AD5933_ImpedanceData *buf;
    
    switch(status)
    {
        case AD_UNINIT:
        case AD_IDLE:
        case AD_FINISH:
            return;
            
        case AD_MEASURE_TEMP:
            if(AD5933_ReadStatus() & AD5933_STATUS_VALID_TEMP)
            {
                AD5933_ReadReg(AD5933_TEMP_H_ADDR, (uint8_t *)&data, 2);
                // Convert data to temperature value
                if(data & AD5933_TEMP_SIGN_BIT)
                {
                    *pTemperature = ((int16_t)data - (1 << 14)) / 32.0f;
                }
                else
                {
                    *pTemperature = data / 32.0f;
                }
                status = AD_FINISH;
            }
            break;
            
        case AD_MEASURE_IMPEDANCE:
            dev_status = AD5933_ReadStatus();
            if(dev_status & AD5933_STATUS_VALID_IMPEDANCE)
            {
                buf = pBuffer + sweep_count;
                
                // Read data and save it
                AD5933_ReadReg(AD5933_REAL_H_ADDR, (uint8_t *)&buf->Real, 2);
                AD5933_ReadReg(AD5933_IMAG_H_ADDR, (uint8_t *)&buf->Imag, 2);
                buf->Frequency = sweep_spec.Start_Freq + sweep_count * sweep_spec.Freq_Increment;
                sweep_count++;
                
                // Finish or measure next step
                if(dev_status & AD5933_STATUS_SWEEP_COMPLETE)
                {
                    status = AD_FINISH;
                }
                else
                {
                    data = AD5933_FUNCTION_INCREMENT_FREQ | range_spec.PGA_Gain | range_spec.Voltage_Range;
                    AD5933_WriteReg(AD5933_CTRL_H_ADDR, (uint8_t *)&data, 1);
                }
            }
            break;
            
        case AD_CALIBRATE:
            if(AD5933_GetStatus() & AD5933_STATUS_VALID_IMPEDANCE)
            {
                if(sweep_count == 0)
                {
                    // First point, read data and do second point if necessary
                    AD5933_ReadReg(AD5933_REAL_H_ADDR, (uint8_t *)&pGainData->real1, 2);
                    AD5933_ReadReg(AD5933_IMAG_H_ADDR, (uint8_t *)&pGainData->imag1, 2);
                    
                    if(pGainData->is_2point)
                    {
                        data = AD5933_FUNCTION_INCREMENT_FREQ | range_spec.PGA_Gain | range_spec.Voltage_Range;
                        AD5933_WriteReg(AD5933_CTRL_H_ADDR, (uint8_t *)&data, 1);
                        sweep_count++;
                    }
                    else
                    {
                        status = AD_FINISH;
                    }
                }
                else
                {
                    // Second point, read data and finish
                    AD5933_ReadReg(AD5933_REAL_H_ADDR, (uint8_t *)&pGainData->real2, 2);
                    AD5933_ReadReg(AD5933_IMAG_H_ADDR, (uint8_t *)&pGainData->imag2, 2);
                    status = AD_FINISH;
                }
            }
            break;
    }
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
void AD5933_CalculateGainFactor(AD5933_GainFactorData *data, AD5933_GainFactor *gf)
{
    // Gain factor is calculated by 1/(Magnitude * Impedance), with Magnitude being sqrt(Real^2 + Imag^2)
    float magnitude = hypotf(data->real1, data->imag1);
    float gain2;
    
    gf->is_2point = data->is_2point;
    gf->freq1 = data->freq1;
    gf->offset = 1.0f / (magnitude * (float)data->impedance);
    
    // TODO add phase calibration
    if(data->is_2point)
    {
        magnitude = hypotf(data->real2, data->imag2);
        gain2 = 1.0f / (magnitude * (float)data->impedance);
        
        gf->slope = (gain2 - gf->offset) / (float)(data->freq2 - data->freq1);
    }
}

/**
 * Calculates the actual impedance magnitude from a measurement data point.
 * 
 * @param data The measurement point to calculate the magnitude from
 * @param gain Gain factor structure to use
 * @return The magnitude of the impedance in Ohms
 */
float AD5933_GetMagnitude(AD5933_ImpedanceData *data, AD5933_GainFactor *gain)
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

// ----------------------------------------------------------------------------
