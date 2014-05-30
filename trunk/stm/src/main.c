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

// AD5933 driver values
static AD5933_Sweep sweep;
static AD5933_RangeSettings range;
static AD5933_GainFactorData gainData;
static AD5933_GainFactor gainFactor;
static uint32_t stopFreq;
static uint8_t port;
static uint8_t autorange;

static AD5933_ImpedanceData bufData[AD5933_MAX_NUM_INCREMENTS + 1];
static AD5933_ImpedancePolar bufPolar[AD5933_MAX_NUM_INCREMENTS + 1];

// Private functions ----------------------------------------------------------
static void SetDefaults(void)
{
    sweep.Num_Increments = 50;
    sweep.Start_Freq = 1000;
    stopFreq = 100000;
    sweep.Freq_Increment = (stopFreq - sweep.Start_Freq) / sweep.Num_Increments;
    // TODO settling
    
    range.PGA_Gain = AD5933_GAIN_1;
    range.Voltage_Range = AD5933_VOLTAGE_1;
    range.Attenuation = 1;
    
    // TODO enable autorange by default
    autorange = 0;
}

// ----- main() ---------------------------------------------------------------

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
    
    while(1)
    {
        // Do stuff.
        HAL_GPIO_TogglePin(LED_PORT, LED_BLUE);
        HAL_Delay(600);
    }
}

#pragma GCC diagnostic pop

// Exported functions ---------------------------------------------------------

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
 * Note that the returned structure can change if autoranging is enabled and a sweep is running.
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
}

/**
 * Gets the port of the active (or last) sweep.
 */
uint8_t Board_GetPort(void)
{
    return port;
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

// ----------------------------------------------------------------------------
