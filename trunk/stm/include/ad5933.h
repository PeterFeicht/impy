/**
 * @file    ad5933.h
 * @author  Peter Feichtinger
 * @date    14.04.2014
 * @brief   This file contains register definitions and control function prototypes for the AD5933 driver.
 */

#ifndef AD5933_H_
#define AD5933_H_

// Exported type definitions --------------------------------------------------
typedef struct
{
    uint32_t Start_Freq;        // Start frequency for the sweep
    uint32_t Freq_Increment;    // Frequency increment for the sweep
    uint16_t Num_Increments;    // Number of frequency points for the sweep
    uint16_t Settling_Cycles;   // Number of settling cycles before a measurement
} AD5933_SweepTypeDef;

typedef struct
{
    int16_t Real;   // Scaled real part of the impedance
    int16_t Imag;   // Scaled imaginary part of the impedance
} AD5933_ImpedanceTypeDef;

// Constants ------------------------------------------------------------------



// Exported functions ---------------------------------------------------------



// ----------------------------------------------------------------------------

#endif /* AD5933_H_ */
