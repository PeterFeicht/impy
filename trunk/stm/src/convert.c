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
#include <math.h>
#include "stm32f4xx.h"
#include "convert.h"
// Pull in support function needed for float formatting with printf
__ASM (".global _printf_float");

// Private function prototypes ------------------------------------------------
static Buffer Convert_PolarAscii(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count);
static Buffer Convert_PolarBinary(uint32_t format, const AD5933_ImpedancePolar *data, uint32_t count);
static Buffer Convert_RawAscii(uint32_t format, const AD5933_ImpedanceData *data, uint32_t count);
static Buffer Convert_RawBinary(uint32_t format, const AD5933_ImpedanceData *data, uint32_t count);

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
    // Second line break at end of transmission
    alloc += 2;
    
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
                        size += snprintf(buffer + size, alloc - size, "%lu%c%g%c%g\r\n",
                                data[j].Frequency, separator, data[j].Magnitude, separator, data[j].Angle);
                    }
                    break;
                    
                case FORMAT_FLAG_HEX:
                    for(uint32_t j = 0; j < count; j++)
                    {
                        uint32_t magnitude = *((uint32_t *)(&data[j].Magnitude));
                        uint32_t angle = *((uint32_t *)(&data[j].Angle));
                        size += snprintf(buffer + size, alloc - size, "%.8lx%c%.8lx%c%.8lx\r\n",
                                data[j].Frequency, separator, magnitude, separator, angle);
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
                        
                        size += snprintf(buffer + size, alloc - size, "%lu%c%g%c%g\r\n",
                                tmp.Frequency, separator, tmp.Real, separator, tmp.Imag);
                    }
                    break;
                    
                case FORMAT_FLAG_HEX:
                    for(uint32_t j = 0; j < count; j++)
                    {
                        AD5933_ImpedanceCartesian tmp;
                        AD5933_ConvertPolarToCartesian(&data[j], &tmp);
                        
                        uint32_t real = *((uint32_t *)&tmp.Real);
                        uint32_t imag = *((uint32_t *)&tmp.Imag);
                        size += snprintf(buffer + size, alloc - size, "%.8lx%c%.8lx%c%.8lx\r\n",
                                data[j].Frequency, separator, real, separator, imag);
                    }
                    break;
            }
            break;
    }
    // Second line break at end of transmission
    *((char *)(buffer + size++)) = '\r';
    *((char *)(buffer + size++)) = '\n';
    
    if(size < alloc - 100)
    {
        buffer = realloc(buffer, size);
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
    alloc += (format & FORMAT_FLAG_HEADER ? 4 : 0);
    
    buffer = malloc(alloc);
    if(buffer == NULL)
        return ret;
    
    if(format & FORMAT_FLAG_HEADER)
    {
#ifdef __ARMEB__
        *((uint32_t *)buffer) = alloc - 4;
#else
        *((uint32_t *)buffer) = __REV(alloc - 4);
#endif
        size += 4;
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
                AD5933_ImpedancePolar *tmp = buffer + size;
                tmp->Frequency = __REV(data[j].Frequency);
                *((uint32_t *)&tmp->Magnitude) = __REV(*((uint32_t *)&data[j].Magnitude));
                *((uint32_t *)&tmp->Angle) = __REV(*((uint32_t *)&data[j].Angle));
                size += sizeof(AD5933_ImpedancePolar);
            }
#endif
            break;
            
        case FORMAT_FLAG_CARTESIAN:
            for(uint32_t j = 0; j < count; j++)
            {
                AD5933_ImpedanceCartesian *tmp = buffer + size;
                AD5933_ConvertPolarToCartesian(&data[j], tmp);
#ifndef __ARMEB__
                tmp->Frequency = __REV(data[j].Frequency);
                *((uint32_t *)&tmp->Real) = __REV(*((uint32_t *)&tmp->Real));
                *((uint32_t *)&tmp->Imag) = __REV(*((uint32_t *)&tmp->Imag));
#endif
                size += sizeof(AD5933_ImpedanceCartesian);
            }
            break;
    }
    
    ret.data = buffer;
    ret.size = size;
    return ret;
}

/**
 * Converts raw data in ASCII format.
 * 
 * @param format Format specifier
 * @param data Data to convert
 * @param count Number of elements in array
 * @return Buffer with converted data
 */
