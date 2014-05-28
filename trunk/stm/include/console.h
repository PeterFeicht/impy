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
typedef enum
{
    CON_BINARY,
    CON_ASCII
} Console_DataEncoding;

typedef enum
{
    CON_POLAR,
    CON_CARTHESIAN
} Console_CoordinateFormat;

typedef enum
{
    CON_SPACE,
    CON_TAB,
    CON_COMMA
} Console_Separator;

typedef enum
{
    CON_FLOAT,
    CON_HEX
} Console_NumberFormat;

typedef struct
{
    Console_DataEncoding encoding;      //!< Specifies the transfer encoding (binary or ASCII)
    Console_CoordinateFormat coord;     //!< Specifies the coordinate format (polar or carthesian)
    uint8_t header;                     //!< Specifies whether a header is sent
    Console_Separator separator;        //!< For ASCII transfer, specifies the value separator character
    Console_NumberFormat numbers;       //!< For ASCII transfer, specifies the number format (float or hex)
} Console_FormatSpecification;

// Constants ------------------------------------------------------------------

/**
 * The maximum number of arguments a command line can have
 */
#define CON_MAX_ARGUMENTS       15

// Macros ---------------------------------------------------------------------

#define NUMEL(X)                (sizeof(X) / sizeof(*X))

// Exported functions ---------------------------------------------------------
void Console_Init(void);
void Console_ProcessLine(char *str);

// ----------------------------------------------------------------------------

#endif /* CONSOLE_H_ */
