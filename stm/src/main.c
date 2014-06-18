/**
 * @file    main.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Main file for the impedance spectrometer firmware.
 */

// Includes -------------------------------------------------------------------
#include <math.h>
#include "main.h"

// Private function prototypes ------------------------------------------------
static void SetDefaults(void);

// Variables ------------------------------------------------------------------
USBD_HandleTypeDef hUsbDevice;
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi3;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim10;

// AD5933 driver values
static AD5933_Sweep sweep;
static AD5933_RangeSettings range;
static AD5933_GainFactorData gainData;
static AD5933_GainFactor gainFactor;
static uint32_t stopFreq;
static uint8_t port;
static uint8_t autorange;

// XXX should we allocate buffers dynamically?
static AD5933_ImpedanceData bufData[AD5933_MAX_NUM_INCREMENTS + 1];
static AD5933_ImpedancePolar bufPolar[AD5933_MAX_NUM_INCREMENTS + 1];
static uint8_t convertedPolar = 0;
static uint8_t interrupted = 0;
static uint32_t pointCount = 0;

// main and Interrupt handlers ------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

int main(int argc, char* argv[])
{
    // At this stage the system clock should have already been configured at high speed.
    MX_Init();
    Console_Init();
    SetDefaults();
    // TODO read settings from EEPROM
    
    // TODO always initialize AD5933 driver once stable
    if(HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_SET)
    {
        HAL_GPIO_WritePin(LED_PORT, LED_ORANGE, GPIO_PIN_SET);
        AD5933_Init(&hi2c1);
    }
    
    // Start timer for periodic interrupt generation
    HAL_TIM_Base_Start_IT(&htim3);
    
    while(1)
    {
        // Do stuff.
        HAL_GPIO_TogglePin(LED_PORT, LED_BLUE);
        HAL_Delay(600);
    }
}

#pragma GCC diagnostic pop

/**
 * Calls the appropriate functions for timer period interrupts.
 * 
 * @param htim Timer handle
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    static AD5933_Status prevStatus = AD_UNINIT;
    
    if(htim->Instance == TIM3)
    {
        AD5933_Status status = AD5933_TimerCallback();
        if(prevStatus != status)
        {
            if(status == AD_FINISH)
            {
                pointCount = AD5933_GetSweepCount();
                interrupted = 0;
                convertedPolar = 0;
            }
        }
        prevStatus = status;
    }
}

// Private functions ----------------------------------------------------------
static void SetDefaults(void)
{
    sweep.Num_Increments = 50;
    sweep.Start_Freq = 1000;
    stopFreq = 100000;
    sweep.Freq_Increment = (stopFreq - sweep.Start_Freq) / sweep.Num_Increments;
    sweep.Settling_Cycles = 16;
    sweep.Settling_Mult = AD5933_SETTL_MULT_1;
    
    range.PGA_Gain = AD5933_GAIN_1;
    range.Voltage_Range = AD5933_VOLTAGE_1;
    range.Attenuation = 1;
    range.Feedback_Value = 1000;
    
    gainData.impedance = 0;
    
    // TODO enable autorange by default
    autorange = 0;
}

// Exported functions ---------------------------------------------------------

// TODO implement calibration

/**
 * Sets the start frequency used for a sweep.
 * The value needs to be between {@link FREQ_MIN} and {@link FREQ_MAX}, and less than the stop frequency.
 * 
 * @param freq the frequency in Hz
 * @return {@link Board_Error} code
 */
Board_Error Board_SetStartFreq(uint32_t freq)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    if(freq < FREQ_MIN || freq > FREQ_MAX || freq >= stopFreq)
    {
        return BOARD_ERROR;
    }
    
    sweep.Start_Freq = freq;
    return BOARD_OK;
}

/**
 * Sets the stop frequency used for a sweep.
 * The value needs to be between {@link FREQ_MIN} and {@link FREQ_MAX}, and greater than the start frequency.
 * 
 * @param freq the frequency in Hz
 * @return {@link Board_Error} code
 */
Board_Error Board_SetStopFreq(uint32_t freq)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    if(freq < FREQ_MIN || freq > FREQ_MAX || freq <= sweep.Start_Freq)
    {
        return BOARD_ERROR;
    }
    
    stopFreq = freq;
    return BOARD_OK;
}

