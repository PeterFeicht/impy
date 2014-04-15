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

// Private variables ----------------------------------------------------------
static AD5933_Status status = AD_UNINIT;
static I2C_HandleTypeDef *i2cHandle = NULL;

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
    uint8_t data;
    
    AD5933_ReadReg(AD5933_STATUS_ADDR, &data, 1);
    return data;
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
    HAL_I2C_Mem_Write(i2cHandle, AD5933_ADDR, AD5933_CTRL_L_ADDR, 1, ((uint8_t *)&data) + 1, 1, AD5933_I2C_TIMEOUT);
    HAL_I2C_Mem_Write(i2cHandle, AD5933_ADDR, AD5933_CTRL_H_ADDR, 1, ((uint8_t *)&data), 1, AD5933_I2C_TIMEOUT);
    status = AD_IDLE;
    
    return AD_OK;
}

/**
 * This function should be called periodically to update measurement data and driver status.
 */
void AD5933_TIM_PeriodElapsedCallback(void)
{
    switch(status)
    {
        case AD_UNINIT:
        case AD_IDLE:
        case AD_FINISH:
            return;
        case AD_MEASURE_TEMP:
            // TODO handle temperature measurement update
            break;
        case AD_MEASURE_IMPEDANCE:
            // TODO handle impedance measurement update
            break;
        case AD_CALIBRATE:
            // TODO handle calibration update
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