static Buffer Convert_RawAscii(uint32_t format, const AD5933_ImpedanceData *data, uint32_t count)
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
            // ASCII format: uint32 < 100k + char + int16 + char + int16 + newline
            alloc = count * (6 + 1 + 6 + 1 + 6 + 2) + 1;
            break;
        case FORMAT_FLAG_HEX:
            // ASCII format: hex32 + char + hex16 + char + hex16 + newline
            alloc = count * (8 + 1 + 4 + 1 + 4 + 2) + 1;
            break;
    }
    // Second line break at end of transmission
    alloc += 2;
    
    if(format & FORMAT_FLAG_HEADER)
    {
        alloc += strlen(txtReal);
        alloc += strlen(txtImaginary);
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
        APPEND(txtReal);
        *((char *)(buffer + size++)) = separator;
        APPEND(txtImaginary);
        *((char *)(buffer + size++)) = '\r';
        *((char *)(buffer + size++)) = '\n';
    }
    
    switch(format & FORMAT_MASK_NUMBERS)
    {
        case FORMAT_FLAG_FLOAT:
            for(uint32_t j = 0; j < count; j++)
            {
                size += snprintf(buffer + size, alloc - size, "%lu%c%hi%c%hi\r\n",
                        data[j].Frequency, separator, data[j].Real, separator, data[j].Imag);
            }
            break;
            
        case FORMAT_FLAG_HEX:
            for(uint32_t j = 0; j < count; j++)
            {
                uint16_t real = *((uint16_t *)&data[j].Real);
                uint16_t imag = *((uint16_t *)&data[j].Imag);
                size += snprintf(buffer + size, alloc - size, "%.8lx%c%.4hx%c%.4hx\r\n",
                        data[j].Frequency, separator, real, separator, imag);
            }
            break;
    }
    // Second line break at end of transmission
    *((char *)(buffer + size++)) = '\r';
    *((char *)(buffer + size++)) = '\n';
    
    if(size < alloc - 100)
    {
        buffer = realloc(buffer, size);
    }
    
    ret.data = buffer;
    ret.size = size;
    return ret;
}

/**
 * Converts raw data in binary format.
 * 
 * @param format Format specifier
 * @param data Data to convert
 * @param count Number of elements in array
 * @return Buffer with converted data
 */
static Buffer Convert_RawBinary(uint32_t format, const AD5933_ImpedanceData *data, uint32_t count)
{
    uint32_t alloc = 0;
    void *buffer;
    uint32_t size = 0;
    
    Buffer ret = {
        .data = NULL,
        .size = 0
    };

    alloc = count * sizeof(AD5933_ImpedanceData) + (format & FORMAT_FLAG_HEADER ? 4 : 0);
    
    buffer = malloc(alloc);
    if(buffer == NULL)
        return ret;
    
    if(format & FORMAT_FLAG_HEADER)
    {
#ifdef __ARMEB__
        *((uint32_t *)buffer) = alloc - 4;
#else
        *((uint32_t *)buffer) = __REV(alloc - 4);
#endif
        size += 4;
    }
    
#ifdef __ARMEB__
    // If we have big endian data we can just copy it over
    memcpy(buffer + size, data, count * sizeof(AD5933_ImpedanceData));
#else
    for(uint32_t j = 0; j < count; j++)
    {
        AD5933_ImpedanceData *tmp = buffer + size;
        tmp->Frequency = __REV(data[j].Frequency);
        tmp->Real = __REV16(data[j].Real);
        tmp->Imag = __REV16(data[j].Imag);
        size += sizeof(AD5933_ImpedanceData);
    }
#endif
    
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
    
    if((flags & FORMAT_MASK_COORDINATES) == 0)
    {
        flags |= FORMAT_DEFAULT_COORDINATES;
    }
    
    if((flags & FORMAT_MASK_UNKNOWN) || !IS_POWER_OF_TWO(flags & FORMAT_MASK_COORDINATES))
    {
        return 0;
    }
    
    switch(flags & FORMAT_MASK_ENCODING)
    {
        case FORMAT_FLAG_ASCII:
            if((flags & FORMAT_MASK_NUMBERS) == 0)
            {
                flags |= FORMAT_DEFAULT_NUMBERS;
            }
            if((flags & FORMAT_MASK_SEPARATOR) == 0)
            {
                flags |= FORMAT_DEFAULT_SEPARATOR;
            }
            
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
 * Converts raw impedance data according to the format specified.
 * 
 * The buffer for the resulting data is obtained by {@code malloc} and can thus be {@code free}d when no longer needed.
 * If the required amount of memory cannot be allocated, a buffer containing a {@code NULL} pointer is returned.
 * 
 * @param format Format specification for the conversion, coordinate format is ignored
 * @param data Pointer to data to convert
 * @param count Number of elements in data array
 * @return A buffer structure with the converted data
 */
Buffer Convert_ConvertRaw(uint32_t format, const AD5933_ImpedanceData *data, uint32_t count)
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
            return Convert_RawBinary(format, data, count);
        case FORMAT_FLAG_ASCII:
           return Convert_RawAscii(format, data, count);
        default:
            return null;
    }
}

