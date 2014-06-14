/**
 * @file    convert.c
 * @author  Peter Feichtinger
 * @date    13.06.2014
 * @brief   This file provides functions to convert measurement data between different formats.
 */

// Includes -------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4xx.h"
#include "convert.h"
// Pull in support function needed for float formatting with printf
__ASM (".global _printf_float");

// Private function prototypes ------------------------------------------------
static Buffer Convert_PolarAscii(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count);
static Buffer Convert_PolarBinary(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count);

// Private variables ----------------------------------------------------------
// These strings don't get localized for easier parsing
static const char *txtFrequency = "Frequency";
static const char *txtMagnitude = "Magnitude";
static const char *txtAngle = "Angle";
static const char *txtReal = "Real";
static const char *txtImaginary = "Imaginary";

// Private functions ----------------------------------------------------------

/**
 * Converts data in ASCII format.
 * 
 * @param format Format specifier
 * @param data Data to convert
 * @param count Number of elements in array
 * @return Buffer with converted data
 */
static Buffer Convert_PolarAscii(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count)
{
    uint32_t alloc = 0;
    void *buffer;
    uint32_t size = 0;
    char separator = ' ';
    
    Buffer ret = {
        .data = NULL,
        .size = 0
    };
    
    // Make a guess to the maximum amount of space needed
    switch(format & FORMAT_MASK_NUMBERS)
    {
        case FORMAT_FLAG_FLOAT:
            // ASCII format: int + char + float + char + float + newline
            alloc = count * (6 + 1 + 11 + 1 + 11 + 2) + 1;
            break;
        case FORMAT_FLAG_HEX:
            // ASCII format: hex + char + hex + char + hex + newline
            alloc = count * (8 + 1 + 8 + 1 + 8 + 2) + 1;
            break;
    }
    
    if(format & FORMAT_FLAG_HEADER)
    {
        switch(format & FORMAT_MASK_COORDINATES)
        {
            case FORMAT_FLAG_POLAR:
                alloc += strlen(txtMagnitude);
                alloc += strlen(txtAngle);
                break;
            case FORMAT_FLAG_CARTESIAN:
                alloc += strlen(txtReal);
                alloc += strlen(txtImaginary);
                break;
        }
        alloc += strlen(txtFrequency);
        // 2 separators + newline
        alloc += 2 + 2;
    }
    
    buffer = malloc(alloc);
    if(buffer == NULL)
        return ret;
    
    switch(format & FORMAT_MASK_SEPARATOR)
    {
        case FORMAT_FLAG_TAB:
            separator = '\t';
            break;
        case FORMAT_FLAG_COMMA:
            separator = ',';
            break;
    }
    
#define APPEND(X)   size = memccpy(buffer + size, (X), 0, alloc - size) - buffer - 1
    if(format & FORMAT_FLAG_HEADER)
    {
        APPEND(txtFrequency);
        *((char *)(buffer + size++)) = separator;
        switch(format & FORMAT_MASK_COORDINATES)
        {
            case FORMAT_FLAG_POLAR:
                APPEND(txtMagnitude);
                *((char *)(buffer + size++)) = separator;
                APPEND(txtAngle);
                break;
                
            case FORMAT_FLAG_CARTESIAN:
                APPEND(txtReal);
                *((char *)(buffer + size++)) = separator;
                APPEND(txtImaginary);
                break;
        }
        *((char *)(buffer + size++)) = '\r';
        *((char *)(buffer + size++)) = '\n';
    }
    
    switch(format & FORMAT_MASK_COORDINATES)
    {
        case FORMAT_FLAG_POLAR:
            switch(format & FORMAT_MASK_NUMBERS)
            {
                case FORMAT_FLAG_FLOAT:
                    for(uint32_t j = 0; j < count; j++)
                    {
                        size += snprintf((char *)(buffer + size), alloc - size, "%lu%c%g%c%g\r\n",
                                data[j].Frequency, separator, data[j].Magnitude, separator, data[j].Angle);
                    }
                    break;
                    
                case FORMAT_FLAG_HEX:
                    for(uint32_t j = 0; j < count; j++)
                    {
                        uint32_t magnitude = *((uint32_t *)(&data[j].Magnitude));
                        uint32_t angle = *((uint32_t *)(&data[j].Angle));
                        size += snprintf((char *)(buffer + size), alloc - size, "%.8lx%c%.8lx%c%.8lx\r\n",
#ifdef __ARMEB__
                                data[j].Frequency, separator, magnitude, separator, angle);
#else
                               // TODO reverse float byte order if necessary
                               __REV(data[j].Frequency), separator, magnitude, separator, angle);
#endif
                    }
                    break;
            }
            break;
            
        case FORMAT_FLAG_CARTESIAN:
            switch(format & FORMAT_MASK_NUMBERS)
            {
                case FORMAT_FLAG_FLOAT:
                    for(uint32_t j = 0; j < count; j++)
                    {
                        AD5933_ImpedanceCartesian tmp;
                        AD5933_ConvertPolarToCartesian(&data[j], &tmp);
                        
                        size += snprintf((char *)(buffer + size), alloc - size, "%lu%c%g%c%g\r\n",
                                tmp.Frequency, separator, tmp.Real, separator, tmp.Imag);
                    }
                    break;
                    
                case FORMAT_FLAG_HEX:
                    for(uint32_t j = 0; j < count; j++)
                    {
                        AD5933_ImpedanceCartesian tmp;
                        AD5933_ConvertPolarToCartesian(&data[j], &tmp);
                        
                        uint32_t real = *((uint32_t *)(&tmp.Real));
                        uint32_t imag = *((uint32_t *)(&tmp.Imag));
                        size += snprintf((char *)(buffer + size), alloc - size, "%.8lx%c%.8lx%c%.8lx\r\n",
#ifdef __ARMEB__
                                data[j].Frequency, separator, real, separator, imag);
#else
                                // TODO reverse float byte order if necessary
                                __REV(data[j].Frequency), separator, real, separator, imag);
#endif
                    }
                    break;
            }
            break;
    }
    
    if(size < alloc - 100)
    {
        realloc(buffer, size);
    }
    
    ret.data = buffer;
    ret.size = size;
    return ret;
}

