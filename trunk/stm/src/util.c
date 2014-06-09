/**
 * @file    util.c
 * @author  Peter
 * @date    06.06.2014
 * @brief   This file contains utility functions for things like converting strings to numbers and such.
 */

// Includes -------------------------------------------------------------------
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "util.h"

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

// ----------------------------------------------------------------------------
