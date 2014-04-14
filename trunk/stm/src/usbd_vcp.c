/**
 * @file    usbd_vcp.c
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   This file provides the high layer firmware functions to manage the USB VCP Class.
 * 
 * This file provides functions to manage the following functionalities: 
 *  - Initialization and configuration of high and low layer
 *  - Enumeration as CDC Device (and enumeration for each implemented memory interface)
 *  - OUT/IN data transfer
 *  - Command IN transfer (class requests management)
 *  - Error management
 * 
 * This driver manages the
 *  "Universal Serial Bus Class Definitions for Communications Devices Revision 1.2 November 16, 2007"
 * and the sub-protocol specification of
 *  "Universal Serial Bus Communications Class Subclass Specification for PSTN Devices Revision 1.2 February 9, 2007".
 * 
 * This driver implements the following aspects of the specification:
 *  - Device descriptor management
 *  - Configuration descriptor management
 *  - Enumeration as CDC device with 2 data endpoints (IN and OUT) and 1 command endpoint (IN)
 *  - Requests management (as described in section 6.2 in specification)
 *  - Abstract Control Model compliant
 *  - Union Functional collection (using 1 IN endpoint for control)
 *  - Data interface class
 *  
 *  @note For the Abstract Control Model, this core allows only transmitting the requests to lower layer
 *  dispatcher (e.g. USBD_CDC_vcp.c/.h) which should manage each request and perform relative actions.
 *  These aspects may be enriched or modified for a specific user application.
 *  This driver doesn't implement the following aspects of the specification
 *  (but it is possible to manage these features with some modifications on this driver):
 *   - Any class-specific aspect relative to communication classes should be managed by user application.
 *   - All communication classes other than PSTN are not managed
 * 
 * @attention This file is mostly adapted from usbd_cdc.h from the STM32CubeF4 firmware package by ST.
 */

// Includes -------------------------------------------------------------------
#include "usbd_vcp.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"

// Private function prototypes ------------------------------------------------
static uint8_t USBD_VCP_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_VCP_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_VCP_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_VCP_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_VCP_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_VCP_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_VCP_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_VCP_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_VCP_GetDeviceQualifierDescriptor(uint16_t *length);

// Private variables ----------------------------------------------------------
// USB Standard Device Descriptor
__ALIGN_BEGIN static uint8_t USBD_VCP_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
};

// VCP interface class callback structure
USBD_ClassTypeDef  USBD_VCP = 
{
    USBD_VCP_Init,
    USBD_VCP_DeInit,
    USBD_VCP_Setup,
    NULL,   /* EP0_TxSent */
    USBD_VCP_EP0_RxReady,
    USBD_VCP_DataIn,
    USBD_VCP_DataOut,
    NULL,   /* SOF */ // TODO maybe add SOF callback for faster bulk transfer to host
    NULL,   /* IsoINIncomplete */
    NULL,   /* IsoOUTIncomplete */
    USBD_VCP_GetHSCfgDesc,
    USBD_VCP_GetFSCfgDesc,
    NULL,   /* GetOtherSpeedConfigDescriptor */
    USBD_VCP_GetDeviceQualifierDescriptor,
};

