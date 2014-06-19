/**
 * @file    console.c
 * @author  Peter Feichtinger
 * @date    16.05.2014
 * @brief   This file contains the definition of the console interface.
 */

// Includes -------------------------------------------------------------------
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "console.h"
#include "main.h"
#include "util.h"
#include "convert.h"
// Pull in support function needed for float formatting with printf
__ASM (".global _printf_float");

// Private type definitions ---------------------------------------------------
typedef enum
{
    CON_ARG_INVALID = 0,
    // board read
    CON_ARG_READ_FORMAT,
    // board set/get
    CON_ARG_SET_AUTORANGE,
    CON_ARG_SET_ECHO,
    CON_ARG_SET_FEEDBACK,
    CON_ARG_SET_FORMAT,
    CON_ARG_SET_GAIN,
    CON_ARG_SET_SETTL,
    CON_ARG_SET_START,
    CON_ARG_SET_STEPS,
    CON_ARG_SET_STOP,
    CON_ARG_SET_VOLTAGE,
    // eth set
    CON_ARG_SET_DHCP,
    CON_ARG_SET_IP
} Console_ArgID;

typedef enum
{
    CON_FLAG_INVALID = 0,
    CON_FLAG_ON,
    CON_FLAG_OFF
} Console_FlagValue;

typedef struct
{
    const char *data;
    uint32_t length;
} String;

typedef struct
{
    const char *cmd;    //!< The command the help text is for
    String text;        //!< The help text
} Console_HelpEntry;

/**
 * Pointer to a function that processes a specific command.
 * 
 * Note that this file does not conform to the standard values {@code (argc, argv)} in that {@code argc} contains the
 * number of elements in {@code argv}, but {@code argv} has no {@code NULL} pointer at that index.
 * 
 * @param argc Number of arguments in the command line
 * @param argv Array of arguments
 */
typedef void (*Console_CommandFunc)(uint32_t argc, char **argv);

typedef enum
{
    CON_FLAG = 0,   //!< Argument is either 'on' or 'off'
    CON_INT,        //!< Argument is an integer number
    CON_STRING      //!< Argument is a string of nonzero length
} Console_ArgType;

typedef struct
{
    const char *arg;        //!< Name of this argument
    Console_ArgID id;       //!< ID of this argument
    Console_ArgType type;   //!< Type of this argument
} Console_Arg;

typedef struct
{
    const char *cmd;                //!< Command name
    Console_CommandFunc handler;    //!< Pointer to the function that processes this command
} Console_Command;

// Private function prototypes ------------------------------------------------
static void Console_InitHelp(void);
static uint32_t Console_GetArguments(char *cmdline);
static uint8_t Console_CallProcessor(uint32_t argc, char **argv, const Console_Command *cmds, uint32_t count);
static const Console_Arg* Console_GetArg(const char *arg, const Console_Arg *args, uint32_t count);
static const char* Console_GetArgValue(const char *arg);
static Console_FlagValue Console_GetFlag(const char *str);
// Command line processors
static void Console_Board(uint32_t argc, char **argv);
static void Console_BoardCalibrate(uint32_t argc, char **argv);
static void Console_BoardGet(uint32_t argc, char **argv);
static void Console_BoardInfo(uint32_t argc, char **argv);
static void Console_BoardMeasure(uint32_t argc, char **argv);
static void Console_BoardRead(uint32_t argc, char **argv);
static void Console_BoardSet(uint32_t argc, char **argv);
static void Console_BoardStart(uint32_t argc, char **argv);
static void Console_BoardStatus(uint32_t argc, char **argv);
static void Console_BoardStop(uint32_t argc, char **argv);
static void Console_BoardTemp(uint32_t argc, char **argv);
static void Console_Eth(uint32_t argc, char **argv);
static void Console_Usb(uint32_t argc, char **argv);
static void Console_Help(uint32_t argc, char **argv);
static void Console_Debug(uint32_t argc, char **argv);
#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
static void Console_EthDisable(uint32_t argc, char **argv);
static void Console_EthEnable(uint32_t argc, char **argv);
static void Console_EthSet(uint32_t argc, char **argv);
static void Console_EthStatus(uint32_t argc, char **argv);
#endif
#if defined(BOARD_HAS_USBH) && BOARD_HAS_USBH == 1
static void Console_UsbEject(uint32_t argc, char **argv);
static void Console_UsbInfo(uint32_t argc, char **argv);
static void Console_UsbLs(uint32_t argc, char **argv);
static void Console_UsbStatus(uint32_t argc, char **argv);
static void Console_UsbWrite(uint32_t argc, char **argv);
#endif

// Private variables ----------------------------------------------------------
static char *arguments[CON_MAX_ARGUMENTS];
static uint32_t format_spec;
static Buffer board_read_data = {
    .data = NULL,
    .size = 0
};

// Console definition
extern const char helptext_start;
extern const char helptext_end;
static String strHelp;
// All help topics from command-line.txt need to be added here
#define TOPIC(X)    { X, { NULL, 0 } }
static Console_HelpEntry txtHelpTopics[] = {
    TOPIC("eth"),
    TOPIC("usb"),
    TOPIC("format"),
    TOPIC("settl"),
    TOPIC("voltage"),
    TOPIC("autorange"),
    TOPIC("calibrate"),
    TOPIC("echo"),
};
// Those are the top level commands, subcommands are called from their respective processing functions
static const Console_Command commands[] = {
    { "board", Console_Board },
    { "eth", Console_Eth },
    { "usb", Console_Usb },
    { "help", Console_Help },
    { "debug", Console_Debug }
};
// Those are the values that can be set with 'board set' and read with 'board get'
static const Console_Arg argsBoardSet[] = {
    { "start", CON_ARG_SET_START, CON_INT },
    { "stop", CON_ARG_SET_STOP, CON_INT },
    { "steps", CON_ARG_SET_STEPS, CON_INT },
    { "settl", CON_ARG_SET_SETTL, CON_STRING },
    { "voltage", CON_ARG_SET_VOLTAGE, CON_INT },
    { "gain", CON_ARG_SET_GAIN, CON_FLAG },
    { "feedback", CON_ARG_SET_FEEDBACK, CON_INT },
    { "format", CON_ARG_SET_FORMAT, CON_STRING },
    { "autorange", CON_ARG_SET_AUTORANGE, CON_FLAG },
    { "echo", CON_ARG_SET_ECHO, CON_FLAG }
};

// Include string definitions in the desired language
#include "strings_en.h"

// Private functions ----------------------------------------------------------

/**
 * Sets the main help string and fills the help topics array.
 */