/**
 * Sets the number of frequency increments used for a sweep.
 * The value cannot be greater than the difference in start and stop frequency (the resolution is 1 Hz) and needs to be
 * in the range from {@code 0} to {@link AD5933_MAX_NUM_INCREMENTS}.
 * 
 * @param steps the number of points to measure
 * @return {@link Board_Error} code
 */
Board_Error Board_SetFreqSteps(uint16_t steps)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    if((stopFreq - sweep.Start_Freq) < steps || steps > AD5933_MAX_NUM_INCREMENTS)
    {
        return BOARD_ERROR;
    }
    
    sweep.Num_Increments = steps;
    return BOARD_OK;
}

/**
 * Sets the number of settling cycles used for a sweep.
 * The value of {@code cycles} needs to be in the range from {@code 0} to {@link AD5933_MAX_SETTL}, {@code multiplier}
 * can be {@code 1}, {@code 2} or {@code 4}.
 * 
 * @param cycles The number of settling cycles
 * @param multiplier Multiplier for the cycle number
 * @return {@link Board_Error} code
 */
Board_Error Board_SetSettlingCycles(uint16_t cycles, uint8_t multiplier)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    if(cycles > AD5933_MAX_SETTL)
    {
        return BOARD_ERROR;
    }
    
    sweep.Settling_Cycles = cycles;
    switch(multiplier)
    {
        case 1:
            sweep.Settling_Mult = AD5933_SETTL_MULT_1;
            break;
        case 2:
            sweep.Settling_Mult = AD5933_SETTL_MULT_2;
            break;
        case 4:
            sweep.Settling_Mult = AD5933_SETTL_MULT_4;
            break;
        default:
            return BOARD_ERROR;
    }
    
    return BOARD_OK;
}

/**
 * Sets the voltage range used for a sweep.
 * The value can be <i>0.2V</i>, <i>0.4V</i>, <i>1V</i> or <i>2V</i>, attenuated by the values in
 * {@link AD5933_ATTENUATION_PORT}.
 * 
 * @param range The output voltage in mV
 * @return {@link Board_Error} code
 */
Board_Error Board_SetVoltageRange(uint16_t voltage)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    
    static const uint16_t voltages[] = { 200, 400, 1000, 2000 };
    static const uint16_t voltage_values[] = {
        AD5933_VOLTAGE_0_2,
        AD5933_VOLTAGE_0_4,
        AD5933_VOLTAGE_1,
        AD5933_VOLTAGE_2
    };
    static const uint16_t attenuations[] = {
        AD5933_ATTENUATION_PORT_0,
        AD5933_ATTENUATION_PORT_1
    };
    
    for(uint32_t j = 0; j < NUMEL(attenuations); j++)
    {
        for(uint32_t k = 0; k < NUMEL(voltages); k++)
        {
            if(voltage == voltages[k] / attenuations[j])
            {
                range.Attenuation = attenuations[j];
                range.Voltage_Range = voltage_values[k];
                return BOARD_OK;
            }
        }
    }
    
    return BOARD_ERROR;
}

/**
 * Sets whether the x5 gain stage of the PGA is enabled. This setting is ignored, if autoranging is enabled.
 * 
 * @param enable {@code 0} to disable the gain stage, nonzero value to enable
 * @return {@link Board_Error} code
 */
Board_Error Board_SetPGA(uint8_t enable)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    
    if(!autorange)
    {
        range.PGA_Gain = (enable ? AD5933_GAIN_5 : AD5933_GAIN_1);
    }
    
    return BOARD_OK;
}

/**
 * Sets whether autoranging is enabled.
 * 
 * @param enable {@code 0} to disable autoranging, nonzero value to enable
 * @return {@code BOARD_OK}
 */
Board_Error Board_SetAutorange(uint8_t enable)
{
    autorange = enable;
    return BOARD_OK;
}

/**
 * Sets the value of the current feedback resistor in Ohms.
 * 
 * @param ohms the value of the resistor in Ohms
 * @return {@link Board_Error} code
 */
