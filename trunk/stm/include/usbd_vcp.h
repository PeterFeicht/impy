/**
 * @file    usbd_vcp.h
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   Header file for the usbd_vcp class.
 * 
 * This file is mostly adapted from usbd_cdc.h from the STM32CubeF4 firmware package by ST.
 */


#ifndef USBD_VCP_H_
#define USBD_VCP_H_

// Includes -------------------------------------------------------------------
#include  "usbd_ioreq.h"

// Constants ------------------------------------------------------------------
#define VCP_IN_EP       0x81    /* EP1 for data IN */
#define VCP_OUT_EP      0x01    /* EP1 for data OUT */
#define VCP_CMD_EP      0x82    /* EP2 for CDC commands */

// VCP Endpoints parameters: you can fine tune these values depending on the needed baudrates and performance.
#define VCP_DATA_HS_MAX_PACKET_SIZE        512  /* Endpoint IN & OUT Packet size */
#define VCP_DATA_FS_MAX_PACKET_SIZE         64  /* Endpoint IN & OUT Packet size */
#define VCP_CMD_PACKET_SIZE                  8  /* Control Endpoint Packet size */ 

#define USB_VCP_CONFIG_DESC_SIZ             67
#define VCP_DATA_HS_IN_PACKET_SIZE          VCP_DATA_HS_MAX_PACKET_SIZE
#define VCP_DATA_HS_OUT_PACKET_SIZE         VCP_DATA_HS_MAX_PACKET_SIZE
#define VCP_DATA_FS_IN_PACKET_SIZE          VCP_DATA_FS_MAX_PACKET_SIZE
#define VCP_DATA_FS_OUT_PACKET_SIZE         VCP_DATA_FS_MAX_PACKET_SIZE

// VCP Definitions
#define VCP_SEND_ENCAPSULATED_COMMAND       0x00
#define VCP_GET_ENCAPSULATED_RESPONSE       0x01
#define VCP_SET_COMM_FEATURE                0x02
#define VCP_GET_COMM_FEATURE                0x03
#define VCP_CLEAR_COMM_FEATURE              0x04
#define VCP_SET_LINE_CODING                 0x20
#define VCP_GET_LINE_CODING                 0x21
#define VCP_SET_CONTROL_LINE_STATE          0x22
#define VCP_SEND_BREAK                      0x23

// Exported type definitions --------------------------------------------------
typedef struct
{
    uint32_t bitrate;
    uint8_t  format;
    uint8_t  paritytype;
    uint8_t  datatype;
} USBD_VCP_LineCodingTypeDef;

typedef struct
{
    int8_t (*Init)      (void);
    int8_t (*DeInit)    (void);
    int8_t (*Control)   (uint8_t, uint8_t*, uint16_t);
    int8_t (*Receive)   (uint8_t*, uint32_t*);
} USBD_VCP_ItfTypeDef;


typedef struct
{
    uint32_t data[VCP_DATA_HS_MAX_PACKET_SIZE/4];
    uint8_t  CmdOpCode;
    uint8_t  CmdLength;    
    uint8_t  *RxBuffer;  
    uint8_t  *TxBuffer;   
    uint32_t RxLength;
    uint32_t TxLength;    
    
    __IO uint32_t TxState;     
    __IO uint32_t RxState;    
} USBD_VCP_HandleTypeDef; 

// Exported variables ---------------------------------------------------------
extern USBD_ClassTypeDef  USBD_VCP;

// Exported functions ---------------------------------------------------------
uint8_t USBD_VCP_RegisterInterface(USBD_HandleTypeDef *pdev, USBD_VCP_ItfTypeDef *fops);
uint8_t USBD_VCP_SetTxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff, uint16_t length);
uint8_t USBD_VCP_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff);
uint8_t USBD_VCP_ReceivePacket(USBD_HandleTypeDef *pdev);
uint8_t USBD_VCP_TransmitPacket(USBD_HandleTypeDef *pdev);

// ----------------------------------------------------------------------------

#endif /* USBD_VCP_H_ */
