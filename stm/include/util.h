/**
 * @file    util.h
 * @author  Peter Feichtinger
 * @date    06.06.2014
 * @brief   Header file for the utility functions.
 */

#ifndef UTIL_H_
#define UTIL_H_

// Includes -------------------------------------------------------------------
#include <stdint.h>

// Constants ------------------------------------------------------------------

/**
 * @defgroup SI_PREFIX Characters for SI Unit Prefixes
 * @{
 */
#define SI_PREFIX_MILLI         'm'
#define SI_PREFIX_KILO          'k'
#define SI_PREFIX_MEGA          'M'
/** @} */

// Exported functions ---------------------------------------------------------

uint32_t IntFromSiString(const char *str, const char **end);
int SiStringFromInt(char *s, uint32_t size, uint32_t value);

int MacAddressFromString(const char *str, uint8_t *result);
int StringFromMacAddress(char *s, uint32_t size, const uint8_t *mac);

// ----------------------------------------------------------------------------

#endif /* UTIL_H_ */