/**
 * Converts data in binary format.
 * 
 * @param format Format specifier
 * @param data Data to convert
 * @param count Number of elements in array
 * @return Buffer with converted data
 */
static Buffer Convert_PolarBinary(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count)
{
    uint32_t alloc = 0;
    void *buffer;
    uint32_t size = 0;
    
    Buffer ret = {
        .data = NULL,
        .size = 0
    };

    switch(format & FORMAT_MASK_COORDINATES)
    {
        case FORMAT_FLAG_POLAR:
            alloc = count * sizeof(AD5933_ImpedancePolar);
            break;
        case FORMAT_FLAG_CARTESIAN:
            alloc = count * sizeof(AD5933_ImpedanceCartesian);
            break;
    }
    alloc += (format & FORMAT_FLAG_HEADER ? sizeof(count) : 0);
    
    buffer = malloc(alloc);
    if(buffer == NULL)
        return ret;
    
    if(format & FORMAT_FLAG_HEADER)
    {
#ifdef __ARMEB__
        *((uint32_t *)buffer) = count;
#else
        *((uint32_t *)buffer) = __REV(count);
#endif
        size += sizeof(count);
    }
    
    switch(format & FORMAT_MASK_COORDINATES)
    {
        case FORMAT_FLAG_POLAR:
#ifdef __ARMEB__
            // If we need big endian polar data we can take a shortcut and just copy the data
            memcpy(buffer + size, data, count * sizeof(AD5933_ImpedancePolar));
#else
            for(uint32_t j = 0; j < count; j++)
            {
                AD5933_ImpedancePolar *tmp = (AD5933_ImpedancePolar *)(buffer + size);
                tmp->Frequency = __REV(data[j].Frequency);
                // TODO determine if we need to reverse float byte order or not
                tmp->Magnitude = data[j].Magnitude;
                tmp->Angle = data[j].Angle;
                size += sizeof(AD5933_ImpedancePolar);
            }
#endif
            break;
            
        case FORMAT_FLAG_CARTESIAN:
            for(uint32_t j = 0; j < count; j++)
            {
                AD5933_ImpedanceCartesian *tmp = (AD5933_ImpedanceCartesian *)(buffer + size);
                AD5933_ConvertPolarToCartesian(&data[j], tmp);
#ifndef __ARMEB__
                tmp->Frequency = __REV(data[j].Frequency);
                // TODO determine if we need to reverse float byte order or not
#endif
                size += sizeof(AD5933_ImpedanceCartesian);
            }
            break;
    }
    
    ret.data = buffer;
    ret.size = size;
    return ret;
}

// Exported functions ---------------------------------------------------------

/**
 * Extract format flags from the specified string.
 * 
 * For example, for the string <i>BHP</i> the return value would be an integer with bits {@code FORMAT_FLAG_BINARY},
 * {@code FORMAT_FLAG_HEADER} and {@code FORMAT_FLAG_POLAR} set.
 * The return value for the string <i>AHP</i>, however, would be {@code 0}, since the specification for separator
 * character and number format are missing ({@code 0} is not a valid format specifier).
 * 
 * @param str Pointer to a zero terminated string
 * @return Format flags on success, {@code 0} otherwise
 */