// USB VCP HS device Configuration Descriptor
__ALIGN_BEGIN uint8_t USBD_VCP_CfgHSDesc[USB_VCP_CONFIG_DESC_SIZ] __ALIGN_END =
{
    /* Configuration Descriptor*/
    0x09,   /* bLength: Configuration Descriptor size */
    USB_DESC_TYPE_CONFIGURATION,    /* bDescriptorType: Configuration */
    USB_VCP_CONFIG_DESC_SIZ,        /* wTotalLength:no of returned bytes */
    0x00,
    0x02,   /* bNumInterfaces: 2 interface */
    0x01,   /* bConfigurationValue: Configuration value */
    0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
    0xC0,   /* bmAttributes: self powered */
    0x32,   /* MaxPower 0 mA */
    /*---------------------------------------------------------------------------*/
    
    /* Interface Descriptor */
    0x09,   /* bLength: Interface Descriptor size */
    USB_DESC_TYPE_INTERFACE,    /* bDescriptorType: Interface */
    /* Interface descriptor type */
    0x00,   /* bInterfaceNumber: Number of Interface */
    0x00,   /* bAlternateSetting: Alternate setting */
    0x01,   /* bNumEndpoints: One endpoints used */
    0x02,   /* bInterfaceClass: Communication Interface Class */
    0x02,   /* bInterfaceSubClass: Abstract Control Model */
    0x01,   /* bInterfaceProtocol: Common AT commands */
    0x00,   /* iInterface: */
    
    /* Header Functional Descriptor */
    0x05,   /* bLength: Endpoint Descriptor size */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x00,   /* bDescriptorSubtype: Header Func Desc */
    0x10,   /* bcdCDC: spec release number */
    0x01,
    
    /* Call Management Functional Descriptor */
    0x05,   /* bFunctionLength */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x01,   /* bDescriptorSubtype: Call Management Func Desc */
    0x00,   /* bmCapabilities: D0+D1 */
    0x01,   /* bDataInterface: 1 */
    
    /* ACM Functional Descriptor */
    0x04,   /* bFunctionLength */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
    0x02,   /* bmCapabilities */
    
    /* Union Functional Descriptor */
    0x05,   /* bFunctionLength */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x06,   /* bDescriptorSubtype: Union func desc */
    0x00,   /* bMasterInterface: Communication class interface */
    0x01,   /* bSlaveInterface0: Data Class Interface */
    
    /* Endpoint 2 Descriptor */
    0x07,                           /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,         /* bDescriptorType: Endpoint */
    VCP_CMD_EP,                     /* bEndpointAddress */
    0x03,                           /* bmAttributes: Interrupt */
    LOBYTE(VCP_CMD_PACKET_SIZE),    /* wMaxPacketSize: */
    HIBYTE(VCP_CMD_PACKET_SIZE),
    0x10,                           /* bInterval: */ 
    /*---------------------------------------------------------------------------*/
    
    /* Data class interface descriptor */
    0x09,   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_INTERFACE,    /* bDescriptorType: */
    0x01,   /* bInterfaceNumber: Number of Interface */
    0x00,   /* bAlternateSetting: Alternate setting */
    0x02,   /* bNumEndpoints: Two endpoints used */
    0x0A,   /* bInterfaceClass: CDC */
    0x00,   /* bInterfaceSubClass: */
    0x00,   /* bInterfaceProtocol: */
    0x00,   /* iInterface: */
    
    /* Endpoint OUT Descriptor */
    0x07,                                   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType: Endpoint */
    VCP_OUT_EP,                             /* bEndpointAddress */
    0x02,                                   /* bmAttributes: Bulk */
    LOBYTE(VCP_DATA_HS_MAX_PACKET_SIZE),    /* wMaxPacketSize: */
    HIBYTE(VCP_DATA_HS_MAX_PACKET_SIZE),
    0x00,                                   /* bInterval: ignore for Bulk transfer */
    
    /* Endpoint IN Descriptor */
    0x07,                                   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType: Endpoint */
    VCP_IN_EP,                              /* bEndpointAddress */
    0x02,                                   /* bmAttributes: Bulk */
    LOBYTE(VCP_DATA_HS_MAX_PACKET_SIZE),    /* wMaxPacketSize: */
    HIBYTE(VCP_DATA_HS_MAX_PACKET_SIZE),
    0x00                                    /* bInterval: ignore for Bulk transfer */
};

