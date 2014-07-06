/**
 * @file    main.c
 * @author  Peter Feichtinger
 * @date    13.04.2014
 * @brief   Main file for the impedance spectrometer firmware.
 */

// Includes -------------------------------------------------------------------
#include <math.h>
#include "main.h"

// Private function prototypes ------------------------------------------------
static void SetDefaults(void);
static void UpdateSettings(void);
static void Handle_TIM3_AD5933(void);
static void Handle_TIM3_EEPROM(void);

// Variables ------------------------------------------------------------------
USBD_HandleTypeDef hUsbDevice;
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi3;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim10;
CRC_HandleTypeDef hcrc;

// Default board configuration
EEPROM_ConfigurationBuffer board_config =
{
    .peripherals = {
        .sram = 0,
        .flash = 0,
        .eth = 0,
        .usbh = 0,
        .reserved = 0
    },
    .attenuations = { 1, 100, 0, 0 },
    .feedback_resistors = { 100, 1000, 10000, 100000, 1000000, 0, 0, 0 },
    .calibration_values = { 10, 100, 1000, 10000, 100000, 1000000 },
    .coupling_tau = 110,
    .eth_mac = { 0x11, 0x00, 0xAA, 0x00, 0x00, 0x00 },
    .sram_size = 0,
    .flash_size = 0,
    .reserved = { 0 },
    .checksum = 0
};

static EEPROM_SettingsBuffer settings;

// AD5933 driver values
static AD5933_Sweep sweep;
static AD5933_RangeSettings range;
static uint32_t stopFreq;
static uint8_t lastPort;
static uint8_t autorange;       // Whether autoranging should be enabled for the next sweep

// XXX should we allocate buffers dynamically?
static AD5933_ImpedanceData bufData[AD5933_MAX_NUM_INCREMENTS + 1];
static uint8_t validData = 0;
static AD5933_ImpedancePolar bufPolar[AD5933_MAX_NUM_INCREMENTS + 1];
static uint8_t validPolar = 0;
static AD5933_GainFactor dataGainFactor;    // Gain factor for valid raw data
static uint32_t pointCount = 0;
static uint8_t interrupted = 0;
static AD5933_GainFactor gainFactor;        // Current gain factor, could have changed since the measurement finished
static uint8_t validGain = 0;               // Whether gainFactor is valid for the current sweep parameters

// main and Interrupt handlers ------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

int main(int argc, char* argv[])
{
    // At this stage the system clock should have already been configured at high speed.
    MX_Init();
    Console_Init();
    SetDefaults();
    
#if defined(BOARD_HAS_EEPROM) && BOARD_HAS_EEPROM == 1
    EE_Init(&hi2c1, &hcrc, EEPROM_E2_PIN_SET);
    
    if(EE_ReadConfiguration(&board_config) != EE_OK)
    {
        // Bad configuration, write default values to EEPROM
        EE_WriteConfiguration(&board_config);
    }
    
    if(EE_ReadSettings(&settings) == EE_OK)
    {
        // Populate the various variables with settings read from EEPROM, the opposite of what UpdateSettings does
        sweep.Num_Increments = settings.num_steps;
        sweep.Start_Freq = settings.start_freq;
        stopFreq = settings.stop_freq;
        sweep.Settling_Cycles = settings.settling_cycles & AD5933_MAX_SETTL;
        sweep.Settling_Mult = settings.settling_cycles & ~AD5933_MAX_SETTL;
        sweep.Averages = settings.averages;

        range.PGA_Gain = (settings.flags.pga_enabled ? AD5933_GAIN_5 : AD5933_GAIN_1);
        range.Voltage_Range = settings.voltage;
        range.Attenuation = settings.attenuation;
        range.Feedback_Value = settings.feedback;

        autorange = settings.flags.autorange;
        Console_SetFormat(settings.format_spec);
    }
    else
    {
        // Settings could not be read, write default settings to EEPROM
        EE_WriteSettings(&settings);
    }
#endif
    
    AD5933_Init(&hi2c1, &htim10);
    
    // Start timer for periodic interrupt generation
    HAL_TIM_Base_Start_IT(&htim3);
    
    while(1)
    {
        // Do stuff.
        HAL_GPIO_TogglePin(LED_PORT, LED_BLUE);
        HAL_Delay(600);
    }
}

