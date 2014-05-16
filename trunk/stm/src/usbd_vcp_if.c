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
static USBD_VCP_LineCodingTypeDef linecoding =
{
    115200,     // baud rate
    0x00,       // stop bits - 1
    0x00,       // parity
    0x08        // no. of bits
};

// Data received from the host are stored in this buffer
static uint8_t VCPRxBuffer[APP_RX_BUFFER_SIZE];
// Data to be transmitted to the host are stored in this buffer
static uint8_t VCPTxBuffer[APP_TX_BUFFER_SIZE];
// End index of fresh data to be transmitted
static uint32_t VCPTxBufEnd = 0;
// Start index of fresh data to be transmitted
static uint32_t VCPTxBufStart = 0;
// Whether an external buffer should be transmitted
static uint8_t VCPTxExternal = 0;
// Whether to echo characters received from the host, enabled by default
static uint8_t echo_enabled = 1;
// The current command line text (0 terminated)
static uint8_t VCP_cmdline[MAX_CMDLINE_LENGTH + 1];
// Whether the current command is still busy and input should be ignored
static uint8_t cmd_busy = 0;

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
    *VCP_cmdline = 0;
    
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
    // Whether the character received is the first in a new line
    static uint8_t cmd_newline = 1;
    // Current length of received command
    static uint8_t cmd_len = 0;
    // Whether to disable echo for the current line (when preceded with '@')
    static uint8_t echo_suppress = 0;
    
    uint8_t *const rxend = Buf + Len;
    uint8_t *txbuf = VCPTxBuffer + VCPTxBufEnd;
    uint32_t txlen = 0;
    
    // If previous command is busy, do nothing
    for(uint8_t *rxbuf = Buf; rxbuf < rxend && !cmd_busy; rxbuf++)
    {
        if(cmd_newline && *rxbuf == '@')
        {
            echo_suppress = 1;
            continue;
        }
        
        if(echo_enabled && !echo_suppress)
        {
            if(txbuf == (VCPTxBuffer + APP_TX_BUFFER_SIZE))
            {
                txbuf = VCPTxBuffer;
            }
            *txbuf++ = *rxbuf;
            txlen++;
        }
        
        if(*rxbuf == '\r' || *rxbuf == '\n' || cmd_len == MAX_CMDLINE_LENGTH)
        {
            // Don't call console with empty command
            if(cmd_newline)
            {
                continue;
            }
            
            VCP_cmdline[cmd_len] = 0;
            cmd_newline = 1;
            echo_suppress = 0;
            cmd_len = 0;
            cmd_busy = 1;
            
            // TODO call console
            
            continue;
        }
        else
        {
            cmd_newline = 0;
            VCP_cmdline[cmd_len++] = *rxbuf;
        }
    }
    
    if(txlen > 0)
    {
        VCPTxBufEnd += txlen;
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
    if(VCPTxExternal)
    {
        // Tried to transmit external buffer earlier while busy, transmit now
        VCPTxExternal = 0;
        USBD_VCP_TransmitPacket(&hUsbDevice);
    }
    else
    {
        SendBuffer();
    }
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

/**
 * Send the specified character over the virtual COM port.
 * 
 * @param c The value to send
 * @return {@code 1} if the character was sent, {@code 0} if the buffer is full
 */
uint32_t VCP_SendChar(uint8_t c)
{
    if(VCPTxBufStart == VCPTxBufEnd)
    {
        VCPTxBufStart = 0;
        VCPTxBufEnd = 1;
        *VCPTxBuffer = c;
    }
    else if((VCPTxBufEnd != VCPTxBufStart - 1) && (VCPTxBufEnd != APP_TX_BUFFER_SIZE - 1 || VCPTxBufStart != 0))
    {
        VCPTxBuffer[VCPTxBufEnd++] = c;
        
        if(VCPTxBufEnd == APP_TX_BUFFER_SIZE)
        {
            VCPTxBufEnd = 0;
        }
    }
    else
    {
        return 0;
    }
    
    SendBuffer();
    return 1;
}

/**
 * Send the specified 0 terminated string over the virtual COM port.
 *
 * This function should be used for small strings that fit into the buffer and when multiple strings are to be
 * transmitted consecutively. For transmitting data that does not fit into a single buffer {@link VCP_SendBuffer}
 * should be used instead.
 * 
 * @param str Pointer to a zero terminated string
 * @return The number of bytes transmitted. This can be less than the string length, if the string is longer than the
 *         free space in the transmit buffer.
 */
uint32_t VCP_SendString(uint8_t *str)
{
    uint32_t buffered = 0;
    uint32_t len;
    uint32_t sent;
    
    if(str == NULL)
        return 0;
    
    len = strlen((char *)str);
    
    if(VCPTxBufStart == VCPTxBufEnd)
    {
        sent = (len < APP_TX_BUFFER_SIZE ? len : APP_TX_BUFFER_SIZE - 1);
        VCPTxBufStart = 0;
        VCPTxBufEnd = sent;
        memcpy(VCPTxBuffer, str, sent);
    }
    else if(VCPTxBufEnd > VCPTxBufStart)
    {
        buffered = VCPTxBufEnd - VCPTxBufStart;
        sent = (len < APP_TX_BUFFER_SIZE - buffered ? len : APP_TX_BUFFER_SIZE - buffered - 1);
        
        if(VCPTxBufEnd + sent >= APP_TX_BUFFER_SIZE)
        {
            uint32_t tmp = APP_TX_BUFFER_SIZE - VCPTxBufEnd;
            memcpy(VCPTxBuffer + VCPTxBufEnd, str, tmp);
            memcpy(VCPTxBuffer, str + tmp, sent - tmp);
            VCPTxBufEnd = sent - tmp;
        }
        else
        {
            memcpy(VCPTxBuffer + VCPTxBufEnd, str, sent);
        }
    }
    else // VCPTxBufEnd < VCPTxBufStart
    {
        buffered = VCPTxBufEnd + APP_TX_BUFFER_SIZE - VCPTxBufStart;
        sent = (len < APP_TX_BUFFER_SIZE - buffered ? len : APP_TX_BUFFER_SIZE - buffered - 1);
        memcpy(VCPTxBuffer + VCPTxBufEnd, str, sent);
        VCPTxBufEnd += sent;
    }
    
    SendBuffer();
    return sent;
}

/**
 * Send the specified buffer over the virtual COM port.
 * 
 * This function should be used for data that does not fit into the internal buffer.
 * 
 * @param buf Pointer to the buffer to be sent
 * @param len Number of bytes to be sent
 * @return {@code 1} on success, {@code 0} otherwise
 */
uint32_t VCP_SendBuffer(uint8_t *buf, uint32_t len)
{
    if(buf == NULL)
    {
        return 0;
    }
    
    if(len == 0)
    {
        return 1;
    }
    
    USBD_VCP_SetTxBuffer(&hUsbDevice, buf, (uint16_t)len);
    if(USBD_VCP_TransmitPacket(&hUsbDevice) == USBD_BUSY)
    {
        VCPTxExternal = 1;
    }
    
    return 1;
}

// ----------------------------------------------------------------------------
