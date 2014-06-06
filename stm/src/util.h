/**
 * @file    util.h
 * @author  Peter
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

uint32_t IntFromSiString(char *str);

// ----------------------------------------------------------------------------

#endif /* UTIL_H_ */
