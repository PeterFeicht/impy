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
#define APP_RX_BUFFER_SIZE      VCP_DATA_HS_MAX_PACKET_SIZE
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
// End index of fresh data to be transmitted
uint32_t VCPTxBufEnd = 0;
// Start index of fresh data to be transmitted
uint32_t VCPTxBufStart = 0;
// Whether to echo characters received from the host, enabled by default
uint8_t echo_enabled = 1;
// The current command line text (0 terminated)
uint8_t VCP_cmdline[MAX_CMDLINE_LENGTH + 1];
// Whether the current command is still busy and input should be ignored
uint8_t cmd_busy = 0;

// USB device handle in main.c
extern USBD_HandleTypeDef hUsbDevice;

// Private function prototypes ------------------------------------------------
static int8_t VCP_Init     (void);
static int8_t VCP_DeInit   (void);
static int8_t VCP_Control  (uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t VCP_Receive  (uint8_t* pbuf, uint32_t Len);
static int8_t VCP_Transmit (uint8_t* pbuf, uint32_t Len);

static int8_t SendBuffer(void);

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
static int8_t VCP_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length __attribute__((unused)))
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
    uint8_t *rxbuf = Buf;
    uint8_t *const rxend = Buf + Len;
    uint8_t *txbuf = VCPTxBuffer + VCPTxBufEnd;
    
    if(echo_enabled)
    {
        // Copy received data into transmit buffer (echo)
        while(rxbuf < rxend)
        {
            // Avoid buffer overflow
            if(txbuf == (VCPTxBuffer + APP_TX_BUFFER_SIZE))
            {
                txbuf = VCPTxBuffer;
            }
            *txbuf = *rxbuf++;
            txbuf++;
        }
        
        // Set buffer pointer
        VCPTxBufEnd += Len;
        if(VCPTxBufEnd >= APP_TX_BUFFER_SIZE)
        {
            VCPTxBufEnd -= APP_TX_BUFFER_SIZE;
        }
        
        SendBuffer();
    }
    
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
    // Send new data if present
    SendBuffer();
    return USBD_OK;
}

/**
 * Send the current buffer contents via USB. If the buffer is empty, does nothing.
 * 
 * @return {@code USBD_Status} code
 */
static int8_t SendBuffer(void)
{
    uint32_t buffsize;
    int8_t status;
    
    if(VCPTxBufStart == VCPTxBufEnd)
        return USBD_OK;
    
    if(VCPTxBufStart > VCPTxBufEnd) /* rollback */
    {
        buffsize = APP_TX_BUFFER_SIZE - VCPTxBufStart;
    }
    else
    {
        buffsize = VCPTxBufEnd - VCPTxBufStart;
    }
    
    USBD_VCP_SetTxBuffer(&hUsbDevice, VCPTxBuffer + VCPTxBufStart, buffsize);
    status = USBD_VCP_TransmitPacket(&hUsbDevice);
    
    if(status == USBD_OK)
    {
        VCPTxBufStart += buffsize;
        if(VCPTxBufStart == APP_TX_BUFFER_SIZE)
        {
            VCPTxBufStart = 0;
        }
    }
    
    return status;
}

// Exported functions ---------------------------------------------------------

/**
 * Sets whether characters received from the host should be echoed back.
 * 
 * @param enable {@code 0} to disable echo, nonzero value otherwise
 */
void VCP_SetEcho(uint8_t enable)
{
    echo_enabled = enable;
}

/**
 * Gets a value indicating whether input received over the VCP is echoed back.
 * 
 * @return {@code 0} if echo is disabled, nonzero value otherwise
 */
uint8_t VCP_GetEcho(void)
{
    return echo_enabled;
}

/**
 * This function should be called by the command line processor when it is finished with processing the current command
 * and new console input should be possible.
 */
void VCP_CommandFinish(void)
{
    cmd_busy = 0;
}

// ----------------------------------------------------------------------------
