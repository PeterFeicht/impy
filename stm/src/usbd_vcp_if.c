/**
 * @file    usbd_vcp_if.c
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   This file implements the application specific interface for the USB VCP.
 * 
 * @todo detail usage
 */

// Includes -------------------------------------------------------------------
#include "usbd_vcp_if.h"
#include "main.h"

// Constants ------------------------------------------------------------------
#define APP_RX_BUFFER_SIZE      2048
#define APP_TX_BUFFER_SIZE      2048

// Private variables ----------------------------------------------------------
USBD_VCP_LineCodingTypeDef linecoding =
{
    115200,     // baud rate
    0x00,       // stop bits - 1
    0x00,       // parity
    0x08        // no. of bits
};

// Data received from the host are stored in this buffer
uint8_t VCPRxBuffer[APP_RX_BUFFER_SIZE];
// Data to be transmitted to the host are stored in this buffer
uint8_t VCPTxBuffer[APP_TX_BUFFER_SIZE];
// End index of fresh data to be transmitted (in index)
uint32_t VCPTxBufIn = 0;
// Start index of fresh data to be transmitted (out index)
uint32_t VCPTxBufOut = 0;

TIM_HandleTypeDef TimHandle;
// USB device handle in main.c
extern USBD_HandleTypeDef hUsbDevice;

// Private function prototypes ------------------------------------------------
static int8_t VCP_Init     (void);
static int8_t VCP_DeInit   (void);
static int8_t VCP_Control  (uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t VCP_Receive  (uint8_t* pbuf, uint32_t Len);
static int8_t VCP_Transmit (uint8_t* pbuf, uint32_t Len);

USBD_VCP_ItfTypeDef USBD_VCP_fops =
{
    VCP_Init,
    VCP_DeInit,
    VCP_Control,
    VCP_Receive,
    VCP_Transmit
};

// Private functions ----------------------------------------------------------

/**
 * Initializes the VCP media low layer.
 * 
 * @return {@code USBD_Status} code
 */
static int8_t VCP_Init(void)
{
    USBD_VCP_SetTxBuffer(&hUsbDevice, VCPTxBuffer, 0);
    USBD_VCP_SetRxBuffer(&hUsbDevice, VCPRxBuffer);
    
    return USBD_OK;
}

/**
 * DeInitializes the VCP media low layer.
 * 
 * @return {@code USBD_Status} code
 */
static int8_t VCP_DeInit(void)
{
    return USBD_OK;
}

/**
 * Manage the CDC class requests.
 * 
 * @param  cmd Command code
 * @param  pbuf Buffer containing command data (request parameters)
 * @param  length Number of bytes to be sent
 * @retval {@code USBD_Status} code
 */
static int8_t VCP_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    switch(cmd)
    {
        case CDC_SEND_ENCAPSULATED_COMMAND:
            break;
            
        case CDC_GET_ENCAPSULATED_RESPONSE:
            break;
            
        case CDC_SET_COMM_FEATURE:
            break;
            
        case CDC_GET_COMM_FEATURE:
            break;
            
        case CDC_CLEAR_COMM_FEATURE:
            break;
            
        case CDC_SET_LINE_CODING:
            linecoding.bitrate = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
            linecoding.format = pbuf[4];
            linecoding.paritytype = pbuf[5];
            linecoding.datatype = pbuf[6];
            break;
            
        case CDC_GET_LINE_CODING:
            pbuf[0] = (uint8_t)(linecoding.bitrate);
            pbuf[1] = (uint8_t)(linecoding.bitrate >> 8);
            pbuf[2] = (uint8_t)(linecoding.bitrate >> 16);
            pbuf[3] = (uint8_t)(linecoding.bitrate >> 24);
            pbuf[4] = linecoding.format;
            pbuf[5] = linecoding.paritytype;
            pbuf[6] = linecoding.datatype;
            break;
            
        case CDC_SET_CONTROL_LINE_STATE:
            break;
            
        case CDC_SEND_BREAK:
            break;
            
        default:
            break;
    }
    
    return USBD_OK;
}

/**
 * Data received on USB OUT endpoint are sent over VCP interface through this function.
 * 
 * @note
 * This function will block any OUT packet reception on USB endpoint until return. If you exit this function before
 * transfer is complete on VCP interface (e.g. using DMA controller) it will result in receiving more data while
 * previous ones are still not sent.
 * 
 * @param  Buf Buffer of data to be received
 * @param  Len Number of bytes received
 * @retval {@code USBD_Status} code
 */
static int8_t VCP_Receive(uint8_t* Buf, uint32_t Len)
{
    USBD_VCP_ReceivePacket(&hUsbDevice);
    return USBD_OK;
}

/**
 * This function is called once a transfer is complete and a new one can be started.
 * 
 * @param Buf Buffer of data that was sent
 * @param Len Number of bytes transmitted
 * @return {@code USBD_Status} code
 */
static int8_t VCP_Transmit(uint8_t* Buf, uint32_t Len)
{
    return USBD_OK;
}
