/**
 * @file    usbd_vcp_if.h
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   Header file for the VCP interface definition.
 */

#ifndef USBD_VCP_IF_H_
#define USBD_VCP_IF_H_

// Includes -------------------------------------------------------------------
#include "usbd_vcp.h"

// Constants ------------------------------------------------------------------

/**
 * The maximum number of characters in one command line string, terminating 0 character not included.
 */
#define MAX_CMDLINE_LENGTH      200

// Exported variables ---------------------------------------------------------
extern USBD_VCP_ItfTypeDef USBD_VCP_fops;

// Exported functions ---------------------------------------------------------
void VCP_SetEcho(uint8_t enable);
uint8_t VCP_GetEcho(void);
void VCP_CommandFinish(void);
uint32_t VCP_SendChar(uint8_t c);
uint32_t VCP_SendString(const char *str);
uint32_t VCP_SendBuffer(const uint8_t *buf, uint32_t len);

// ----------------------------------------------------------------------------

#endif /* USBD_VCP_IF_H_ */
