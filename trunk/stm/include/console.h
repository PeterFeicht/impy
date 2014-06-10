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

// Macros ---------------------------------------------------------------------

#define NUMEL(X)                (sizeof(X) / sizeof(*X))
// We only accept upper case letters for flags, so they all fit in one 32 bit integer
#define IS_FORMAT_FLAG(C)           ((C) >= 'A' && (C) <= 'Z')
#define FORMAT_FLAG_FROM_CHAR(C)    ((uint32_t)1 << ((C) - 'A'))
#ifdef __GNUC__
#define CHAR_FROM_FORMAT_FLAG(F)    ('A' + (char)(__builtin_ctzl(F)))
#else
#define CHAR_FROM_FORMAT_FLAG(F)    ('A' + (char)(ffs(F) - 1))
#endif
#define IS_POWER_OF_TWO(X)          ((X) && !((uint32_t)(X) & ((uint32_t)(X) - 1)))

// Constants ------------------------------------------------------------------

/**
 * The maximum number of arguments a command line can have
 */
#define CON_MAX_ARGUMENTS       15
// Known format flags
#define FORMAT_FLAG_ASCII           FORMAT_FLAG_FROM_CHAR('A')
#define FORMAT_FLAG_BINARY          FORMAT_FLAG_FROM_CHAR('B')
#define FORMAT_FLAG_CARTESIAN       FORMAT_FLAG_FROM_CHAR('C')
#define FORMAT_FLAG_POLAR           FORMAT_FLAG_FROM_CHAR('P')
#define FORMAT_FLAG_HEADER          FORMAT_FLAG_FROM_CHAR('H')
#define FORMAT_FLAG_FLOAT           FORMAT_FLAG_FROM_CHAR('F')
#define FORMAT_FLAG_HEX             FORMAT_FLAG_FROM_CHAR('X')
#define FORMAT_FLAG_SPACE           FORMAT_FLAG_FROM_CHAR('S')
#define FORMAT_FLAG_TAB             FORMAT_FLAG_FROM_CHAR('T')
#define FORMAT_FLAG_COMMA           FORMAT_FLAG_FROM_CHAR('D')
// Format flag masks
#define FORMAT_MASK_ENCODING        (FORMAT_FLAG_ASCII | \
                                     FORMAT_FLAG_BINARY)
#define FORMAT_MASK_COORDINATES     (FORMAT_FLAG_CARTESIAN | \
                                     FORMAT_FLAG_POLAR)
#define FORMAT_MASK_NUMBERS         (FORMAT_FLAG_FLOAT | \
                                     FORMAT_FLAG_HEX)
#define FORMAT_MASK_SEPARATOR       (FORMAT_FLAG_SPACE | \
                                     FORMAT_FLAG_TAB | \
                                     FORMAT_FLAG_COMMA)
#define FORMAT_MASK_UNKNOWN         (~(FORMAT_MASK_ENCODING | \
                                       FORMAT_MASK_COORDINATES | \
                                       FORMAT_MASK_NUMBERS | \
                                       FORMAT_MASK_SEPARATOR | \
                                       FORMAT_FLAG_HEADER))

// Exported functions ---------------------------------------------------------
void Console_Init(void);
void Console_ProcessLine(char *str);

// ----------------------------------------------------------------------------

#endif /* CONSOLE_H_ */
