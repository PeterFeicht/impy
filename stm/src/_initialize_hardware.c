//
// This file is part of the µOS++ III distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_cortex.h"

// ----------------------------------------------------------------------------

// The external clock frequency is specified as a preprocessor definition
// passed to the compiler via a command line option (see the 'C/C++ General' ->
// 'Paths and Symbols' -> the 'Symbols' tab, if you want to change it).
// The value selected during project creation was HSE_VALUE=8000000.
//
// The code to set the clock is at the end.
//
// Note1: The default clock settings assume that the HSE_VALUE is a multiple
// of 1MHz, and try to reach the maximum speed available for the
// board. It does NOT guarantee that the required USB clock of 48MHz is
// available. If you need this, please update the settings of PLL_M, PLL_N,
// PLL_P, PLL_Q to match your needs.
//
// Note2: The external memory controllers are not enabled. If needed, you
// have to define DATA_IN_ExtSRAM or DATA_IN_ExtSDRAM and to configure
// the memory banks in system/src/cmsis/system_stm32f4xx.c to match your needs.

// ----------------------------------------------------------------------------

// Forward declarations.

void __initialize_hardware(void);

extern void SystemClock_Config(void);

// ----------------------------------------------------------------------------

// This is the application hardware initialisation routine,
// redefined to add more inits.
//
// Called early from _start(), right after data & bss init, before
// constructors.
//
// After Reset the Cortex-M processor is in Thread mode,
// priority is Privileged, and the Stack is set to Main.

void __initialize_hardware(void)
{
    // Call the CSMSIS system initialisation routine.
    SystemInit();
    
#if defined (__VFP_FP__) && !defined (__SOFTFP__)
    
    // Enable the Cortex-M4 FPU only when -mfloat-abi=hard.
    // Code taken from Section 7.1, Cortex-M4 TRM (DDI0439C)
    
    // Set bits 20-23 to enable CP10 and CP11 coprocessor
    SCB->CPACR |= (0xF << 20);

#endif // (__VFP_FP__) && !(__SOFTFP__)
    
    // Initialise the HAL Library; it must be the first
    // instruction to be executed in the main program.
    HAL_Init();
    
    // Configure the system clock
    SystemClock_Config();
    
    // Set SysTick priority higher than HAL_Init does to avoid conflicts
    HAL_NVIC_SetPriority(SysTick_IRQn, 3, 0);
}

// ----------------------------------------------------------------------------
