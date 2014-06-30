/**
 * @file    console.h
 * @author  Peter Feichtinger
 * @date    15.05.2014
 * @brief   Header file for the console interface.
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

// Includes -------------------------------------------------------------------
#include <stdint.h>
#include "usbd_vcp_if.h"

// Exported type definitions --------------------------------------------------
/**
 * Defines the interface used by the virtual console to send data back to the user.
 * Different back ends can supply their functions using this structure when calling {@link Console_ProcessLine}.
 * 
 * Care has to be taken when multiple back ends want to communicate concurrently; only the last interface supplied to
 * {@code Console_ProcessLine} is used for communication.
 */
typedef struct
{
    uint32_t (*SendString)(const char *str);
    uint32_t (*SendLine)(const char *str);
    uint32_t (*SendBuffer)(const uint8_t *buf, uint32_t len);
    uint32_t (*SendChar)(uint8_t c);
    void     (*Flush)(void);
    void     (*CommandFinish)(void);
    void     (*SetEcho)(uint8_t enable);
    uint8_t  (*GetEcho)(void);
} Console_Interface;

// Macros ---------------------------------------------------------------------

#define NUMEL(X)                (sizeof(X) / sizeof(*X))

// Constants ------------------------------------------------------------------

/**
 * The maximum number of arguments a command line can have
 */
#define CON_MAX_ARGUMENTS       15

// Exported functions ---------------------------------------------------------
void Console_Init(void);
void Console_ProcessLine(char *str);

__STATIC_INLINE uint32_t max(uint32_t left, uint32_t right)
{
    return (left > right ? left : right);
}

// ----------------------------------------------------------------------------

#endif /* CONSOLE_H_ */
