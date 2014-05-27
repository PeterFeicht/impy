/**
 * @file    main.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Main file for the impedance spectrometer firmware.
 */

// Includes -------------------------------------------------------------------
#include "main.h"
#include "console.h"

// Variables ------------------------------------------------------------------
USBD_HandleTypeDef hUsbDevice;
I2C_HandleTypeDef hi2c1;

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

// ----------------------------------------------------------------------------
