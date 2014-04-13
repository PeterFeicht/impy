/**
 * @file    usbd_conf.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   This file implements the USB Device library callbacks and MSP.
 */

// Includes -------------------------------------------------------------------
#include "stm32f4xx_hal.h"
#include "usbd_core.h"

// Variables ------------------------------------------------------------------
PCD_HandleTypeDef hpcd_FS;

// Functions ------------------------------------------------------------------

/*****************************************************************************
 *                              PCD MSP Routines                             *
 *****************************************************************************/

/**
 * @brief  Initializes the PCD MSP.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    if(hpcd->Instance == USB_OTG_FS)
    {
        __GPIOA_CLK_ENABLE();
        
        /*
         * Data Pins (in|out):
         *  PA11: DM
         *  PA12: DP
         */
        GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        /*
         * VBUS Pin (in): PA9
         */
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        __USB_OTG_FS_CLK_ENABLE();
        
        // Set USBFS Interrupt to the lowest priority
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 7, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
    
}

/**
 * @brief  De-Initializes the PCD MSP.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
    if(hpcd->Instance == USB_OTG_FS)
    {
        __USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

/*******************************************************************************
 LL Driver Callbacks (PCD -> USB Device Library)
 *******************************************************************************/

/**
 * @brief  Setup stage callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
 * @brief  Data Out stage callback.
 * @param  hpcd: PCD handle
 * @param  epnum: Endpoint Number
 */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
 * @brief  Data In stage callback.
 * @param  hpcd: PCD handle
 * @param  epnum: Endpoint Number
 */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
 * @brief  SOF callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SOF(hpcd->pData);
}

/**
 * @brief  Reset callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
    
    switch(hpcd->Init.speed)
    {
        case PCD_SPEED_HIGH:
            speed = USBD_SPEED_HIGH;
            break;
            
        case PCD_SPEED_FULL:
            speed = USBD_SPEED_FULL;
            break;
            
        default:
            speed = USBD_SPEED_FULL;
            break;
    }
    USBD_LL_SetSpeed(hpcd->pData, speed);
    
    USBD_LL_Reset(hpcd->pData);
}

/**
 * @brief  Suspend callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Suspend(hpcd->pData);
}

/**
 * @brief  Resume callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Resume(hpcd->pData);
}

/**
 * @brief  ISOC Out Incomplete callback.
 * @param  hpcd: PCD handle 
 * @param  epnum: Endpoint Number
 */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum);
}

/**
 * @brief  ISOC In Incomplete callback.
 * @param  hpcd: PCD handle 
 * @param  epnum: Endpoint Number
 */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoINIncomplete(hpcd->pData, epnum);
}

/**
 * @brief  Connect callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevConnected(hpcd->pData);
}

/**
 * @brief  Disconnect callback.
 * @param  hpcd: PCD handle
 */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevDisconnected(hpcd->pData);
}

/*****************************************************************************
 *              LL Driver Interface (USB Device Library --> PCD)             *
 *****************************************************************************/

/**
 * @brief  Initializes the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    if(pdev->id == 0)
    {
        // Set LL Driver parameters
        hpcd_FS.Instance = USB_OTG_FS;
        hpcd_FS.Init.speed = PCD_SPEED_FULL;
        hpcd_FS.Init.dev_endpoints = 4;
        hpcd_FS.Init.use_dedicated_ep1 = 0;
        hpcd_FS.Init.ep0_mps = 0x40;
        hpcd_FS.Init.dma_enable = DISABLE;
        hpcd_FS.Init.low_power_enable = ENABLE;
        hpcd_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
        hpcd_FS.Init.Sof_enable = DISABLE;
        hpcd_FS.Init.vbus_sensing_enable = ENABLE;
        hpcd_FS.Init.use_external_vbus = ENABLE;
        
        // Link the driver to the stack
        hpcd_FS.pData = pdev;
        pdev->pData = &hpcd_FS;
        
        // Initialize LL Driver
        HAL_PCD_Init(&hpcd_FS);
        HAL_PCD_SetRxFiFo(&hpcd_FS, 0x80);
        HAL_PCD_SetTxFiFo(&hpcd_FS, 0, 0x40);
        HAL_PCD_SetTxFiFo(&hpcd_FS, 1, 0x80);
    }
    return USBD_OK;
}

/**
 * @brief  De-Initializes the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_DeInit(pdev->pData);
    return USBD_OK;
}

/**
 * @brief  Starts the Low Level portion of the Device driver. 
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_Start(pdev->pData);
    return USBD_OK;
}

/**
 * @brief  Stops the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_Stop(pdev->pData);
    return USBD_OK;
}

/**
 * @brief  Opens an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @param  ep_type: Endpoint Type
 * @param  ep_mps: Endpoint Max Packet Size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{
    HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
    return USBD_OK;
}

/**
 * @brief  Closes an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_Close(pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Flushes an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_Flush(pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Returns Stall condition.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval Stall (1: Yes, 0: No)
 */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd_FS = pdev->pData;
    
    if((ep_addr & 0x80) == 0x80)
    {
        return hpcd_FS->IN_ep[ep_addr & 0x7F].is_stall;
    }
    else
    {
        return hpcd_FS->OUT_ep[ep_addr & 0x7F].is_stall;
    }
    
}

/**
 * @brief  Assigns a USB address to the device.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
    HAL_PCD_SetAddress(pdev->pData, dev_addr);
    return USBD_OK;
}

/**
 * @brief  Transmits data over an endpoint.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @param  pbuf: Pointer to data to be sent
 * @param  size: Data size    
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size)
{
    HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;
}

/**
 * @brief  Prepares an endpoint for reception.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @param  pbuf: Pointer to data to be received
 * @param  size: Data size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size)
{
    HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;
}

/**
 * @brief  Returns the last transfered packet size.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval Recived Data Size
 */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

/**
 * @brief  Delay routine for the USB Device Library.
 * @param  Delay: Delay in ms
 */
void USBD_LL_Delay(uint32_t Delay)
{
    HAL_Delay(Delay);
}