#pragma GCC diagnostic pop

/**
 * Calls the appropriate functions for timer period interrupts.
 * 
 * @param htim Timer handle
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM3)
    {
        Handle_TIM3_AD5933();
        Handle_TIM3_EEPROM();
    }
}

/**
 * Handles TIM3 period elapsed event for the AD5933 driver.
 */
static void Handle_TIM3_AD5933(void)
{
    static AD5933_Status prevStatus = AD_UNINIT;

    AD5933_Status status = AD5933_TimerCallback();
    if(prevStatus != status)
    {
        if(status == AD_FINISH_IMPEDANCE)
        {
            pointCount = AD5933_GetSweepCount();
            interrupted = 0;
            
            if(prevStatus == AD_MEASURE_IMPEDANCE)
            {
                validData = 1;
                validPolar = 0;
                dataGainFactor = gainFactor;
            }
            else if(prevStatus == AD_MEASURE_IMPEDANCE_AUTORANGE)
            {
                validData = 0;
                validPolar = 1;
            }
        }
    }
    prevStatus = status;
}

/**
 * Handles TIM3 period elapsed event for the EEPROM driver.
 */
static void Handle_TIM3_EEPROM(void)
{
#if defined(BOARD_HAS_EEPROM) && BOARD_HAS_EEPROM == 1
    static EEPROM_Status prevStatus = EE_UNINIT;
    
    EEPROM_Status status = EE_TimerCallback();
    if(prevStatus != status)
    {
        // Nothing to do yet
    }
    prevStatus = status;
#endif
}

// Private functions ----------------------------------------------------------

static void SetDefaults(void)
{
    sweep.Num_Increments = 50;
    sweep.Start_Freq = 10000;
    stopFreq = 100000;
    sweep.Settling_Cycles = 16;
    sweep.Settling_Mult = AD5933_SETTL_MULT_1;
    sweep.Averages = 1;
    
    range.PGA_Gain = AD5933_GAIN_1;
    range.Voltage_Range = AD5933_VOLTAGE_1;
    range.Attenuation = 1;
    range.Feedback_Value = 10000;
    
    autorange = 0;
    validData = 0;
    validPolar = 0;
    validGain = 0;
    pointCount = 0;
    interrupted = 0;
    
    UpdateSettings();
}

/**
 * Update settings structure from sweep values.
 */
static void UpdateSettings(void)
{
    settings.num_steps = sweep.Num_Increments;
    settings.start_freq = sweep.Start_Freq;
    settings.stop_freq = stopFreq;
    settings.settling_cycles = sweep.Settling_Cycles | sweep.Settling_Mult;
    settings.averages = sweep.Averages;
    
    settings.flags.pga_enabled = (range.PGA_Gain == AD5933_GAIN_5 ? 1 : 0);
    settings.voltage = range.Voltage_Range;
    settings.attenuation = range.Attenuation;
    settings.feedback = range.Feedback_Value;
    
    settings.flags.autorange = (autorange ? 1 : 0);
    settings.format_spec = Console_GetFormat();
}

// Exported functions ---------------------------------------------------------

/**
 * Sets the start frequency used for a sweep.
 * The value needs to be between {@link FREQ_MIN} and {@link FREQ_MAX}, and less than the stop frequency.
 * 
 * @param freq the frequency in Hz
 * @return {@link Board_Error} code
 */
Board_Error Board_SetStartFreq(uint32_t freq)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if(freq < AD5933_FREQ_MIN || freq > AD5933_FREQ_MAX || freq >= stopFreq)
    {
        return BOARD_ERROR;
    }
    
    sweep.Start_Freq = freq;
    validGain = 0;
    return BOARD_OK;
}

