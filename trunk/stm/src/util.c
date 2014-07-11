/**
 * @file    util.c
 * @author  Peter Feichtinger
 * @date    06.06.2014
 * @brief   This file contains utility functions for things like converting strings to numbers and such.
 */

// Includes -------------------------------------------------------------------
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "util.h"

// Private function prototypes ------------------------------------------------
static uint8_t Util_ConvertHexDigit(char c);

// Private functions ----------------------------------------------------------

/**
 * Convert a single hexadecimal digit to an integer value.
 * 
 * @param c The digit to convert
 * @return The corresponding integer value, or {@code 0xFF} in case {@code c} is not a proper hex digit
 */
static uint8_t Util_ConvertHexDigit(char c)
{
    char low = c & 0x0F;
    
    switch((c & 0xF0) >> 4)
    {
        case 3: /* 0 .. 9 */
            if(c <= '9')
            {
                return low;
            }
            break;
            
        case 4: /* A .. F */
        case 6: /* a .. f */
            if(low && low <= 6)
            {
                return low + 9;
            }
            break;
    }
    
    return 0xFF;
}

// Exported functions ---------------------------------------------------------

/**
 * Convert an integer value from a string with possible SI suffix (like {@code 100k}).
 * 
 * The string supplied to this function may not contain space characters inside the number, leading white space and
 * zeros are ignored. Parsing of the value is stopped at the first space character, so the string need not be
 * terminated.
 * 
 * If not {@code NULL}, the variable pointed to by {@code end} will receive the address of the first character after
 * the converted number, or {@code NULL} in case of an error.
 * 
 * Possible SI suffixes for this function are:
 *  + {@code SI_PREFIX_KILO} = k
 *  + {@code SI_PREFIX_MEGA} = M
 * 
 * @param str Pointer to a string containing a numeric value
 * @param end Pointer to a variable receiving the position of the first character after the number, or {@code NULL}
 * @return The converted numeric value, or {@code 0} in case of an error
 */
uint32_t IntFromSiString(const char *str, const char **end)
{
    uint32_t val = 0;
    const char *pos = str;
    
    if(str == NULL)
    {
        if(end != NULL)
            *end = NULL;
        return 0;
    } 
    
    // Ignore leading white space and zeros
    while(isspace((unsigned char)*pos) || *pos == '0')
    {
        pos++;
    }
    
    // Convert numeric part
    while(isdigit((unsigned char)*pos))
    {
        val = val * 10 + (*pos - '0');
        pos++;
    }
    
    // No suffix
    if(isspace((unsigned char)*pos) || *pos == 0)
    {
        if(end != NULL)
            *end = pos;
        return val;
    }
    
    // Check for valid suffix
    switch(*pos)
    {
        case SI_PREFIX_KILO:
            val *= 1000;
            break;
        case SI_PREFIX_MEGA:
            val *= 1000000;
            break;
        default:
            if(end != NULL)
                *end = NULL;
            return 0;
    }
    
    pos++;
    if(isspace((unsigned char)*pos) || *pos == 0)
    {
        if(end != NULL)
            *end = pos;
        return val;
    }
    else
    {
        if(end != NULL)
            *end = NULL;
        return 0;
    }
}

/**
 * Convert an integer value to a string with possible SI suffix (like {@code 100k}).
 * 
 * @param s Pointer to a buffer receiving the converted string
 * @param size Size of the buffer in bytes
 * @param value The value to convert
 * @return The number of characters that would have been written, if this value is greater or equal to {@code size}
 *         then not all characters have been written to the buffer
 */
int SiStringFromInt(char *s, uint32_t size, uint32_t value)
{
    char c = ' ';
    
    if(s == NULL)
    {
        return 0;
    }
    
    if(value % 1000 == 0)
    {
        value /= 1000;
        c = SI_PREFIX_KILO;
        
        if(value % 1000 == 0)
        {
            value /= 1000;
            c = SI_PREFIX_MEGA;
        }
    }
    
    if(c == ' ')
    {
        return snprintf(s, size, "%lu", value);
    }
    else
    {
        return snprintf(s, size, "%lu%c", value, c);
    }
}

/**
 * Converts a MAC address from a string in the format {@code 12:34:56:78:9A:BC}.
 * 
 * @param str Pointer to a string containing a MAC address
 * @param result Pointer to array receiving the converted address, needs to be able to store at least 6 values
 * @return The number of characters read if successful (always 17), {@code -1} otherwise
 */
int MacAddressFromString(const char *str, uint8_t *result)
{
    if(strlen(str) < 17)
    {
        return -1;
    }
    
    char sep = str[2];
    
    if(sep != ':' && sep != '-' && sep != ' ')
    {
        return -1;
    }
    
    for(uint32_t j = 0; j < 6; j++)
    {
        uint8_t hi = Util_ConvertHexDigit(*str++);
        uint8_t lo = Util_ConvertHexDigit(*str++);
        char s = *str++;
        if(hi == 0xFF || lo == 0xFF || (s && s != sep))
        {
            return -1;
        }
        result[j] = (hi << 4) | lo;
    }
    
    return 17;
}

/**
 * Convert a MAC address (6 bytes) to a human readable string in the format {@code 12-34-56-78-9A-BC}.
 * 
 * @param s Pointer to a buffer receiving the converted string
 * @param size Size of the buffer in bytes
 * @param mac The MAC address to convert, needs to have 6 elements
 * @return {@code -1} if {@code size} is less than 18, the number of characters written (excluding terminating 0)
 *         otherwise (always 17)
 */
int StringFromMacAddress(char *s, uint32_t size, const uint8_t *mac)
{
    static const char digits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    
    if(size < 18)
    {
        return -1;
    }
    
    for(uint32_t j = 0; j < 6; j++)
    {
        *s++ = digits[(mac[j] >> 4) & 0x0F];
        *s++ = digits[mac[j] & 0x0F];
        *s++ = '-';
    }
    
    s[-1] = 0;
    return 17;
}

// ----------------------------------------------------------------------------