// USB VCP FS device Configuration Descriptor
__ALIGN_BEGIN uint8_t USBD_VCP_CfgFSDesc[USB_VCP_CONFIG_DESC_SIZ] __ALIGN_END =
{
    /* Configuration Descriptor*/
    0x09,   /* bLength: Configuration Descriptor size */
    USB_DESC_TYPE_CONFIGURATION,    /* bDescriptorType: Configuration */
    USB_VCP_CONFIG_DESC_SIZ,        /* wTotalLength:no of returned bytes */
    0x00,
    0x02,   /* bNumInterfaces: 2 interface */
    0x01,   /* bConfigurationValue: Configuration value */
    0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
    0xC0,   /* bmAttributes: self powered */
    0x32,   /* MaxPower 0 mA */
    /*---------------------------------------------------------------------------*/
    
    /* Interface Descriptor */
    0x09,   /* bLength: Interface Descriptor size */
    USB_DESC_TYPE_INTERFACE,    /* bDescriptorType: Interface */
    /* Interface descriptor type */
    0x00,   /* bInterfaceNumber: Number of Interface */
    0x00,   /* bAlternateSetting: Alternate setting */
    0x01,   /* bNumEndpoints: One endpoints used */
    0x02,   /* bInterfaceClass: Communication Interface Class */
    0x02,   /* bInterfaceSubClass: Abstract Control Model */
    0x01,   /* bInterfaceProtocol: Common AT commands */
    0x00,   /* iInterface: */
    
    /* Header Functional Descriptor */
    0x05,   /* bLength: Endpoint Descriptor size */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x00,   /* bDescriptorSubtype: Header Func Desc */
    0x10,   /* bcdCDC: spec release number */
    0x01,
    
    /* Call Management Functional Descriptor */
    0x05,   /* bFunctionLength */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x01,   /* bDescriptorSubtype: Call Management Func Desc */
    0x00,   /* bmCapabilities: D0+D1 */
    0x01,   /* bDataInterface: 1 */
    
    /* ACM Functional Descriptor */
    0x04,   /* bFunctionLength */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
    0x02,   /* bmCapabilities */
    
    /* Union Functional Descriptor */
    0x05,   /* bFunctionLength */
    0x24,   /* bDescriptorType: CS_INTERFACE */
    0x06,   /* bDescriptorSubtype: Union func desc */
    0x00,   /* bMasterInterface: Communication class interface */
    0x01,   /* bSlaveInterface0: Data Class Interface */
    
    /* Endpoint 2 Descriptor */
    0x07,                           /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,         /* bDescriptorType: Endpoint */
    VCP_CMD_EP,                     /* bEndpointAddress */
    0x03,                           /* bmAttributes: Interrupt */
    LOBYTE(VCP_CMD_PACKET_SIZE),    /* wMaxPacketSize: */
    HIBYTE(VCP_CMD_PACKET_SIZE),
    0x10,                           /* bInterval: */ 
    /*---------------------------------------------------------------------------*/
    
    /* Data class interface descriptor */
    0x09,   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_INTERFACE,    /* bDescriptorType: */
    0x01,   /* bInterfaceNumber: Number of Interface */
    0x00,   /* bAlternateSetting: Alternate setting */
    0x02,   /* bNumEndpoints: Two endpoints used */
    0x0A,   /* bInterfaceClass: CDC */
    0x00,   /* bInterfaceSubClass: */
    0x00,   /* bInterfaceProtocol: */
    0x00,   /* iInterface: */
    
    /* Endpoint OUT Descriptor */
    0x07,                                   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType: Endpoint */
    VCP_OUT_EP,                             /* bEndpointAddress */
    0x02,                                   /* bmAttributes: Bulk */
    LOBYTE(VCP_DATA_FS_MAX_PACKET_SIZE),    /* wMaxPacketSize: */
    HIBYTE(VCP_DATA_FS_MAX_PACKET_SIZE),
    0x00,                                   /* bInterval: ignore for Bulk transfer */
    
    /* Endpoint IN Descriptor */
    0x07,                                   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType: Endpoint */
    VCP_IN_EP,                              /* bEndpointAddress */
    0x02,                                   /* bmAttributes: Bulk */
    LOBYTE(VCP_DATA_FS_MAX_PACKET_SIZE),    /* wMaxPacketSize: */
    HIBYTE(VCP_DATA_FS_MAX_PACKET_SIZE),
    0x00                                    /* bInterval: ignore for Bulk transfer */
};

// Private functions ----------------------------------------------------------

/**
 * @brief  Initializes the VCP interface
 * @param  pdev device instance
 * @param  cfgidx Configuration index
 * @retval {@code USBD_Status} code
 */