/**
 * Sets the stop frequency used for a sweep.
 * The value needs to be between {@link FREQ_MIN} and {@link FREQ_MAX}, and greater than the start frequency.
 * 
 * @param freq the frequency in Hz
 * @return {@link Board_Error} code
 */
Board_Error Board_SetStopFreq(uint32_t freq)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if(freq < AD5933_FREQ_MIN || freq > AD5933_FREQ_MAX || freq <= sweep.Start_Freq)
    {
        return BOARD_ERROR;
    }
    
    stopFreq = freq;
    validGain = 0;
    return BOARD_OK;
}

/**
 * Sets the number of frequency increments used for a sweep.
 * The value cannot be greater than the difference in start and stop frequency (the resolution is 1 Hz) and needs to be
 * in the range from {@code 0} to {@link AD5933_MAX_NUM_INCREMENTS}.
 * 
 * @param steps the number of points to measure
 * @return {@link Board_Error} code
 */
Board_Error Board_SetFreqSteps(uint16_t steps)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if((stopFreq - sweep.Start_Freq) < steps || steps > AD5933_MAX_NUM_INCREMENTS)
    {
        return BOARD_ERROR;
    }
    
    sweep.Num_Increments = steps;
    return BOARD_OK;
}

/**
 * Sets the number of settling cycles used for a sweep.
 * The value of {@code cycles} needs to be in the range from {@code 0} to {@link AD5933_MAX_SETTL}, {@code multiplier}
 * can be {@code 1}, {@code 2} or {@code 4}.
 * 
 * @param cycles The number of settling cycles
 * @param multiplier Multiplier for the cycle number
 * @return {@link Board_Error} code
 */
Board_Error Board_SetSettlingCycles(uint16_t cycles, uint8_t multiplier)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if(cycles > AD5933_MAX_SETTL)
    {
        return BOARD_ERROR;
    }
    
    switch(multiplier)
    {
        case 1:
            sweep.Settling_Mult = AD5933_SETTL_MULT_1;
            break;
        case 2:
            sweep.Settling_Mult = AD5933_SETTL_MULT_2;
            break;
        case 4:
            sweep.Settling_Mult = AD5933_SETTL_MULT_4;
            break;
        default:
            return BOARD_ERROR;
    }
    sweep.Settling_Cycles = cycles;
    
    return BOARD_OK;
}

/**
 * Sets the voltage range used for a sweep.
 * The value can be <i>0.2V</i>, <i>0.4V</i>, <i>1V</i> or <i>2V</i>, attenuated by the values in
 * {@link AD5933_ATTENUATION_PORT}.
 * 
 * @param range The output voltage in mV
 * @return {@link Board_Error} code
 */
Board_Error Board_SetVoltageRange(uint16_t voltage)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    
    static const uint16_t voltages[] = { 200, 400, 1000, 2000 };
    static const uint16_t voltage_values[] = {
        AD5933_VOLTAGE_0_2,
        AD5933_VOLTAGE_0_4,
        AD5933_VOLTAGE_1,
        AD5933_VOLTAGE_2
    };
    
    for(uint32_t j = 0; j < NUMEL(board_config.attenuations) && board_config.attenuations[j]; j++)
    {
        for(uint32_t k = 0; k < NUMEL(voltages); k++)
        {
            if(voltage == voltages[k] / board_config.attenuations[j])
            {
                range.Attenuation = board_config.attenuations[j];
                range.Voltage_Range = voltage_values[k];
                validGain = 0;
                return BOARD_OK;
            }
        }
    }
    
    return BOARD_ERROR;
}

/**
 * Sets whether the x5 gain stage of the PGA is enabled. This setting is ignored, if autoranging is enabled.
 * 
 * @param enable {@code 0} to disable the gain stage, nonzero value to enable
 * @return {@link Board_Error} code
 */
Board_Error Board_SetPGA(uint8_t enable)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    
    if(!autorange)
    {
        range.PGA_Gain = (enable ? AD5933_GAIN_5 : AD5933_GAIN_1);
        validGain = 0;
    }
    
    return BOARD_OK;
}