Board_Error Board_SetFeedback(uint32_t ohms)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    
    static const uint32_t feedbackValues[] = {
        AD5933_FEEDBACK_PORT_0, AD5933_FEEDBACK_PORT_1, AD5933_FEEDBACK_PORT_2, AD5933_FEEDBACK_PORT_3,
        AD5933_FEEDBACK_PORT_4, AD5933_FEEDBACK_PORT_5, AD5933_FEEDBACK_PORT_6, AD5933_FEEDBACK_PORT_7
    };
    
    if(!autorange)
    {
        uint32_t fb = 0;
        for(uint32_t j = 0; j < NUMEL(feedbackValues); j++)
        {
            if(ohms == feedbackValues[j])
            {
                fb = feedbackValues[j];
                break;
            }
        }
        if(!fb)
        {
            return BOARD_ERROR;
        }
        range.Feedback_Value = fb;
    }
    
    return BOARD_OK;
}

/**
 * Gets the current start frequency used for a sweep.
 */
uint32_t Board_GetStartFreq(void)
{
    return sweep.Start_Freq;
}

/**
 * Gets the current stop frequency used for a sweep.
 */
uint32_t Board_GetStopFreq(void)
{
    return stopFreq;
}

/**
 * Gets the current number of frequency increments used for a sweep.
 */
uint16_t Board_GetFreqSteps(void)
{
    return sweep.Num_Increments;
}

/**
 * Gets the current range settings.
 */
AD5933_RangeSettings* Board_GetRangeSettings(void)
{
    return &range;
}

/**
 * Gets the current number of settling cycles.
 */
uint16_t Board_GetSettlingCycles(void)
{
    switch(sweep.Settling_Mult)
    {
        case AD5933_SETTL_MULT_2:
            return sweep.Settling_Cycles << 1;
        case AD5933_SETTL_MULT_4:
            return sweep.Settling_Cycles << 2;
        default:
            return sweep.Settling_Cycles;
    }
}

/**
 * Gets whether autoranging is enabled or not.
 * 
 * @return {@code 0} if autoranging is disabled, a nonzero value otherwise
 */
uint8_t Board_GetAutorange(void)
{
    return autorange;
}

/**
 * Gets the current measurement status.
 * 
 * @param result Pointer to a status structure to be populated
 */
void Board_GetStatus(Board_Status *result)
{
    result->ad_status = AD5933_GetStatus();
    result->point = AD5933_GetSweepCount();
    result->totalPoints = sweep.Num_Increments;
    result->interrupted = interrupted;
    // FIXME return value for current sweep if running
    result->autorange = autorange;
}

/**
 * Gets a pointer to the converted measurement data in polar format.
 * 
 * @param count Pointer to a variable receiving the number of points in the buffer
 * @return Pointer to the data buffer
 */
const AD5933_ImpedancePolar* Board_GetDataPolar(uint32_t *count)
{
    if(!convertedPolar)
    {
        for(uint32_t j = 0; j < pointCount; j++)
        {
            bufPolar[j].Frequency = bufData[j].Frequency;
            bufPolar[j].Magnitude = AD5933_GetMagnitude(&bufData[j], &gainFactor);
            bufPolar[j].Angle = AD5933_GetPhase(&bufData[j], &gainFactor);
        }
        convertedPolar = 1;
    }
    
    *count = pointCount;
    return &bufPolar[0];
}

/**
 * Gets a pointer to the raw measurement data.
 * 
 * @param count Pointer to a variable receiving the number of points in the buffer
 * @return Pointer to the data buffer
 */
const AD5933_ImpedanceData* Board_GetDataRaw(uint32_t *count)
{
    *count = pointCount;
    return &bufData[0];
}

/**
 * Initiates a frequency sweep on the specified port.
 * 
 * @param port Port number for the sweep, needs to be in the range {@link PORT_MIN} to {@link PORT_MAX}
 * @return {@link Board_Error} code
 */
Board_Error Board_StartSweep(uint8_t port)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    // Uncomment if PORT_MIN is greater than 0
    if(/*port < PORT_MIN ||*/ port > PORT_MAX)
    {
        return BOARD_ERROR;
    }

    // Set output mux
    HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
    HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
    
    // TODO implement autorange
    sweep.Freq_Increment = (stopFreq - sweep.Start_Freq) / sweep.Num_Increments;
    AD5933_Error ok = AD5933_MeasureImpedance(&sweep, &range, &bufData[0]);
    return (ok == AD_OK ? BOARD_OK : BOARD_ERROR);
}

/**
 * Stops a currently running frequency measurement, if any.
 * 
 * @return {@link BOARD_OK}
 */