/**
 * Converts a gain factor to formatted floating point text suitable for parsing.
 * 
 * The buffer for the resulting data is obtained by {@code malloc} and can thus be {@code free}d when no longer needed.
 * If the required amount of memory cannot be allocated, a buffer containing a {@code NULL} pointer is returned.
 * 
 * @param gain Pointer to the gain factor to convert
 * @return A buffer structure with the converted gain factor
 */
Buffer Convert_ConvertGainFactor(const AD5933_GainFactor *gain)
{
    static const char* const point = "%s point gain factor\r\n";
    static const char* const freq1 = "freq1={%g";
    static const char* const offset = "}\r\noffset={%g";
    static const char* const slope = "}\r\nslope={%g";
    static const char* const phaseOffset = "}\r\nphaseOffset={%g";
    static const char* const phaseSlope = "}\r\nphaseSlope={%g";
    static const char* const end = "}\r\n\r\n";
    static const char* const next = ",%g";
    
    uint32_t alloc = 0;
    void *buffer;
    uint32_t size = 0;
    Buffer ret = {
        .data = NULL,
        .size = 0
    };
    
    if(gain == NULL)
    {
        return ret;
    }
    
    alloc += strlen(point) + strlen(freq1) + strlen(offset) + strlen(phaseOffset) + strlen(end);
    if(gain->is_2point)
    {
        alloc += strlen(slope) + strlen(phaseSlope);
        alloc += AD5933_NUM_CLOCKS * 60 /* 5 floats */;
    }
    else
    {
        alloc += AD5933_NUM_CLOCKS * 36 /* 3 floats */;
    }
    // Word space + terminating 0
    alloc += 2 + 1;
    
    buffer = malloc(alloc);
    if(buffer == NULL)
        return ret;
    
    size += snprintf(buffer + size, alloc - size, point, (gain->is_2point ? "Two" : "One"));
    
    size += snprintf(buffer + size, alloc - size, freq1, gain->ranges[0].freq1);
    for(uint32_t j = 1; j < AD5933_NUM_CLOCKS; j++)
    {
        size+= snprintf(buffer + size, alloc - size, next, gain->ranges[j].freq1);
    }
    
    size += snprintf(buffer + size, alloc - size, offset,
            (isnan(gain->ranges[0].freq1) ? NAN : gain->ranges[0].offset));
    for(uint32_t j = 1; j < AD5933_NUM_CLOCKS; j++)
    {
        size+= snprintf(buffer + size, alloc - size, next,
                (isnan(gain->ranges[j].freq1) ? NAN : gain->ranges[j].offset));
    }
    
    if(gain->is_2point)
    {
        size += snprintf(buffer + size, alloc - size, slope,
                (isnan(gain->ranges[0].freq1) ? NAN : gain->ranges[0].slope));
        for(uint32_t j = 1; j < AD5933_NUM_CLOCKS; j++)
        {
            size+= snprintf(buffer + size, alloc - size, next,
                    (isnan(gain->ranges[j].freq1) ? NAN : gain->ranges[j].slope));
        }
    }
    
    size += snprintf(buffer + size, alloc - size, phaseOffset,
            (isnan(gain->ranges[0].freq1) ? NAN : gain->ranges[0].phaseOffset));
    for(uint32_t j = 1; j < AD5933_NUM_CLOCKS; j++)
    {
        size+= snprintf(buffer + size, alloc - size, next,
                (isnan(gain->ranges[j].freq1) ? NAN : gain->ranges[j].phaseOffset));
    }
    
    if(gain->is_2point)
    {
        size += snprintf(buffer + size, alloc - size, phaseSlope,
                (isnan(gain->ranges[0].freq1) ? NAN : gain->ranges[0].phaseSlope));
        for(uint32_t j = 1; j < AD5933_NUM_CLOCKS; j++)
        {
            size+= snprintf(buffer + size, alloc - size, next,
                    (isnan(gain->ranges[j].freq1) ? NAN : gain->ranges[j].phaseSlope));
        }
    }
    
    // Copy termination
    size = memccpy(buffer + size, end, 0, alloc - size) - buffer - 1;

    ret.data = buffer;
    ret.size = size;
    return ret;
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