/**
 * Sets whether autoranging is enabled.
 * 
 * @param enable {@code 0} to disable autoranging, nonzero value to enable
 * @return {@code BOARD_OK}
 */
Board_Error Board_SetAutorange(uint8_t enable)
{
    autorange = enable;
    return BOARD_OK;
}

/**
 * Sets the value of the current feedback resistor in Ohms.
 * 
 * @param ohms the value of the resistor in Ohms
 * @return {@link Board_Error} code
 */
Board_Error Board_SetFeedback(uint32_t ohms)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    
    if(!autorange)
    {
        uint32_t fb = 0;
        for(uint32_t j = 0; j < NUMEL(board_config.feedback_resistors) && board_config.feedback_resistors[j]; j++)
        {
            if(ohms == board_config.feedback_resistors[j])
            {
                fb = board_config.feedback_resistors[j];
                break;
            }
        }
        if(!fb)
        {
            return BOARD_ERROR;
        }
        range.Feedback_Value = fb;
        validGain = 0;
    }
    
    return BOARD_OK;
}

/**
 * Sets the number of averages for each frequency point.
 * 
 * @param value the number of averages, a value of {@code 1} means no averaging is performed
 * @return {@link Board_Error} code
 */
Board_Error Board_SetAverages(uint16_t value)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if(value == 0)
    {
        return BOARD_ERROR;
    }
    
    sweep.Averages = value;
    return BOARD_OK;
}

/**
 * Gets the current start frequency used for a sweep.
 */
uint32_t Board_GetStartFreq(void)
{
    return sweep.Start_Freq;
}

/**
 * Gets the current stop frequency used for a sweep.
 */
uint32_t Board_GetStopFreq(void)
{
    return stopFreq;
}

/**
 * Gets the current number of frequency increments used for a sweep.
 */
uint16_t Board_GetFreqSteps(void)
{
    return sweep.Num_Increments;
}

/**
 * Gets the current range settings.
 */
const AD5933_RangeSettings* Board_GetRangeSettings(void)
{
    return &range;
}

/**
 * Gets the current number of settling cycles.
 */
uint16_t Board_GetSettlingCycles(void)
{
    switch(sweep.Settling_Mult)
    {
        case AD5933_SETTL_MULT_2:
            return sweep.Settling_Cycles << 1;
        case AD5933_SETTL_MULT_4:
            return sweep.Settling_Cycles << 2;
        default:
            return sweep.Settling_Cycles;
    }
}

/**
 * Gets whether autoranging is enabled or not.
 * 
 * Note that the value of an active sweep can be different if it has been set since the sweep has started.
 * To get the value for the active sweep, use {@link  Board_GetStatus}.
 * 
 * @return {@code 0} if autoranging is disabled, a nonzero value otherwise
 */
uint8_t Board_GetAutorange(void)
{
    return autorange;
}

/**
 * Gets the current number of averages for each frequency point.
 */
uint16_t Board_GetAverages(void)
{
    return sweep.Averages;
}

/**
 * Gets the current measurement status.
 * 
 * @param result Pointer to a status structure to be populated
 */
void Board_GetStatus(Board_Status *result)
{
    result->ad_status = AD5933_GetStatus();
    result->point = AD5933_GetSweepCount();
    result->totalPoints = sweep.Num_Increments;
    result->interrupted = interrupted;
    result->validGainFactor = validGain;
    result->validData = validData || validPolar;
    
    switch(result->ad_status)
    {
        case AD_MEASURE_IMPEDANCE:
            result->autorange = 0;
            break;
        case AD_MEASURE_IMPEDANCE_AUTORANGE:
            result->autorange = 1;
            break;
        default:
            result->autorange = autorange;
            break;
    }
}

/**
 * Resets the board to a known state. This should be made accessible to the user, so that after a potentially wrong
 * configuration (whatever <i>wrong</i> may be), a known state that is documented can easily be restored.
 * 
 * Things to consider:
 *  + Saved configuration in the EEPROM is ignored
 *  + Running measurements are stopped, AD5933 is reset
 */
