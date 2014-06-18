/**
 * @file    stm32f4xx_it.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Interrupt service routines.
 * 
 * This file contains some of the interrupt service routines that mostly only call HAL functions.
 */

// Includes -------------------------------------------------------------------
#include "stm32f4xx_hal.h"
#include "main.h"
#include "usbd_conf.h"

// Functions ------------------------------------------------------------------

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/**
 * Gets register values from the stack after a hard fault and enters an endless loop.
 * 
 * @param pulFaultStackAddress Pointer to the stack address
 */
void prvGetRegistersFromStack(uint32_t *pulFaultStackAddress)
{
    /*
     * These are volatile to try and prevent the compiler/linker optimizing them
     * away as the variables never actually get used.  If the debugger won't show
     * the values of the variables, make them global my moving their declaration
     * outside of this function.
     */
    volatile uint32_t r0;
    volatile uint32_t r1;
    volatile uint32_t r2;
    volatile uint32_t r3;
    volatile uint32_t r12;
    volatile uint32_t lr;   // Link register.
    volatile uint32_t pc;   // Program counter.
    volatile uint32_t psr;  // Program status register.
    
    r0 = pulFaultStackAddress[0];
    r1 = pulFaultStackAddress[1];
    r2 = pulFaultStackAddress[2];
    r3 = pulFaultStackAddress[3];
    
    r12 = pulFaultStackAddress[4];
    lr = pulFaultStackAddress[5];
    pc = pulFaultStackAddress[6];
    psr = pulFaultStackAddress[7];
    
    // When the following line is hit, the variables contain the register values.
    while(1)
    {
        
    }
}
#pragma GCC diagnostic pop

// Interrupt Handlers ---------------------------------------------------------

/**
 * This fault handler calls the function {@link prvGetRegistersFromStack} to get register values from the stack and be
 * able to see their values in the debugger.
 */
void __attribute__((naked)) HardFault_Handler(void)
{
    __asm volatile
    (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler2_address_const                            \n"
        " bx r2                                                     \n"
        " handler2_address_const: .word prvGetRegistersFromStack    \n"
    );
}

/**
 * This function handles the System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * This function handles I2C1 event interrupt.
 */
void I2C1_EV_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

/**
 * This function handles SPI3 global interrupt.
 */
void SPI3_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(SPI3_IRQn);
    HAL_SPI_IRQHandler(&hspi3);
}

/**
 * This function handles TIM3 global interrupt.
 */
void TIM3_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    HAL_TIM_IRQHandler(&htim3);
}

/**
 * This function handles USB On The Go FS global interrupt.
 */
void OTG_FS_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(OTG_FS_IRQn);
    HAL_PCD_IRQHandler(&hpcd_FS);
}

// ----------------------------------------------------------------------------