static void Console_InitHelp(void)
{
    // This text is at the beginning of a help topic start line
    static const char *topic = "\r\nhelp ";
    uint32_t topicLen = strlen(topic);
    const char *help;
    String *prev = NULL;
    
    // Set usage message
    strHelp.data = &helptext_start;
    help = strstr(strHelp.data, "----");
    strHelp.length = help - strHelp.data;
    
    // Look for specific help messages
    while((help = strstr(help, topic)) != NULL)
    {
        if(prev != NULL)
        {
            prev->length = help - prev->data + 2;
            prev = NULL;
        }
        
        help += topicLen;
        const char *tmp = strchr(help, ':');
        if(tmp == NULL)
            break;
        
        for(uint32_t j = 0; j < NUMEL(txtHelpTopics); j++)
        {
            uint32_t len = max(strlen(txtHelpTopics[j].cmd), tmp - help);
            if(strncmp(help, txtHelpTopics[j].cmd, len) == 0)
            {
                prev = &txtHelpTopics[j].text;
                prev->data = tmp + 3 /* ':' + line break */;
                break;
            }
        }
        // Here we could check for prev == NULL and warn about help topics without declaration in txtHelpTopics
    }
    // Terminate the last topic found
    if(prev != NULL)
    {
        prev->length = &helptext_end - prev->data;
    }
}

/**
 * Extracts single arguments from the specified command line string and puts pointers into {@link arguments} array.
 * 
 * @param cmdline Pointer to the command line string
 * @return The number of arguments in the command line
 */
static uint32_t Console_GetArguments(char *cmdline)
{
    uint32_t argc = 0;
    char *tmp;
    
    if(cmdline == NULL)
    {
        return 0;
    }
    if(*cmdline != ' ')
    {
        argc = 1;
        arguments[0] = cmdline++;
    }
    
    // If we need support for quoted strings and escaping for spaces, this is the place to add it
    while((tmp = strchr(cmdline, ' ')) != NULL)
    {
        *tmp++ = 0;
        while(*tmp == ' ')
        {
            tmp++;
        }
        if(*tmp == 0)
        {
            break;
        }
        cmdline = tmp;
        arguments[argc] = tmp;
        argc++;
    }
    
    return argc;
}

/**
 * Looks for the specified command in an array and calls the corresponding function if found.
 * 
 * @param argc The number of arguments in the array
 * @param argv Array of arguments
 * @param cmds Array of possible commands
 * @param count The number of commands in the array
 * @return {@code 1} if a processing function was called, {@code 0} otherwise
 */
static uint8_t Console_CallProcessor(uint32_t argc, char **argv, const Console_Command *cmds, uint32_t count)
{
    for(uint32_t j = 0; j < count; j++)
    {
        if(strcmp(argv[0], cmds[j].cmd) == 0)
        {
            cmds[j].handler(argc, argv);
            return 1;
        }
    }
    return 0;
}

/**
 * Looks for the specified argument in an array and returns the corresponding structure if found.
 * 
 * @param arg Pointer to the argument
 * @param args Array of possible arguments
 * @param count The number of arguments in the array
 * @return Pointer to the argument structure, or {@code NULL} if none was found
 */
static const Console_Arg* Console_GetArg(const char *arg, const Console_Arg *args, uint32_t count)
{
    // If we need support for single letter arguments, this is the place to add it
    if(arg[0] != '-' || arg[1] != '-')
    {
        return NULL;
    }
    arg += 2;
    
    for(uint32_t j = 0; j < count; j++)
    {
        uint32_t len = max(strlen(args[j].arg), strchrnul(arg, '=') - arg);
        if(strncmp(arg, args[j].arg, len) == 0)
        {
            return &args[j];
        }
    }
    
    return NULL;
}

/**
 * Gets the value of an argument, that is the string after the equals sign.
 * 
 * @param arg The argument to get the value from
 * @return Pointer to the substring containing the value (can be empty), or {@code NULL} in case there is no value
 */
static const char* Console_GetArgValue(const char *arg)
{
    if(arg[0] != '-' || arg[0] != '-')
    {
        return NULL;
    }
    
    const char *val = strchr(arg + 2, '=');
    return (val != NULL ? val + 1 : NULL);
}

/**
 * Gets a flag value corresponding to the specified string.
 * 
 * The valid flags for <i>on</i> and <i>off</i> are taken from the string constants {@code txtOn} and {@code txtOff},
 * respectively.
 * 
 * @param str A string containing 'on', 'off' or some other, invalid value
 * @return a {@link Console_FlagValue}
 */
static Console_FlagValue Console_GetFlag(const char *str)
{
    if(str == NULL)
    {
        return CON_FLAG_INVALID;
    }
    else if(strcmp(str, txtOn) == 0)
    {
        return CON_FLAG_ON;
    }
    else if(strcmp(str, txtOff) == 0)
    {
        return CON_FLAG_OFF;
    }
    else
    {
        return CON_FLAG_INVALID;
    }
}

// Command processing functions -----------------------------------------------

