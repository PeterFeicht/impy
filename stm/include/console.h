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
 * Functions may not be `NULL` unless explicitly specified.
 * 
 * Care has to be taken when multiple back ends want to communicate concurrently; only the last interface supplied to
 * `Console_ProcessLine` is used for communication.
 */
typedef struct
{
    uint32_t (*SendString)(const char *str);    //!< Send a string unaltered
    uint32_t (*SendLine)(const char *str);      //!< Send a string (possibly `NULL`) followed by a line break
    uint32_t (*SendBuffer)(const uint8_t *buf, uint32_t len);   //!< Send a buffer with the specified length
    uint32_t (*SendChar)(uint8_t c);            //!< Send a single byte
    void     (*Flush)(void);                    //!< Send buffered data if necessary (may be `NULL`)
    void     (*CommandFinish)(void);            //!< Finish currently executing command and accept new input
    void     (*SetEcho)(uint8_t enable);        //!< Set whether received characters should be echoed back
    uint8_t  (*GetEcho)(void);                  //!< Get whether echoing received characters is enabled
} Console_Interface;

// Macros ---------------------------------------------------------------------

#define NUMEL(X)                (sizeof(X) / sizeof(X[0]))

// Constants ------------------------------------------------------------------

/**
 * The maximum number of arguments a command line can have
 */
#define CON_MAX_ARGUMENTS       15

// Exported functions ---------------------------------------------------------
void Console_Init(void);
void Console_ProcessLine(Console_Interface *itf, char *str);

uint32_t Console_GetFormat(void);
void Console_SetFormat(uint32_t spec);

// Callbacks
void Console_CalibrateCallback(void);
void Console_TempCallback(float temp);

// ----------------------------------------------------------------------------

#endif /* CONSOLE_H_ */
