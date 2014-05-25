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

// Constants ------------------------------------------------------------------

/**
 * The maximum number of arguments a command line can have
 */
#define CON_MAX_ARGUMENTS       15

// Macros ---------------------------------------------------------------------

#define STRINGIFY(src)          #src

// Exported functions ---------------------------------------------------------
void Console_Init(void);
void Console_ProcessLine(char *str);

// ----------------------------------------------------------------------------

#endif /* CONSOLE_H_ */