Board_Error Board_StopSweep(void)
{
    interrupted = (AD5933_GetStatus() == AD_MEASURE_IMPEDANCE);
    AD5933_Reset();
    return BOARD_OK;
}

/**
 * Gets the port of the active (or last) sweep.
 */
uint8_t Board_GetPort(void)
{
    return port;
}

/**
 * Measures a single frequency point on the specified port with the current range settings.
 * 
 * @param port The port to measure on
 * @param freq The frequency to measure
 * @param result Pointer to a structure receiving the converted impedance value
 * @return {@link Board_Error} code
 */
Board_Error Board_MeasureSingleFrequency(uint8_t port, uint32_t freq, AD5933_ImpedancePolar *result)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    // Uncomment if PORT_MIN is greater than 0
    if(freq < FREQ_MIN || freq > FREQ_MAX || /*port < PORT_MIN ||*/ port > PORT_MAX || result == NULL)
    {
        return BOARD_ERROR;
    }
    
    AD5933_ImpedanceData buffer;
    AD5933_Sweep sw = sweep;
    sw.Start_Freq = freq;
    // TODO determine value
    sw.Num_Increments = 0;
    
    // TODO implement autorange
    AD5933_MeasureImpedance(&sw, &range, &buffer);
    while(AD5933_GetStatus() != AD_FINISH)
    {
        HAL_Delay(2);
    }
    
    result->Frequency = freq;
    result->Magnitude = AD5933_GetMagnitude(&buffer, &gainFactor);
    result->Angle = AD5933_GetPhase(&buffer, &gainFactor);
    
    return BOARD_OK;
}

/**
 * Initiates a temperature measurement from the specified source and returns the value measured.
 * This function blocks until a value is available.
 * 
 * @param what Which temperature to measure
 * @return The measured temperature, or {@code NaN} in case of an error
 */
float Board_MeasureTemperature(Board_TemperatureSource what)
{
    float temp;
    
    switch(what)
    {
        case TEMP_AD5933:
            if(AD5933_MeasureTemperature(&temp) != AD_OK)
            {
                return NAN;
            }
            // TODO use callbacks
            while(AD5933_GetStatus() == AD_MEASURE_TEMP)
            {
                HAL_Delay(1);
            }
            break;
            
        default:
            return NAN;
    }
    
    return temp;
}

/**
 * Initiates a calibration measurement with the specified calibration resistor.
 * 
 * @param ohms Calibration resistor value in Ohms, must be one of the {@link CAL_PORT} values.
 * @return {@link Board_Error} code
 */
Board_Error Board_Calibrate(uint32_t ohms)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        return BOARD_BUSY;
    }
    
    static const uint32_t calibrationValues[] = {
        CAL_PORT_10, CAL_PORT_11, CAL_PORT_12, CAL_PORT_13, CAL_PORT_14, CAL_PORT_15
    };
    
    if(!autorange)
    {
        uint8_t cal = 0;
        for(uint32_t j = 0; j < NUMEL(calibrationValues) && calibrationValues[j]; j++)
        {
            if(ohms == calibrationValues[j])
            {
                cal = CAL_PORT_MIN + j;
                gainData.impedance = calibrationValues[j];
                break;
            }
        }
        if(!cal)
        {
            return BOARD_ERROR;
        }
        
        // Set calibration frequencies midway between center and start/stop frequency
        gainData.is_2point = 1;
        gainData.freq1 = sweep.Start_Freq + ((stopFreq - sweep.Start_Freq) >> 2);
        gainData.freq2 = stopFreq - ((stopFreq - sweep.Start_Freq) >> 2);
        if(gainData.freq1 == gainData.freq2)
        {
            gainData.freq1 = sweep.Start_Freq;
            gainData.freq2 = stopFreq;
        }
        
        cal = cal & AD725_MASK_PORT;
        HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
        HAL_SPI_Transmit(&hspi3, &cal, 1, BOARD_SPI_TIMEOUT);
        HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
        
        AD5933_Calibrate(&gainData, &range);
        // TODO use callback
        while(AD5933_GetStatus() == AD_CALIBRATE)
        {
            HAL_Delay(1);
        }
        AD5933_CalculateGainFactor(&gainData, &gainFactor);
    }
    
    return BOARD_OK;
}

// ----------------------------------------------------------------------------