static uint8_t USBD_VCP_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx __attribute__((unused)))
{
    USBD_VCP_HandleTypeDef *hcdc;
    
    // Open IN and OUT endpoints
    if(pdev->dev_speed == USBD_SPEED_HIGH)
    {
        USBD_LL_OpenEP(pdev, VCP_IN_EP, USBD_EP_TYPE_BULK, VCP_DATA_HS_IN_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, VCP_OUT_EP, USBD_EP_TYPE_BULK, VCP_DATA_HS_OUT_PACKET_SIZE);
    }
    else
    {
        USBD_LL_OpenEP(pdev, VCP_IN_EP, USBD_EP_TYPE_BULK, VCP_DATA_FS_IN_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, VCP_OUT_EP, USBD_EP_TYPE_BULK, VCP_DATA_FS_OUT_PACKET_SIZE);
    }
    
    // Open Command IN EP
    USBD_LL_OpenEP(pdev, VCP_CMD_EP, USBD_EP_TYPE_INTR, VCP_CMD_PACKET_SIZE);
    
    pdev->pClassData = USBD_malloc(sizeof(USBD_VCP_HandleTypeDef));
    
    if(pdev->pClassData == NULL)
    {
        return USBD_FAIL;
    }
    
    hcdc = pdev->pClassData;
    
    // Init physical interface components
    ((USBD_VCP_ItfTypeDef *)pdev->pUserData)->Init();
    
    // Init Xfer states
    hcdc->TxState = 0;
    hcdc->RxState = 0;

    // Prepare OUT endpoint to receive next packet
    if(pdev->dev_speed == USBD_SPEED_HIGH)
    {
        USBD_LL_PrepareReceive(pdev, VCP_OUT_EP, hcdc->RxBuffer, VCP_DATA_HS_OUT_PACKET_SIZE);
    }
    else
    {
        USBD_LL_PrepareReceive(pdev, VCP_OUT_EP, hcdc->RxBuffer, VCP_DATA_FS_OUT_PACKET_SIZE);
    }
    
    return USBD_OK;
}

/**
 * @brief  DeInitialize the VCP layer
 * @param  pdev device instance
 * @param  cfgidx Configuration index
 * @retval {@code USBD_Status} code
 */
static uint8_t USBD_VCP_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx __attribute__((unused)))
{
    // Close endpoints
    USBD_LL_CloseEP(pdev, VCP_IN_EP);
    USBD_LL_CloseEP(pdev, VCP_OUT_EP);
    USBD_LL_CloseEP(pdev, VCP_CMD_EP);
    
    // DeInit physical interface components
    if(pdev->pClassData != NULL)
    {
        ((USBD_VCP_ItfTypeDef *)pdev->pUserData)->DeInit();
        USBD_free(pdev->pClassData);
        pdev->pClassData = NULL;
    }
    
    return USBD_OK;
}

/**
 * @brief  Handle the VCP specific requests
 * @param  pdev instance
 * @param  req usb requests
 * @retval {@code USBD_Status} code
 */
static uint8_t USBD_VCP_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    switch(req->bmRequest & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS:
            if(req->wLength)
            {
                if(req->bmRequest & 0x80)
                {
                    ((USBD_VCP_ItfTypeDef *)pdev->pUserData)->
                            Control(req->bRequest, (uint8_t *)hcdc->data, req->wLength);
                    USBD_CtlSendData(pdev, (uint8_t *)hcdc->data, req->wLength);
                }
                else
                {
                    hcdc->CmdOpCode = req->bRequest;
                    hcdc->CmdLength = req->wLength;
                    
                    USBD_CtlPrepareRx(pdev, (uint8_t *)hcdc->data, req->wLength);
                }
            }
            break;
    }
    
    return USBD_OK;
}

/**
 * @brief  Data sent on non-control IN endpoint
 * @param  pdev device instance
 * @param  epnum endpoint number
 * @retval {@code USBD_Status} code
 */
static uint8_t USBD_VCP_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum __attribute__((unused)))
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    if(hcdc != NULL)
    {
        hcdc->TxState = 0;
        
        // Call interface callback
        ((USBD_VCP_ItfTypeDef *)pdev->pUserData)->Transmit(hcdc->TxBuffer, hcdc->TxLength);
        
        return USBD_OK;
    }
    else
    {
        return USBD_FAIL;
    }
}

/**
 * @brief  Data received on non-control Out endpoint
 * @param  pdev device instance
 * @param  epnum endpoint number
 * @retval {@code USBD_Status} code
 */
static uint8_t USBD_VCP_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    // USB data will be immediately processed, this allow next USB traffic being NAKed till
    // the end of the application Xfer
    if(hcdc != NULL)
    {
        hcdc->RxLength = USBD_LL_GetRxDataSize(pdev, epnum);
        
        // Call interface callback
        ((USBD_VCP_ItfTypeDef *)pdev->pUserData)->Receive(hcdc->RxBuffer, hcdc->RxLength);
        
        return USBD_OK;
    }
    else
    {
        return USBD_FAIL;
    }
}