/**
 * Calls the appropriate subcommand processing function for {@code board} commands.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Board(uint32_t argc, char **argv)
{
    static const Console_Command cmds[] = {
        { "set", Console_BoardSet },
        { "get", Console_BoardGet },
        { "info", Console_BoardInfo },
        { "calibrate", Console_BoardCalibrate },
        { "start", Console_BoardStart },
        { "stop", Console_BoardStop },
        { "status", Console_BoardStatus },
        { "temp", Console_BoardTemp },
        { "measure", Console_BoardMeasure },
        { "read", Console_BoardRead }
    };
    
    if(argc == 1)
    {
        VCP_SendLine(txtErrNoSubcommand);
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        VCP_SendLine(txtUnknownSubcommand);
        VCP_CommandFinish();
    }
}

static void Console_BoardCalibrate(uint32_t argc, char **argv)
{
    // Arguments: ohms
    Board_Error err;
    const char *end;
    uint32_t ohms;
    
    if(argc != 2)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    ohms = IntFromSiString(argv[1], &end);
    if(end == NULL)
    {
        VCP_SendString(txtInvalidValue);
        VCP_SendLine("ohms");
    }

    err = Board_Calibrate(ohms);
    switch(err)
    {
        case BOARD_OK:
            VCP_SendLine(txtOK);
            break;
        case BOARD_BUSY:
            VCP_SendLine(txtBoardBusy);
            break;
        case BOARD_ERROR:
            VCP_SendLine(txtWrongCalibValue);
            break;
    }
    
    VCP_CommandFinish();
}

/**
 * Processes the 'board get' command for all the defined options. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardGet(uint32_t argc, char **argv)
{
    // Arguments: option
    Console_ArgID option = CON_ARG_INVALID;
    char buf[16];
    
    if(argc != 2)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    for(uint32_t j = 0; j < NUMEL(argsBoardSet); j++)
    {
        if(strcmp(argsBoardSet[j].arg, argv[1]) == 0)
        {
            option = argsBoardSet[j].id;
            break;
        }
    }
    
    uint8_t autorange = Board_GetAutorange();
    switch(option)
    {
        case CON_ARG_SET_AUTORANGE:
            VCP_SendLine(autorange ? txtEnabled : txtDisabled);
            break;
            
        case CON_ARG_SET_ECHO:
            // Well, do you see what you're typing or not?
            VCP_SendLine(VCP_GetEcho() ? txtEnabled : txtDisabled);
            break;
            
        case CON_ARG_SET_FEEDBACK:
            if(autorange)
            {
                VCP_SendLine(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                AD5933_RangeSettings *range = Board_GetRangeSettings();
                SiStringFromInt(buf, NUMEL(buf), range->Feedback_Value);
                VCP_SendLine(buf);
            }
            break;
            
        case CON_ARG_SET_FORMAT:
            Convert_FormatSpecToString(buf, NUMEL(buf), format_spec);
            VCP_SendLine(buf);
            break;
            
        case CON_ARG_SET_GAIN:
            if(autorange)
            {
                VCP_SendLine(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                VCP_SendLine(Board_GetRangeSettings()->PGA_Gain == AD5933_GAIN_1 ? txtDisabled : txtEnabled);
            }
            break;
            
        case CON_ARG_SET_SETTL:
            snprintf(buf, NUMEL(buf), "%u", Board_GetSettlingCycles());
            VCP_SendLine(buf);
            break;
            
        case CON_ARG_SET_START:
            snprintf(buf, NUMEL(buf), "%lu", Board_GetStartFreq());
            VCP_SendLine(buf);
            break;
            
        case CON_ARG_SET_STEPS:
            snprintf(buf, NUMEL(buf), "%u", Board_GetFreqSteps());
            VCP_SendLine(buf);
            break;
            
        case CON_ARG_SET_STOP:
            snprintf(buf, NUMEL(buf), "%lu", Board_GetStopFreq());
            VCP_SendLine(buf);
            break;
            
        case CON_ARG_SET_VOLTAGE:
            if(autorange)
            {
                VCP_SendLine(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                AD5933_RangeSettings *range = Board_GetRangeSettings();
                uint16_t voltage = AD5933_GetVoltageFromRegister(range->Voltage_Range);
                snprintf(buf, NUMEL(buf), "%u", voltage / range->Attenuation);
                VCP_SendLine(buf);
            }
            break;
            
        default:
            if(strcmp(argv[1], "all") == 0)
            {
                // Send all relevant options, suitable for parsing
                VCP_SendString("start=");
                snprintf(buf, NUMEL(buf), "%lu", Board_GetStartFreq());
                VCP_SendLine(buf);
                
                VCP_SendString("steps=");
                snprintf(buf, NUMEL(buf), "%u", Board_GetFreqSteps());
                VCP_SendLine(buf);
                
                VCP_SendString("stop=");
                snprintf(buf, NUMEL(buf), "%lu", Board_GetStopFreq());
                VCP_SendLine(buf);
                
                VCP_SendString("settl=");
                snprintf(buf, NUMEL(buf), "%u", Board_GetSettlingCycles());
                VCP_SendLine(buf);
                
                VCP_SendString("autorange=");
                VCP_SendLine(autorange ? txtEnabled : txtDisabled);
                
                if(!autorange)
                {
                    AD5933_RangeSettings *range = Board_GetRangeSettings();
                    
                    VCP_SendString("gain=");
                    VCP_SendLine(range->PGA_Gain == AD5933_GAIN_5 ? txtEnabled : txtDisabled);
                    
                    VCP_SendString("voltage=");
                    uint16_t voltage = AD5933_GetVoltageFromRegister(range->Voltage_Range);
                    snprintf(buf, NUMEL(buf), "%u", voltage / range->Attenuation);
                    VCP_SendLine(buf);
                    
                    VCP_SendString("feedback=");
                    snprintf(buf, NUMEL(buf), "%lu", range->Feedback_Value);
                    VCP_SendLine(buf);
                }
            }
            else
            {
                VCP_SendString(txtUnknownOption);
                VCP_SendLine(argv[1]);
            }
            break;
    }
    
    VCP_CommandFinish();
}

/**
 * Processes the 'board info' command. This command finishes immediately.
 * 
 * The info that is printed is:
 *  + Measurement and AD5933 driver status
 *  + Available ports, frequencies and values of feedback resistors, voltage attenuations and calibration resistors
 *  + Available peripherals (EEPROM, SRAM, Ethernet, USB, Flash memory)
 *  + USB status info
 *  + Ethernet status info
 *  + Memory status information (EEPROM writes, SRAM size, Flash memory size)
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardInfo(uint32_t argc, char **argv __attribute__((unused)))
{
    const char *temp;
    char buf[16];
    
    if(argc != 1)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    // Board info and AD5933 status
    VCP_SendLine(THIS_IS_IMPY BOARD_VERSION BUILT_ON __DATE__ ", " __TIME__ ".\r\n");
    VCP_SendString(txtAdStatus);
    switch(AD5933_GetStatus())
    {
        case AD_IDLE:
        case AD_FINISH_CALIB:
        case AD_FINISH_TEMP:
        case AD_FINISH_IMPEDANCE:
            temp = txtAdStatusIdle;
            break;
        case AD_MEASURE_TEMP:
            temp = txtAdStatusTemp;
            break;
        case AD_MEASURE_IMPEDANCE:
        case AD_MEASURE_IMPEDANCE_AUTORANGE:
            temp = txtAdStatusMeasureImpedance;
            break;
        case AD_CALIBRATE:
            temp = txtAdStatusCalibrate;
            break;
        default:
            temp = txtAdStatusUnknown;
            break;
    }
    VCP_SendLine(temp);
    
    // Ports and values
    static const uint16_t attenuations[] = {
        AD5933_ATTENUATION_PORT_0, AD5933_ATTENUATION_PORT_1, AD5933_ATTENUATION_PORT_2, AD5933_ATTENUATION_PORT_3
    };
    static const uint32_t feedbackValues[] = {
        AD5933_FEEDBACK_PORT_0, AD5933_FEEDBACK_PORT_1, AD5933_FEEDBACK_PORT_2, AD5933_FEEDBACK_PORT_3,
        AD5933_FEEDBACK_PORT_4, AD5933_FEEDBACK_PORT_5, AD5933_FEEDBACK_PORT_6, AD5933_FEEDBACK_PORT_7
    };
    static const uint32_t calibrationValues[] = {
        CAL_PORT_10, CAL_PORT_11, CAL_PORT_12, CAL_PORT_13, CAL_PORT_14, CAL_PORT_15
    };
    
    VCP_SendString(txtPortsAvailable);
    snprintf(buf, NUMEL(buf), "(out) %u", PORT_MAX - PORT_MIN + 1);
    VCP_SendString(buf);
    VCP_SendString(" (");
    snprintf(buf, NUMEL(buf), "%u", PORT_MIN);
    VCP_SendString(buf);
    VCP_SendString("..");
    snprintf(buf, NUMEL(buf), "%u", PORT_MAX);
    VCP_SendString(buf);
    VCP_SendLine(")");
    
    VCP_SendString(txtFrequencyRange);
    VCP_SendString("(frq) ");
    SiStringFromInt(buf, NUMEL(buf), FREQ_MIN);
    VCP_SendString(buf);
    VCP_SendString("..");
    SiStringFromInt(buf, NUMEL(buf), FREQ_MAX);
    VCP_SendLine(buf);
    
    VCP_SendString(txtMaxNumIncrements);
    VCP_SendString("(inc) ");
    SiStringFromInt(buf, NUMEL(buf), AD5933_MAX_NUM_INCREMENTS);
    VCP_SendLine(buf);
    
    VCP_SendString(txtAttenuationsAvailable);
    VCP_SendString("(att) ");
    for(uint32_t j = 0; j < NUMEL(attenuations) && attenuations[j]; j++)
    {
        SiStringFromInt(buf, NUMEL(buf), attenuations[j]);
        VCP_SendString(buf);
        VCP_SendString(" ");
    }
    VCP_SendLine(NULL);
    
    VCP_SendString(txtFeedbackResistorValues);
    VCP_SendString("(rfb) ");
    for(uint32_t j = 0; j < NUMEL(feedbackValues) && feedbackValues[j]; j++)
    {
        SiStringFromInt(buf, NUMEL(buf), feedbackValues[j]);
        VCP_SendString(buf);
        VCP_SendString(" ");
    }
    VCP_SendLine(NULL);
    
    VCP_SendString(txtCalibrationValues);
    VCP_SendString("(rca) ");
    for(uint32_t j = 0; j < NUMEL(calibrationValues) && calibrationValues[j]; j++)
    {
        SiStringFromInt(buf, NUMEL(buf), calibrationValues[j]);
        VCP_SendString(buf);
        VCP_SendString(" ");
    }
    VCP_SendLine(NULL);
    
    // USB info
#if defined(BOARD_HAS_USBH) && BOARD_HAS_USBH == 1
    VCP_SendLine(NULL);
    // TODO print USB info
    VCP_SendLine(txtNotImplemented);
#else
    VCP_SendString(NULL);
    VCP_SendString(txtUSB);
    VCP_SendLine(txtNotInstalled);
#endif
    
    // Ethernet info
#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
    VCP_SendLine(NULL);
    // TODO print Ethernet info
    VCP_SendLine(txtNotImplemented);
#else
    VCP_SendLine(NULL);
    VCP_SendString(txtEthernet);
    VCP_SendLine(txtNotInstalled);
#endif
    
    // Memory info
#if (defined(BOARD_HAS_EEPROM) && BOARD_HAS_EEPROM == 1) || \
    (defined(BOARD_HAS_SRAM) && BOARD_HAS_SRAM == 1) || \
    (defined(BOARD_HAS_FLASH) && BOARD_HAS_FLASH == 1)
#define MEMORY_FLAG
#endif
    
#if defined(BOARD_HAS_EEPROM) && BOARD_HAS_EEPROM == 1
    VCP_SendLine(NULL);
    // TODO print EEPROM info
    VCP_SendLine(txtNotImplemented);
#elif defined(MEMORY_FLAG)
    VCP_SendLine(NULL);
    VCP_SendString(txtEEPROM);
    VCP_SendLine(txtNotInstalled);
#endif
    
#if defined(BOARD_HAS_SRAM) && BOARD_HAS_SRAM == 1
    VCP_SendLine(NULL);
    // TODO print SRAM info
    VCP_SendLine(txtNotImplemented);
#elif defined(MEMORY_FLAG)
    VCP_SendLine(NULL);
    VCP_SendString(txtSRAM);
    VCP_SendLine(txtNotInstalled);
#endif
    
#if defined(BOARD_HAS_FLASH) && BOARD_HAS_FLASH == 1
    VCP_SendLine(NULL);
    // TODO print Flash info
    VCP_SendLine(txtNotImplemented);
#elif defined(MEMORY_FLAG)
    VCP_SendLine(NULL);
    VCP_SendString(txtFlash);
    VCP_SendLine(txtNotInstalled);
#endif
    
#ifndef MEMORY_FLAG
    VCP_SendLine(txtNoMemory);
#endif
    
    VCP_CommandFinish();
}

/**
 * Processes the 'board measure' command. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardMeasure(uint32_t argc, char **argv)
{
    // Arguments: port, freq
    AD5933_ImpedancePolar result;
    Board_Error ok;
    uint32_t port;
    uint32_t freq;
    const char *end;
    char *buf;
    const uint32_t buflen = 64;
    
    if(argc != 3)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    port = IntFromSiString(argv[1], &end);
    // Uncomment if PORT_MIN is greater than 0
    if(end == NULL || /*port < PORT_MIN ||*/ port > PORT_MAX)
    {
        VCP_SendString(txtInvalidValue);
        VCP_SendLine("port");
        VCP_CommandFinish();
        return;
    }
    
    freq = IntFromSiString(argv[2], &end);
    if(end == NULL || freq < FREQ_MIN || freq > FREQ_MAX)
    {
        VCP_SendString(txtInvalidValue);
        VCP_SendLine("freq");
        VCP_CommandFinish();
        return;
    }
    
    ok = Board_MeasureSingleFrequency((uint8_t)port, freq, &result);
    
    switch(ok)
    {
        case BOARD_OK:
            VCP_SendString(txtImpedance);
            buf = malloc(buflen);
            if(buf != NULL)
            {
                snprintf(buf, buflen, "%g < %g", result.Magnitude, result.Angle);
                VCP_SendLine(buf);
                free(buf);
            }
            else
            {
                VCP_SendLine("PANIC!");
            }
            break;
            
        case BOARD_BUSY:
            VCP_SendLine(txtBoardBusy);
            break;
            
        case BOARD_ERROR:
            VCP_SendLine(txtNoGain);
            break;
    }
    
    VCP_CommandFinish();
}

