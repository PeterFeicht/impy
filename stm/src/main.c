/**
 * @file    main.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Main file for the impedance spectrometer firmware.
 */

// Includes -------------------------------------------------------------------
#include <math.h>
#include "main.h"

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

// Private functions ----------------------------------------------------------


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
