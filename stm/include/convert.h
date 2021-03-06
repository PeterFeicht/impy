/**
 * @file    convert.h
 * @author  Peter Feichtinger
 * @date    13.06.2014
 * @brief   Header file for the data conversion routines.
 */

#ifndef CONVERT_H_
#define CONVERT_H_

// Includes -------------------------------------------------------------------
#include <stdint.h>
#include "ad5933.h"

// Exported type definitions --------------------------------------------------
/**
 * Represents a data buffer of a certain length. This structure in returned by the data conversion functions.
 */
typedef struct
{
    void *data;         //!< Pointer to the buffer data
    uint32_t size;      //!< Length of the buffer in bytes
} Buffer;

// Macros ---------------------------------------------------------------------

#define NUMEL(X)                (sizeof(X) / sizeof(X[0]))
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

// Default flag values when missing
#define FORMAT_DEFAULT_COORDINATES  FORMAT_FLAG_POLAR
#define FORMAT_DEFAULT_NUMBERS      FORMAT_FLAG_FLOAT
#define FORMAT_DEFAULT_SEPARATOR    FORMAT_FLAG_SPACE
#define FORMAT_DEFAULT              (FORMAT_FLAG_ASCII | \
                                     FORMAT_FLAG_HEADER | \
                                     FORMAT_DEFAULT_COORDINATES | \
                                     FORMAT_DEFAULT_NUMBERS | \
                                     FORMAT_DEFAULT_SEPARATOR)

// Exported functions ---------------------------------------------------------
uint32_t Convert_FormatSpecFromString(const char *str);
uint32_t Convert_FormatSpecToString(char *buf, uint32_t length, uint32_t format);
Buffer Convert_ConvertPolar(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count);
Buffer Convert_ConvertRaw(uint32_t format, const AD5933_ImpedanceData *data, uint32_t count);
Buffer Convert_ConvertGainFactor(const AD5933_GainFactor *gain);

void FreeBuffer(Buffer *buffer);

// ----------------------------------------------------------------------------

#endif /* CONVERT_H_ */
