/**
 * @file    mx_init.h
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   
 * 
 * 
 */

#ifndef MX_INIT_H_
#define MX_INIT_H_

// Includes -------------------------------------------------------------------
#include "stm32f4xx_hal.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_vcp_if.h"

// External variables ---------------------------------------------------------
extern USBD_HandleTypeDef hUsbDevice;

// Exported functions ---------------------------------------------------------
void MX_Init(void);

// ----------------------------------------------------------------------------

#endif /* MX_INIT_H_ */