static void Console_BoardRead(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "format", CON_ARG_READ_FORMAT, CON_STRING }
    };
    
    uint32_t format = format_spec;
    const AD5933_ImpedancePolar *data;
    uint32_t count;
    
    // In case data from the previous command has not been deallocated, do so now
    FreeBuffer(&board_read_data);
    
    // Check status, we also allow for incomplete data to be retrieved (status == AD_IDLE)
    if(AD5933_IsBusy())
    {
        VCP_SendLine(txtNoReadWhileBusy);
        VCP_CommandFinish();
        return;
    }
    
    // Process additional arguments, if any
    for(uint32_t j = 1; j < argc; j++)
    {
        const Console_Arg *arg = Console_GetArg(argv[j], args, NUMEL(args));
        const char *value = Console_GetArgValue(argv[j]);
        
        uint32_t intval = 0;
        
        if(arg == NULL)
        {
            // Complain about unknown arguments and bail out
            VCP_SendString(txtUnknownOption);
            VCP_SendLine(argv[j]);
            VCP_CommandFinish();
            return;
        }
        
        switch(arg->id)
        {
            case CON_ARG_READ_FORMAT:
                intval = Convert_FormatSpecFromString(value);
                if(intval != 0)
                {
                    format = intval;
                }
                else
                {
                    VCP_SendString(txtInvalidValue);
                    VCP_SendLine(arg->arg);
                    VCP_CommandFinish();
                    return;
                }
                break;
                
            default:
                // Should not happen, means that a defined argument has no switch case
                VCP_SendLine(txtNotImplemented);
                VCP_SendLine(arg->arg);
                VCP_CommandFinish();
                return;
        }
    }
    
    // Get and assemble data to be sent
    data = Board_GetDataPolar(&count);
    board_read_data = Convert_ConvertPolar(format, data, count);
    
    if(board_read_data.data != NULL)
    {
        VCP_SendBuffer((uint8_t *)board_read_data.data, board_read_data.size);
    }
    else
    {
        VCP_SendLine(txtOutOfMemory);
    }

    VCP_CommandFinish();
}