void Board_Reset(void)
{
    SetDefaults();
    Console_Init();
    Board_Standby();
}

/**
 * Puts the AD5933 in standby mode, switches off the low speed clock and disconnects the output ports.
 */
void Board_Standby(void)
{
    uint8_t data = ADG725_CHIP_ENABLE_NOT;
    HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi3, &data, 1, BOARD_SPI_TIMEOUT);
    HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
    
    HAL_TIM_OC_Stop(&htim10, AD5933_CLK_TIM_CHANNEL);
    AD5933_Reset();
}

/**
 * Gets a pointer to the converted measurement data in polar format.
 * 
 * @param count Pointer to a variable receiving the number of points in the buffer
 * @return Pointer to the data buffer, or {@code NULL} if no data is available
 */
const AD5933_ImpedancePolar* Board_GetDataPolar(uint32_t *count)
{
    if(!validPolar)
    {
        if(validData)
        {
            for(uint32_t j = 0; j < pointCount; j++)
            {
                bufPolar[j].Frequency = bufData[j].Frequency;
                bufPolar[j].Magnitude = AD5933_GetMagnitude(&bufData[j], &dataGainFactor);
                bufPolar[j].Angle = AD5933_GetPhase(&bufData[j], &dataGainFactor);
            }
            validPolar = 1;
        }
        else
        {
            // Neither raw nor polar data, nothing to return
            *count = 0;
            return NULL;
        }
    }
    
    *count = pointCount;
    return &bufPolar[0];
}

/**
 * Gets a pointer to the raw measurement data.
 * 
 * @param count Pointer to a variable receiving the number of points in the buffer
 * @return Pointer to the data buffer, or {@code NULL} if no raw data is available
 */
const AD5933_ImpedanceData* Board_GetDataRaw(uint32_t *count)
{
    if(validData)
    {
        *count = pointCount;
        return &bufData[0];
    }
    else
    {
        *count = 0;
        return NULL;
    }
}

/**
 * Gets a pointer to the calibrated gain factor.
 * 
 * Note that a gain factor is only valid when no range settings (voltage range, feedback resistor, etc.) have
 * been changed since a calibration was performed.
 * 
 * @return Pointer to gain factor, or {@code NULL} if calibration has not been performed
 */
const AD5933_GainFactor* Board_GetGainFactor(void)
{
    if(validGain)
    {
        return &gainFactor;
    }
    else
    {
        return NULL;
    }
}

/**
 * Initiates a frequency sweep on the specified port.
 * 
 * @param port Port number for the sweep, needs to be in the range 0 to {@link PORT_MAX}
 * @return {@link Board_Error} code
 */
Board_Error Board_StartSweep(uint8_t port)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if(port > PORT_MAX || (!validGain && !autorange))
    {
        return BOARD_ERROR;
    }

    // Set output mux
    HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
    HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
    
    // TODO implement autorange
    sweep.Freq_Increment = (stopFreq - sweep.Start_Freq) / (sweep.Num_Increments != 0 ? sweep.Num_Increments : 1);
    
    if(AD5933_MeasureImpedance(&sweep, &range, &bufData[0]) == AD_OK)
    {
        validPolar = 0;
        validData = 0;
        interrupted = 0;
        lastPort = port;
        return BOARD_OK;
    }
    else
    {
        return BOARD_ERROR;
    }
}

/**
 * Stops a currently running frequency measurement, if any. Always resets the AD5933 and disconnects output ports.
 * 
 * When no measurement is running, this function can be used to switch off the AD5933 so no output signal is generated.
 * 
 * @return {@link BOARD_OK}
 */
Board_Error Board_StopSweep(void)
{
    AD5933_Status status = AD5933_GetStatus();
    if(status == AD_MEASURE_IMPEDANCE)
    {
        interrupted = 1;
        validData = 1;
        dataGainFactor = gainFactor;
    }
    else if(status == AD_MEASURE_IMPEDANCE_AUTORANGE)
    {
        interrupted = 1;
        validPolar = 1;
    }
    
    Board_Standby();
    return BOARD_OK;
}

