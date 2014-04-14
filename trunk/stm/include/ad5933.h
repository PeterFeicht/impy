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

#define AD9533_ADDR                         0x0D

// I2C register addresses
#define AD5933_CTRL_H_ADDR                  0x80
#define AD5933_CTRL_L_ADDR                  0x81
#define AD9533_START_FREQ_H_ADDR            0x82
#define AD9533_START_FREQ_M_ADDR            0x83
#define AD9533_START_FREQ_L_ADDR            0x84
#define AD9533_FREQ_INCR_H_ADDR             0x85
#define AD9533_FREQ_INCR_M_ADDR             0x86
#define AD9533_FREQ_INCR_L_ADDR             0x87
#define AD9533_NUM_INCR_H_ADDR              0x88
#define AD9533_NUM_INCR_L_ADDR              0x89
#define AD9533_SETTL_H_ADDR                 0x8A
#define AD9533_SETTL_L_ADDR                 0x8B
#define AD5933_STATUS_ADDR                  0x8F
#define AD5933_TEMP_H_ADDR                  0x92
#define AD9533_TEMP_L_ADDR                  0x93
#define AD9533_REAL_H_ADDR                  0x94
#define AD9533_REAL_L_ADDR                  0x95
#define AD9533_IMAG_H_ADDR                  0x96
#define AD9533_IMAG_L_ADDR                  0x97

// Control functions (Control register D15:D12)
#define AD9533_FUNCTION_INIT_FREQ           0x01
#define AD9533_FUNCTION_START_SWEEP         0x02
#define AD9533_FUNCTION_INCREMENT_FREQ      0x03
#define AD9533_FUNCTION_REPEAT_FREQ         0x04
#define AD9533_FUNCTION_MEASURE_TEMP        0x09
#define AD9533_FUNCTION_POWER_DOWN          0x0A
#define AD9533_FUNCTION_STANDBY             0x0B

// Output voltage range (Control register D10:D9)
#define AD5933_VOLTAGE_2                    0x00
#define AD5933_VOLTAGE_1                    0x03
#define AD5933_VOLTAGE_0_2                  0x01
#define AD5933_VOLTAGE_0_4                  0x02

// PGA gain setting (Control register D8)
#define AD9533_GAIN_1                       0x01
#define AD9533_GAIN_5                       0x00

// Clock source (Control register D3)
#define AD9533_CLOCK_INTERNAL               0x00
#define AD9533_CLOCK_EXTERNAL               0x01

// Settling time multiplier (Settling time register D10:D9)
#define AD9533_SETTL_MULT_1                 0x00
#define AD9533_SETTL_MULT_2                 0x01
#define AD9533_SETTL_MULT_4                 0x03

// Status register bitmasks
#define AD9533_STATUS_VALID_TEMP            0x01
#define AD9533_STATUS_VALID_IMPEDANCE       0x02
#define AD9533_STATUS_SWEEP_COMPLETE        0x04

// Exported functions ---------------------------------------------------------



// ----------------------------------------------------------------------------

#endif /* AD5933_H_ */