/**
 * Processes the 'board set' command for all the defined options. This command finishes immediately.
 * 
 * All options are processed in the order they appear on the command line, that means for options that are specified
 * multiple times, the last occurrence counts.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardSet(uint32_t argc, char **argv)
{
    if(argc == 1)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    for(uint32_t j = 1; j < argc; j++)
    {
        const Console_Arg *arg = Console_GetArg(argv[j], argsBoardSet, NUMEL(argsBoardSet));
        const char *value = Console_GetArgValue(argv[j]);
        
        Console_FlagValue flag = CON_FLAG_INVALID;
        uint32_t intval = 0;
        
        if(arg == NULL)
        {
            // Complain about unknown arguments but ignore otherwise
            VCP_SendString(txtUnknownOption);
            VCP_SendLine(argv[j]);
            continue;
        }

        const char *end;
        switch(arg->type)
        {
            case CON_FLAG:
                flag = Console_GetFlag(value);
                if(flag == CON_FLAG_INVALID)
                {
                    VCP_SendString(txtInvalidValue);
                    VCP_SendLine(arg->arg);
                    continue;
                }
                break;
                
            case CON_INT:
                intval = IntFromSiString(value, &end);
                if(end == NULL)
                {
                    VCP_SendString(txtInvalidValue);
                    VCP_SendLine(arg->arg);
                    continue;
                }
                break;
                
            case CON_STRING:
                // Handled by each argument
                break;
        }
        
        const uint8_t autorange = Board_GetAutorange();
        Board_Error ok = BOARD_OK;
        switch(arg->id)
        {
            case CON_ARG_SET_AUTORANGE:
                Board_SetAutorange(flag == CON_FLAG_ON);
                if(AD5933_IsBusy())
                {
                    VCP_SendLine(txtEffectiveNextSweep);
                }
                break;
                
            case CON_ARG_SET_ECHO:
                VCP_SetEcho(flag == CON_FLAG_ON);
                break;
                
            case CON_ARG_SET_FEEDBACK:
                if(autorange)
                {
                    VCP_SendString(txtSetOnlyWhenAutorangeDisabled);
                    VCP_SendLine(arg->arg);
                }
                else
                {
                    ok = Board_SetFeedback(intval);
                }
                break;
                
            case CON_ARG_SET_FORMAT:
                intval = Convert_FormatSpecFromString(value);
                if(intval != 0)
                {
                    format_spec = intval;
                }
                else
                {
                    ok = BOARD_ERROR;
                }
                break;
                
            case CON_ARG_SET_GAIN:
                if(autorange)
                {
                    VCP_SendString(txtSetOnlyWhenAutorangeDisabled);
                    VCP_SendLine(arg->arg);
                }
                else
                {
                    ok = Board_SetPGA(flag == CON_FLAG_ON);
                }
                break;
                
            case CON_ARG_SET_SETTL:
                intval = 1;
                char *mult = strchr(value, 'x');
                if(mult != NULL)
                {
                    *mult++ = 0;
                    intval = strtoul(mult, NULL, 10);
                }
                ok = Board_SetSettlingCycles(strtoul(value, NULL, 10), (uint8_t)intval);
                break;
                
            case CON_ARG_SET_START:
                ok = Board_SetStartFreq(intval);
                break;
                
            case CON_ARG_SET_STEPS:
                ok = Board_SetFreqSteps(intval);
                break;
                
            case CON_ARG_SET_STOP:
                ok = Board_SetStopFreq(intval);
                break;
                
            case CON_ARG_SET_VOLTAGE:
                if(autorange)
                {
                    VCP_SendString(txtSetOnlyWhenAutorangeDisabled);
                    VCP_SendLine(arg->arg);
                }
                else
                {
                    ok = Board_SetVoltageRange(intval);
                }
                break;
                
            default:
                // Should not happen, means that a defined argument has no switch case
                VCP_SendLine(txtNotImplemented);
                break;
        }
        
        if(ok == BOARD_BUSY)
        {
            VCP_SendString(txtSetOnlyWhenIdle);
            VCP_SendLine(arg->arg);
        }
        else if(ok == BOARD_ERROR)
        {
            VCP_SendString(txtInvalidValue);
            VCP_SendLine(arg->arg);
        }
    }
    
    VCP_CommandFinish();
}

/**
 * Processes the 'board start' command. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardStart(uint32_t argc, char **argv)
{
    // Arguments: port
    Board_Error ok;
    uint32_t port;
    const char *end;
    
    if(argc != 2)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    port = IntFromSiString(argv[1], &end);
    // Uncomment if PORT_MIN is greater than 0
    if(end == NULL || /*port < PORT_MIN ||*/ port > PORT_MAX)
    {
        VCP_SendString(txtInvalidValue);
        VCP_SendLine("port");
        VCP_CommandFinish();
        return;
    }

    ok = Board_StartSweep(port);
    switch(ok)
    {
        case BOARD_OK:
            VCP_SendLine(txtOK);
            break;
        case BOARD_BUSY:
            VCP_SendLine(txtBoardBusy);
            break;
        case BOARD_ERROR:
            VCP_SendLine(txtNoGain);
            break;
    }
    
    VCP_CommandFinish();
}