uint32_t Convert_FormatSpecFromString(const char *str)
{
    uint32_t flags = 0;
    
    if(str == NULL)
    {
        return 0;
    }
    
    // Set flags for all specified characters
    for(const char *c = str; *c; c++)
    {
        if(!IS_FORMAT_FLAG(*c))
        {
            flags = 0;
            break;
        }
        flags |= FORMAT_FLAG_FROM_CHAR(*c);
    }
    
    // Instead of bailing out we could set defaults for missing flags
    if(!flags || (flags & FORMAT_MASK_UNKNOWN) ||
            !IS_POWER_OF_TWO(flags & FORMAT_MASK_COORDINATES))
    {
        return 0;
    }
    
    switch(flags & FORMAT_MASK_ENCODING)
    {
        case FORMAT_FLAG_ASCII:
            if(IS_POWER_OF_TWO(flags & FORMAT_MASK_NUMBERS) &&
                    IS_POWER_OF_TWO(flags & FORMAT_MASK_SEPARATOR))
            {
                return flags;
            }
            break;
            
        case FORMAT_FLAG_BINARY:
            return flags;
    }
    
    return 0;
}

/**
 * Convert a format specifier to a human readable representation of flags.
 * 
 * For example, for an integer with bits {@code FORMAT_FLAG_BINARY}, {@code FORMAT_FLAG_HEADER} and
 * {@code FORMAT_FLAG_POLAR} set the string would be {@code BPH}.
 * The format specifier is not checked for correctness, that means that a string generated by this function need not
 * necessarily result in a valid format specifier when passed to {@link Convert_FormatSpecFromString}.
 * 
 * @param buf Pointer to a buffer receiving the converted string (needs to be able to hold at least 10 characters)
 * @param length Length of the buffer, that is the maximum number of characters plus terminating zero
 * @param format The format specifier to convert
 * @return The number of characters written, not including the terminating zero
 */
uint32_t Convert_FormatSpecToString(char *buf, uint32_t length, uint32_t format)
{
    // We only accept buffers of at least 10 bytes to keep the code simple and aid expansion with more flags
    if(buf == NULL || length < 10)
    {
        return 0;
    }
    
    uint8_t pos = 0;
    memset(buf, 0, length);
    
    buf[pos++] = CHAR_FROM_FORMAT_FLAG(format & FORMAT_MASK_ENCODING);
    buf[pos++] = CHAR_FROM_FORMAT_FLAG(format & FORMAT_MASK_COORDINATES);
    
    switch(format & FORMAT_MASK_ENCODING)
    {
        case FORMAT_FLAG_ASCII:
            buf[pos++] = CHAR_FROM_FORMAT_FLAG(format & FORMAT_MASK_NUMBERS);
            buf[pos++] = CHAR_FROM_FORMAT_FLAG(format & FORMAT_MASK_SEPARATOR);
            break;
            
        case FORMAT_FLAG_BINARY:
            // Nothing special
            break;
    }
    
    if(format & FORMAT_FLAG_HEADER)
    {
        buf[pos++] = CHAR_FROM_FORMAT_FLAG(format & FORMAT_FLAG_HEADER);
    }
    
    return pos;
}

/**
 * Converts polar impedance data according to the format specified.
 * 
 * The buffer for the resulting data is obtained by {@code malloc} and can thus be {@code free}d when no longer needed.
 * If the required amount of memory cannot be allocated, a buffer containing a {@code NULL} pointer is returned.
 * 
 * @param format Format specification for the conversion
 * @param data Pointer to data to convert
 * @param count Number of elements in data array
 * @return A buffer structure with the converted data
 */
Buffer Convert_ConvertPolar(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count)
{
    Buffer null = {
        .data = NULL,
        .size = 0
    };
    
    if(data == NULL || count == 0)
    {
        return null;
    }
    
    switch(format & FORMAT_MASK_ENCODING)
    {
        case FORMAT_FLAG_BINARY:
            return Convert_PolarBinary(format, data, count);
        case FORMAT_FLAG_ASCII:
           return Convert_PolarAscii(format, data, count);
        default:
            return null;
    }
}

/**
 * Frees the memory allocated for the specified buffer and sets its values to zero.
 * 
 * @param buffer Pointer to the buffer to free
 */
void FreeBuffer(Buffer *buffer)
{
    if(buffer == NULL)
        return;
    
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}

// ----------------------------------------------------------------------------