/**
 * @brief  Data received on non-control Out endpoint
 * @param  pdev device instance
 * @param  epnum endpoint number
 * @retval {@code USBD_Status} code
 */
static uint8_t USBD_VCP_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    if((pdev->pUserData != NULL) && (hcdc->CmdOpCode != 0xFF))
    {
        ((USBD_VCP_ItfTypeDef *)pdev->pUserData)->Control(hcdc->CmdOpCode, (uint8_t *)hcdc->data, hcdc->CmdLength);
        hcdc->CmdOpCode = 0xFF;
    }
    return USBD_OK;
}

/**
 * @brief  Return configuration descriptor
 * @param  speed current device speed
 * @param  length pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_VCP_GetFSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_VCP_CfgFSDesc);
    return USBD_VCP_CfgFSDesc;
}

/**
 * @brief  Return configuration descriptor
 * @param  speed current device speed
 * @param  length pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_VCP_GetHSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_VCP_CfgHSDesc);
    return USBD_VCP_CfgHSDesc;
}

/**
 * @brief  return Device Qualifier descriptor
 * @param  length pointer data length
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_VCP_GetDeviceQualifierDescriptor(uint16_t *length)
{
    *length = sizeof(USBD_VCP_DeviceQualifierDesc);
    return USBD_VCP_DeviceQualifierDesc;
}

/**
 * @brief  USBD_VCP_RegisterInterface
 * @param  pdev device instance
 * @param  fops CD  Interface callback
 * @retval {@code USBD_Status} code
 */
uint8_t USBD_VCP_RegisterInterface(USBD_HandleTypeDef *pdev, USBD_VCP_ItfTypeDef *fops)
{
    if(fops != NULL)
    {
        pdev->pUserData = fops;
        return USBD_OK;
    }
    
    return USBD_FAIL;
}

/**
 * @brief  USBD_VCP_SetTxBuffer
 * @param  pdev device instance
 * @param  pbuff Tx Buffer
 * @retval {@code USBD_Status} code
 */
uint8_t USBD_VCP_SetTxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff, uint16_t length)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    hcdc->TxBuffer = pbuff;
    hcdc->TxLength = length;
    
    return USBD_OK;
}

/**
 * @brief  USBD_VCP_SetRxBuffer
 * @param  pdev device instance
 * @param  pbuff Rx Buffer
 * @retval {@code USBD_Status} code
 */
uint8_t USBD_VCP_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    hcdc->RxBuffer = pbuff;
    
    return USBD_OK;
}

/**
 * @brief  Data received on non-control Out endpoint
 * @param  pdev device instance
 * @param  epnum endpoint number
 * @retval {@code USBD_Status} code
 */
uint8_t USBD_VCP_TransmitPacket(USBD_HandleTypeDef *pdev)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    if(hcdc == NULL)
    {
        return USBD_FAIL;
    }
    
    if(hcdc->TxState == 0)
    {
        // Transmit next packet
        USBD_LL_Transmit(pdev, VCP_IN_EP, hcdc->TxBuffer, hcdc->TxLength);
        
        // Tx Transfer in progress
        hcdc->TxState = 1;
        return USBD_OK;
    }
    else
    {
        return USBD_BUSY;
    }
}

/**
 * @brief  prepare OUT Endpoint for reception
 * @param  pdev device instance
 * @retval {@code USBD_Status} code
 */
uint8_t USBD_VCP_ReceivePacket(USBD_HandleTypeDef *pdev)
{
    USBD_VCP_HandleTypeDef *hcdc = pdev->pClassData;
    
    if(hcdc == NULL)
    {
        return USBD_FAIL;
    }

    // Prepare OUT endpoint to receive next packet
    if(pdev->dev_speed == USBD_SPEED_HIGH)
    {
        USBD_LL_PrepareReceive(pdev, VCP_OUT_EP, hcdc->RxBuffer, VCP_DATA_HS_OUT_PACKET_SIZE);
    }
    else
    {
        USBD_LL_PrepareReceive(pdev, VCP_OUT_EP, hcdc->RxBuffer, VCP_DATA_FS_OUT_PACKET_SIZE);
    }
    
    return USBD_OK;
}

// ----------------------------------------------------------------------------