/**
 * Processes the 'board status' command. This command finished immediately.
 * 
 * Prints the current AD5933 driver status, whether autoranging is enabled and, if a sweep is running, the number of
 * data points already recorded.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardStatus(uint32_t argc, char **argv __attribute__((unused)))
{
    Board_Status status;
    char buf[16];
    
    if(argc != 1)
    {
        VCP_SendLine(txtErrArgNum);
        VCP_CommandFinish();
        return;
    }
    
    Board_GetStatus(&status);
    switch(status.ad_status)
    {
        case AD_MEASURE_IMPEDANCE:
        case AD_MEASURE_IMPEDANCE_AUTORANGE:
            // Point count
            VCP_SendString(txtAdStatusSweep);
            snprintf(buf, NUMEL(buf), "%u", status.point);
            VCP_SendString(buf);
            VCP_SendString(txtOf);
            snprintf(buf, NUMEL(buf), "%u", status.totalPoints);
            VCP_SendLine(buf);
            // Autorange status
            VCP_SendString(txtAutorangeStatus);
            VCP_SendString(status.autorange ? txtEnabled : txtDisabled);
            VCP_SendLine(".");
            break;
            
        case AD_IDLE:
        case AD_FINISH_TEMP:
        case AD_FINISH_CALIB:
            VCP_SendLine(txtAdStatusIdle);
            if(status.interrupted)
            {
                VCP_SendLine(txtLastInterrupted);
            }
            VCP_SendLine(status.validData ? txtValidData : txtNoData);
            VCP_SendLine(status.validGainFactor ? txtValidGain : txtNoGain);
            break;
            
        case AD_FINISH_IMPEDANCE:
            VCP_SendString(txtAdStatusFinishImpedance);
            snprintf(buf, NUMEL(buf), "%u", status.point);
            VCP_SendLine(buf);
            VCP_SendLine(status.validData ? txtValidData : txtNoData);
            VCP_SendLine(status.validGainFactor ? txtValidGain : txtNoGain);
            break;
            
        case AD_MEASURE_TEMP:
            VCP_SendLine(txtAdStatusTemp);
            break;
            
        case AD_CALIBRATE:
            VCP_SendLine(txtAdStatusCalibrate);
            break;
            
        default:
            // Should not happen, driver should be initialized by now.
            VCP_SendLine(txtAdStatusUnknown);
            break;
    }
    
    VCP_CommandFinish();
}

/**
 * Processes the 'board stop' command. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardStop(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc == 1)
    {
        AD5933_Status status = AD5933_GetStatus();
        if(status == AD_MEASURE_IMPEDANCE || status == AD_MEASURE_IMPEDANCE_AUTORANGE)
        {
            Board_StopSweep();
            VCP_SendLine(txtOK);
        }
        else
        {
            VCP_SendLine(txtAdStatusIdle);
        }
    }
    else
    {
        VCP_SendLine(txtErrArgNum);
    }
    VCP_CommandFinish();
}

/**
 * Processes the 'board temp' command. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardTemp(uint32_t argc, char **argv __attribute__((unused)))
{
    char buf[16];
    
    if(argc == 1)
    {
        float temp = Board_MeasureTemperature(TEMP_AD5933);
        snprintf(buf, NUMEL(buf), "%.1f %cC", temp, '\xB0' /* Degree symbol in ISO 8859-1 and -15 */); 
        VCP_SendLine(buf);
    }
    else
    {
        VCP_SendLine(txtErrArgNum);
    }
    
    VCP_CommandFinish();
}