/**
 * Gets the port of the active (or last) sweep.
 */
uint8_t Board_GetPort(void)
{
    return lastPort;
}

/**
 * Measures a single frequency point on the specified port with the current range settings.
 * 
 * @param port The port to measure on
 * @param freq The frequency to measure
 * @param result Pointer to a structure receiving the converted impedance value
 * @return {@link Board_Error} code
 */
Board_Error Board_MeasureSingleFrequency(uint8_t port, uint32_t freq, AD5933_ImpedancePolar *result)
{
    assert_param(result != NULL);
    
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    if(freq < AD5933_FREQ_MIN || freq > AD5933_FREQ_MAX || port > PORT_MAX || (!validGain && !autorange))
    {
        return BOARD_ERROR;
    }
    // Calibration is only valid in the current frequency range
    if(freq < sweep.Start_Freq || freq > stopFreq)
    {
        return BOARD_ERROR;
    }
    
    // AD5933 cannot measure a single frequency, make room for two
    AD5933_ImpedanceData buffer[2];
    AD5933_Sweep sw = sweep;
    sw.Start_Freq = freq;
    sw.Freq_Increment = 1;
    sw.Num_Increments = 1;
    
    // TODO implement autorange
    AD5933_MeasureImpedance(&sw, &range, &buffer[0]);
    while(AD5933_GetStatus() != AD_FINISH_IMPEDANCE)
    {
        HAL_Delay(2);
    }
    
    result->Frequency = freq;
    result->Magnitude = AD5933_GetMagnitude(&buffer[0], &gainFactor);
    result->Angle = AD5933_GetPhase(&buffer[0], &gainFactor);
    
    return BOARD_OK;
}

/**
 * Initiates a temperature measurement from the specified source and returns the value measured.
 * This function blocks until a value is available.
 * 
 * @param what Which temperature to measure
 * @return The measured temperature, or {@code NaN} in case of an error
 */
float Board_MeasureTemperature(Board_TemperatureSource what)
{
    float temp;
    
    switch(what)
    {
        case TEMP_AD5933:
            if(AD5933_MeasureTemperature(&temp) != AD_OK)
            {
                return NAN;
            }
            // TODO use callbacks
            while(AD5933_GetStatus() == AD_MEASURE_TEMP)
            {
                HAL_Delay(1);
            }
            break;
            
        default:
            return NAN;
    }
    
    return temp;
}

/**
 * Initiates a calibration measurement with the specified calibration resistor.
 * 
 * @param ohms Calibration resistor value in Ohms, must be one of the {@link CAL_PORT} values.
 * @return {@link Board_Error} code
 */
Board_Error Board_Calibrate(uint32_t ohms)
{
    if(AD5933_IsBusy())
    {
        return BOARD_BUSY;
    }
    
    if(!autorange)
    {
        AD5933_CalibrationSpec spec;
        AD5933_GainFactorData gainData;
        uint8_t cal = 0;
        for(uint32_t j = 0; j < NUMEL(board_config.calibration_values) && board_config.calibration_values[j]; j++)
        {
            if(ohms == board_config.calibration_values[j])
            {
                cal = CAL_PORT_MIN + j;
                spec.impedance = board_config.calibration_values[j];
                break;
            }
        }
        if(!cal)
        {
            return BOARD_ERROR;
        }
        
        spec.freq1 = sweep.Start_Freq;
        spec.freq2 = stopFreq;
        spec.is_2point = 1;
        
        // Set output mux
        cal = cal & ADG725_MASK_PORT;
        HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
        HAL_SPI_Transmit(&hspi3, &cal, 1, BOARD_SPI_TIMEOUT);
        HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
        
        AD5933_Calibrate(&spec, &range, &gainData);
        // TODO use callback
        while(AD5933_GetStatus() == AD_CALIBRATE)
        {
            HAL_Delay(1);
        }
        AD5933_CalculateGainFactor(&gainData, &gainFactor);
        validGain = 1;
    }
    
    return BOARD_OK;
}

// ----------------------------------------------------------------------------
