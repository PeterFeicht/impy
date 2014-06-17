/**
 * @file    mx_init.c
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   Initialization for clock, GPIO and peripherals.
 */

// Includes -------------------------------------------------------------------
#include "mx_init.h"
#include "main.h"

// Private function prototypes ------------------------------------------------
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI3_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM10_Init(void);
static void MX_USB_DEVICE_Init(void);

// Exported functions ---------------------------------------------------------

/**
 * System Clock Configuration (MX)
 */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    
    // Enable HSE clock input (8MHz) and activate PLL with HSE as source (120MHz)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 240;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 5;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    // Select PLL as system clock source and configure the PCLK1 and PCLK2 clock dividers
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
    
#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
    // Configure PLLI2S used for the Ethernet RMII clock and MCO2 output
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    PeriphClkInitStruct.PLLI2S.PLLI2SN = 200;
    PeriphClkInitStruct.PLLI2S.PLLI2SR = 4;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
    HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_PLLI2SCLK, RCC_MCODIV_1);
#endif
}

/**
 * Calls all the MX initialization functions.
 */
void MX_Init()
{
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI3_Init();
    MX_TIM3_Init();
    MX_TIM10_Init();
    MX_USB_DEVICE_Init();
}

// Private functions ----------------------------------------------------------

/**
 * Initialize I2C1, generated by MX.
 */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_16_9;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;
    HAL_I2C_Init(&hi2c1);
}

/**
 * Initialize SPI3, generated by MX.
 */
static void MX_SPI3_Init(void)
{
    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_2LINES;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi3.Init.NSS = SPI_NSS_SOFT;
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi3.Init.TIMode = SPI_TIMODE_DISABLED;
    hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    hspi3.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&hspi3);
}

/**
 * Initialize TIM10, generated by MX.
 * 
 * TIM10 is the low speed AD5933 clock source and generates a clock signal of 167.6kHz on PB10.
 */
static void MX_TIM10_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC;
    
    htim10.Instance = TIM10;
    htim10.Init.Prescaler = 0;
    htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim10.Init.Period = 357;
    htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim10);
    
    HAL_TIM_OC_Init(&htim10);
    
    sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = 0;
    sConfigOC.OCFastMode = 0;
    sConfigOC.OCNIdleState = 0;
    sConfigOC.OCIdleState = 0;
    HAL_TIM_OC_ConfigChannel(&htim10, &sConfigOC, TIM_CHANNEL_1);
}

/**
 * Initialize TIM3.
 * 
 * TIM3 generates a periodic interrupt used by the AD5933 driver.
 */
static void MX_TIM3_Init(void)
{
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 60 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = TIM3_INTERVAL - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim3);
}

/**
 * Configure GPIO pins, generated by MX.
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    /*
     * Enable GPIO port clocks
     */
    __GPIOA_CLK_ENABLE();
    __GPIOB_CLK_ENABLE();
    __GPIOC_CLK_ENABLE();
    __GPIOD_CLK_ENABLE();
    __GPIOE_CLK_ENABLE();
    __GPIOH_CLK_ENABLE();
    
    /*
     * Configure unused pins as analog to reduce power consumption (doesn't really do much)
     */
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    // PA
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // PB
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    // PC
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_13 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    // PD
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    // PE
    GPIO_InitStruct.Pin = GPIO_PIN_7 | ((uint16_t)0xFF00) /* PE8..PE15 */;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
#if !defined(BOARD_HAS_ETHERNET) || BOARD_HAS_ETHERNET == 0
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    // PA
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // PB
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    // PC
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
#endif
    
#if !defined(BOARD_HAS_USBH) || BOARD_HAS_USBH == 0
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif
    
    /*
     * Button (in): PA0
     * USB ID (in): PA10
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /*
     * MEMS SPI (analog): PA5 PA6
     */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /*
     * USB Power Switch (out): PC0 >high
     * Main Power Switch (out): PC15 >low
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
    
#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
    /*
     * Ethernet Clock (out): PC9
     */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
#endif
    
    /*
     * SPI3 slave select pins (out): >high
     *  PD0: Flash
     *  PD1: SRAM
     *  PD2: Output mux
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    GPIOD->BSRRL = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    
    /*
     * USB Overcurrent (in): PD5
     */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    /*
     * USBH Power Switch (out): PD8
     * LEDs (out):
     *  PD12: LED4 green
     *  PD13: LED3 orange
     *  PD14: LED5 red
     *  PD15: LED6 blue
     */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    /*
     * USBH Overcurrent (in): PD9
     */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    /*
     * MEMS Interrupts (in): PE0 PE1
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /*
     * DC Jack detect (in): PE2
     */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /*
     * CS for MEMS (out): PE3
     */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/**
 * Initialize USB device, generated by MX.
 */
static void MX_USB_DEVICE_Init(void)
{
    USBD_Init(&hUsbDevice, &VCP_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDevice, &USBD_VCP);
    USBD_VCP_RegisterInterface(&hUsbDevice, &USBD_VCP_fops);
    USBD_Start(&hUsbDevice);
}

// ----------------------------------------------------------------------------