/**
 * Calls the appropriate subcommand processing function for {@code eth} commands.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Eth(uint32_t argc __attribute__((unused)), char **argv __attribute__((unused)))
{
#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
    static const Console_Command cmds[] = {
        { "set", Console_EthSet },
        { "status", Console_EthStatus },
        { "enable", Console_EthEnable },
        { "disable", Console_EthDisable }
    };
    
    if(argc == 1)
    {
        VCP_SendLine(txtErrNoSubcommand);
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        VCP_SendLine(txtUnknownSubcommand);
        VCP_CommandFinish();
    }
#else
    VCP_SendString(txtEthernet);
    VCP_SendLine(txtNotInstalled);
    VCP_CommandFinish();
#endif
}

#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
static void Console_EthDisable(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_EthEnable(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_EthSet(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "dhcp", CON_ARG_SET_DHCP, CON_FLAG },
        { "ip", CON_ARG_SET_IP, CON_STRING }
    };

    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_EthStatus(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}
#endif

/**
 * Calls the appropriate subcommand processing function for {@code usb} commands.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Usb(uint32_t argc __attribute__((unused)), char **argv __attribute__((unused)))
{
#if defined(BOARD_HAS_USBH) && BOARD_HAS_USBH == 1
    static const Console_Command cmds[] = {
        { "status", Console_UsbStatus },
        { "info", Console_UsbInfo },
        { "eject", Console_UsbEject },
        { "write", Console_UsbWrite },
        { "ls", Console_UsbLs }
    };
    
    if(argc == 1)
    {
        VCP_SendLine(txtErrNoSubcommand);
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        VCP_SendLine(txtUnknownSubcommand);
        VCP_CommandFinish();
    }
#else
    VCP_SendString(txtUSB);
    VCP_SendLine(txtNotInstalled);
    VCP_CommandFinish();
#endif
}

#if defined(BOARD_HAS_USBH) && BOARD_HAS_USBH == 1
static void Console_UsbEject(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbInfo(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbLs(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbStatus(uint32_t argc, char **argv)
{
    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbWrite(uint32_t argc, char **argv)
{
    // Arguments: file

    VCP_SendLine(txtNotImplemented);
    VCP_CommandFinish();
}
#endif

/**
 * Processes the {@code help} command.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Help(uint32_t argc, char **argv)
{
    Console_HelpEntry *topic = NULL;
    
    switch(argc)
    {
        case 1:
            // Command without arguments, print usage
            VCP_SendBuffer((uint8_t *)strHelp.data, strHelp.length);
            break;
        case 2:
            // Command with topic, look for help message
            for(uint32_t j = 0; j < NUMEL(txtHelpTopics); j++)
            {
                if(strcmp(argv[1], txtHelpTopics[j].cmd) == 0)
                {
                    topic = &txtHelpTopics[j];
                    break;
                }
            }
            if(topic != NULL)
            {
                VCP_SendBuffer((const uint8_t *)topic->text.data, topic->text.length);
            }
            else
            {
                VCP_SendLine(txtUnknownTopic);
            }
            break;
        default:
            // Wrong number of arguments, print error message
            VCP_SendLine(txtErrArgNum);
            break;
    }
    
    VCP_CommandFinish();
}

/**
 * Processes the {@code debug} command.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Debug(uint32_t argc, char **argv __attribute__((unused)))
{
    static Buffer noleak = { NULL, 0 };
    
    if(argc == 1)
    {
        VCP_SendLine("send, echo, char-from-flag, printf-float, malloc, leak, read, usb-paksize, heap,");
        VCP_SendLine("tim, mux, ad-i2c");
        VCP_CommandFinish();
        return;
    }
    
#ifdef DEBUG
    if(strcmp(argv[1], "send") == 0)
    {
        // Send some strings to test how the VCP copes with multiple calls in close succession
        VCP_SendString("this is a test string\r\n");
        VCP_SendString("second SendString call with a string that is longer than before.\r\n");
        VCP_SendString("short line\r\n");
    }
    else if(strcmp(argv[1], "echo") == 0)
    {
        // Echo back all received arguments
        for(uint32_t j = 2; j < argc; j++)
        {
            VCP_SendLine(argv[j]);
        }
    }
    else if(strcmp(argv[1], "char-from-flag") == 0)
    {
        // Test what functions to use for getting the set bit in an integer
        char buf[16];
        
        VCP_SendLine("expected bits clz ffs CHAR_FROM_FORMAT_FLAG");
        for(uint32_t j = 0; j < 32; j++)
        {
            char a = 'A' + j;
            uint32_t flag = (uint32_t)1 << j;
            uint32_t bits_clz = (uint32_t)__builtin_ctzl(flag);
            uint32_t bits_ffs = (uint32_t)(ffs(flag) - 1);
            char c = CHAR_FROM_FORMAT_FLAG(flag);
            
            snprintf(buf, NUMEL(buf), "%c %lu %lu %lu %c", a, j, bits_clz, bits_ffs, c);
            VCP_SendLine(buf);
        }
    }
    else if(strcmp(argv[1], "printf-float") == 0)
    {
        // Test the number format of floating point numbers
        char *buf;
        uint32_t size = 100;
        buf = malloc(size);
        if(buf != NULL)
        {
            float f1 = 1.5378f;
            float f2 = atan2f(0.5f, 0.5f);
            snprintf(buf, size, "%g < %g", f1, f2);
            VCP_SendLine(buf);
            for(uint32_t j = 0; j < 10; j++)
            {
                f1 *= 10.0f;
                f2 /= 10.0f;
                snprintf(buf, size, "%g < %g", f1, f2);
                VCP_SendLine(buf);
            }
            free(buf);
        }
        else
        {
            VCP_SendLine("Pointer was NULL.");
        }
    }
    else if(strcmp(argv[1], "malloc") == 0)
    {
        // Test how much memory can be allocated and whether an out of memory error is handled by malloc
        const uint32_t len = 81;        // String buffer length
        const uint32_t length = 400;    // Length of buffer pointer array
        const uint32_t size = 500;      // Size of allocated buffers
        char *buf;
        char **allocs;
        uint32_t buffers = 0;           // Actual number of buffers allocated
        
        buf = malloc(len);
        allocs = calloc(length, sizeof(char*));
        if(buf == NULL || allocs == NULL)
        {
            VCP_SendLine("Could not allocate string buffer and pointer buffer.");
        }
        else
        {
            VCP_SendLine("Trying to allocate some buffers...");
            VCP_Flush();
            for(uint32_t j = 0; j < length; j++)
            {
                allocs[j] = malloc(size);
                if(allocs[j] == NULL)
                {
                    break;
                }
                if(j % 10 == 0)
                {
                    snprintf(buf, len, "%lu ", j);
                    VCP_SendString(buf);
                    VCP_Flush();
                }
                buffers++;
            }
            snprintf(buf, len, "\r\nCould allocate %lu buffers with %lu bytes each.", buffers, size);
            VCP_SendLine(buf);
            snprintf(buf, len, "For everyone too lazy to use their brain, that's %lu bytes in total.", buffers * size);
            VCP_SendLine(buf);
            VCP_SendLine("Now freeing...");
            VCP_Flush();
            for(uint32_t j = 0; j < buffers; j++)
            {
                free(allocs[j]);
            }
            VCP_SendLine("Finished.");
        }
        free(buf);
        free(allocs);
    }
    else if(strcmp(argv[1], "leak") == 0)
    {
        // Leak a specified amount of memory
        if(argc == 3)
        {
            const char *end;
            uint32_t bytes = IntFromSiString(argv[2], &end);
            if(end == NULL)
            {
                VCP_SendLine("Don't be silly.");
            }
            else if(malloc(bytes) != NULL)
            {
                VCP_SendLine("Allocation successful, memory leaked.");
            }
            else
            {
                VCP_SendLine("Allocation failed.");
            }
        }
        else
        {
            VCP_SendLine(txtErrArgNum);
        }
    }
    else if(strcmp(argv[1], "read") == 0)
    {
        // Generate some impedance data and send it with an optionally specified format
        uint32_t format = format_spec;
        static const AD5933_ImpedancePolar data[] = {
            { 5000, 1.56789e4f, 0.321f },   // 1.4878e4 + 4.94694e3 j
            { 6000, 1.5699e4f, 0.33f },     // 1.48519e4 + 5.08715e3 j
            { 7000, 1.7e4f, 0.34f },        // 1.60268e4 + 5.66928e3 j
            { 8000, 1.6543e4f, 0.352f },    // 1.55287e4 + 5.70363e3 j
            { 9000, 1.5432109e4f, 0.361f }  // 1.44374e4 + 5.45077e3 j
        };
        
        FreeBuffer(&noleak);
        
        if(argc == 3)
        {
            if(strcmp(argv[2], "free") == 0)
            {
                VCP_CommandFinish();
                return;
            }
            format = Convert_FormatSpecFromString(argv[2]);
        }
        
        if(format)
        {
            noleak = Convert_ConvertPolar(format, data, NUMEL(data));
            if(noleak.data != NULL)
            {
                VCP_SendBuffer(noleak.data, noleak.size);
            }
            else
            {
                VCP_SendLine(txtOutOfMemory);
            }
        }
        else
        {
            VCP_SendLine("Invalid format specifier.");
        }
    }
    else if(strcmp(argv[1], "usb-paksize") == 0)
    {
        // Test VCP with multiples of USB packet size
        static const char *txtTest = "This is a string with 64 bytes of data to be sent over the VCP..";
        static const char *txtTest2 = "This is an even longer text that should hold 128 bytes, which is exactly two packet sizes, to test the failure with two packets.";
        VCP_SendBuffer((uint8_t *)(argc == 2 ? txtTest : txtTest2), strlen(argc == 2 ? txtTest : txtTest2));
    }
    else if(strcmp(argv[1], "heap") == 0)
    {
        // Print information about the heap
        extern char _Heap_Begin;
        extern char _Heap_Limit;
        char buf[80];
        void *brk = sbrk(0);
        
        snprintf(buf, NUMEL(buf), "Heap begin: %p, Heap limit: %p (size = %lu)\r\n",
                &_Heap_Begin, &_Heap_Limit, (uint32_t)(&_Heap_Limit - &_Heap_Begin));
        VCP_SendString(buf);
        snprintf(buf, NUMEL(buf), "Current break: %p\r\nFree bytes: %lu\r\n",
                brk, (uint32_t)((void *)&_Heap_Limit - brk));
        VCP_SendString(buf);
    }
    else if(strcmp(argv[1], "tim") == 0)
    {
        // Enable or disable TIM10 OC output
        if(argc != 3)
        {
            VCP_SendLine(txtErrArgNum);
        }
        else
        {
            Console_FlagValue flag = Console_GetFlag(argv[2]);
            switch (flag) {
                case CON_FLAG_ON:
                    HAL_TIM_OC_Start(&htim10, TIM_CHANNEL_1);
                    break;
                case CON_FLAG_OFF:
                    HAL_TIM_OC_Stop(&htim10, TIM_CHANNEL_1);
                    break;
                default:
                    // Ignore
                    break;
            }
        }
    }
    else if(strcmp(argv[1], "mux") == 0)
    {
        // Set output mux port, or disable
        if(argc != 3)
        {
            VCP_SendLine(txtErrArgNum);
        }
        else
        {
            const char *end;
            uint8_t port = IntFromSiString(argv[2], &end);
            
            if(strcmp(argv[2], "off") == 0)
            {
                port = AD725_CHIP_ENABLE_NOT;
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
                HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
                VCP_SendLine("Switched off.");
            }
            else if(end != NULL && port <= 15)
            {
                port &= AD725_MASK_PORT;
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
                HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
                VCP_SendLine("Port set.");
            }
            else
            {
                VCP_SendLine("Unknown port.");
            }
        }
    }
    else if(strcmp(argv[1], "ad-i2c") == 0)
    {
        // Test AD5933 I2C communication by writing to a register and reading the value back.
        extern I2C_HandleTypeDef hi2c1;
        const char* const timeout = "I2C Timeout: ";
        const char* const ackfail = "ACK failure: ";
        const uint8_t ad = ((uint8_t)(0x0D << 1));
        uint8_t addr = 0x82;
        const uint8_t cmdSet = 0xB0;
        const uint8_t cmdWrite = 0xA0;
        const uint8_t cmdRead = 0xA1;
        const uint32_t value = 0xBAC0F5;
        const uint8_t bytes = 3;
        uint8_t data[5];
        uint32_t read;
        char buf[16];
        HAL_StatusTypeDef ret;
        
        ret = HAL_I2C_Mem_Write(&hi2c1, ad, cmdSet, 1, &addr, 1, 200);      // Set address
        if(ret == HAL_TIMEOUT || ret == HAL_ERROR)
        {
            VCP_SendString(ret == HAL_TIMEOUT ? timeout : ackfail);
            VCP_SendLine("set write address");
        }
        
        data[0] = cmdWrite;
        data[1] = bytes;
        data[2] = (uint8_t)((value >> 16) & 0xFF);
        data[3] = (uint8_t)((value >> 8) & 0xFF);
        data[4] = (uint8_t)(value & 0xFF);
        ret = HAL_I2C_Master_Transmit(&hi2c1, ad, data, sizeof(data), 200);
        if(ret == HAL_TIMEOUT || ret == HAL_ERROR)
        {
            VCP_SendString(ret == HAL_TIMEOUT ? timeout : ackfail);
            VCP_SendLine("write data");
        }
        
        VCP_SendString("Data written: ");
        snprintf(buf, NUMEL(buf), "%.6lx", value);
        VCP_SendLine(buf);

        ret = HAL_I2C_Mem_Write(&hi2c1, ad, cmdSet, 1, &addr, 1, 200);      // Set address
        if(ret == HAL_TIMEOUT || ret == HAL_ERROR)
        {
            VCP_SendString(ret == HAL_TIMEOUT ? timeout : ackfail);
            VCP_SendLine("set read address");
        }
        
        *((uint32_t *)data) = 0;
        ret = HAL_I2C_Mem_Read(
                &hi2c1, ad, ((uint16_t)cmdRead << 8) | bytes, I2C_MEMADD_SIZE_16BIT, &data[1], bytes, 200);
        if(ret == HAL_TIMEOUT || ret == HAL_ERROR)
        {
            VCP_SendString(ret == HAL_TIMEOUT ? timeout : ackfail);
            VCP_SendLine("read data");
        }
        
#ifdef __ARMEB__
        read = *((uint32_t *)data);
#else
        read = __REV(*((uint32_t *)data));
#endif
        
        VCP_SendString("Data read:    ");
        snprintf(buf, NUMEL(buf), "%.6lx", read);
        VCP_SendLine(buf);
    }
    else
    {
        VCP_SendLine(txtUnknownSubcommand);
    }
#else
    VCP_SendLine("This is a release build, no debug code compiled in.");
#endif

    VCP_CommandFinish();
}

// Exported functions ---------------------------------------------------------

/**
 * This function sets up the console and should be called before any other console functions.
 */
void Console_Init(void)
{
    Console_InitHelp();
    
    format_spec = FORMAT_FLAG_ASCII | 
            FORMAT_FLAG_POLAR | 
            FORMAT_FLAG_FLOAT | 
            FORMAT_FLAG_HEADER | 
            FORMAT_FLAG_SPACE;
}

/**
 * Processes the specified 0-terminated string to extract commands and arguments and calls the appropriate functions.
 * 
 * @param str Pointer to a command line string
 */
void Console_ProcessLine(char *str)
{
    uint32_t argc;
    
    if(str == NULL)
    {
        // Should not happen, VCP does not call us with NULL so do nothing (not even finish command)
        return;
    }
    
    argc = Console_GetArguments(str);
    
    if(argc == 0)
    {
        // Command line is empty, do nothing
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc, arguments, commands, NUMEL(commands)))
    {
        VCP_SendLine(txtUnknownCommand);
        VCP_CommandFinish();
    }
}

// ----------------------------------------------------------------------------
