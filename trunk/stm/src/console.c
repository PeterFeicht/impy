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
    char *cmd;          //!< The command the help text is for
    char *text;         //!< The help text
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
    char *arg;              //!< Name of this argument
    Console_ArgID id;       //!< ID of this argument
    Console_ArgType type;   //!< Type of this argument
} Console_Arg;

typedef struct
{
    char *cmd;                      //!< Command name
    Console_CommandFunc handler;    //!< Pointer to the function that processes this command
} Console_Command;

// Private function prototypes ------------------------------------------------
static void Console_InitHelp(void);
static uint8_t Console_AddHelpTopic(char *help);
static uint32_t Console_GetArguments(char *cmdline);
static uint8_t Console_CallProcessor(uint32_t argc, char **argv, const Console_Command *cmds, uint32_t count);
static const Console_Arg* Console_GetArg(char *arg, const Console_Arg *args, uint32_t count);
static char* Console_GetArgValue(char *arg);
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
static void Console_EthDisable(uint32_t argc, char **argv);
static void Console_EthEnable(uint32_t argc, char **argv);
static void Console_EthSet(uint32_t argc, char **argv);
static void Console_EthStatus(uint32_t argc, char **argv);
static void Console_Usb(uint32_t argc, char **argv);
static void Console_UsbEject(uint32_t argc, char **argv);
static void Console_UsbInfo(uint32_t argc, char **argv);
static void Console_UsbLs(uint32_t argc, char **argv);
static void Console_UsbStatus(uint32_t argc, char **argv);
static void Console_UsbWrite(uint32_t argc, char **argv);
static void Console_Help(uint32_t argc, char **argv);
static void Console_Debug(uint32_t argc, char **argv);

// Private variables ----------------------------------------------------------
static char *arguments[CON_MAX_ARGUMENTS];
static uint32_t format_spec;
static Buffer board_read_data = {
    .data = NULL,
    .size = 0
};

// Console definition
extern char helptext_start;
static char *txtHelp;
// All help topics from command-line.txt need to be added here
static Console_HelpEntry txtHelpTopics[] = {
    { "eth", NULL },
    { "usb", NULL },
    { "format", NULL },
    { "settl", NULL },
    { "voltage", NULL },
    { "autorange", NULL },
    { "calibrate", NULL },
    { "echo", NULL }
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
 * Cuts the help string after the usage message and fills the help topics array.
 */
static void Console_InitHelp(void)
{
    uint8_t dashes = 0;
    uint8_t newline = 0;
    char *help;
    
    txtHelp = &helptext_start;
    
    // Find end of usage message (terminated by four dashes)
    for(help = txtHelp; *help; help++)
    {
        if(*help == '-')
        {
            dashes++;
            
            if(dashes == 4)
            {
                *(help - 3) = 0;
                help++;
                break;
            }
        }
        else
        {
            dashes = 0;
        }
    }
    
    // Look for specific help messages
    while(*help)
    {
        if(*help == '\n' || *help == '\r')
        {
            newline = 1;
            help++;
        }
        
        if(newline && *help == 'h')
        {
            // TODO avoid writing to help text so it can be moved from RAM to Flash memory
            if(Console_AddHelpTopic(help) == 0)
            {
                *help = 0;
            }
        }
        help++;
    }
}

/**
 * Adds a new help topic to the list, if present.
 * 
 * Help topics are designated by {@code help TOPIC:} at the beginning of a line in the {@code command-line.txt} file.
 * 
 * @param help Pointer to a new line in the help text that may contain a help topic
 * @return {@code 0} if a help topic was added, nonzero value on error
 */
static uint8_t Console_AddHelpTopic(char *help)
{
    // This text is at the beginning of a help topic start line
    static const char txt[] = "help ";
    char *cmd;
    char *tmp;
    
    for(uint32_t j = 0; txt[j] != 0; j++)
    {
        if(*help == 0 || *help != txt[j])
        {
            return 1;
        }
        help++;
    }
    // What comes after the 'help ' text is the command name
    cmd = help;
    
    tmp = strchr(help, ':');
    if(tmp == NULL)
    {
        return 2;
    }
    // Terminate command name for comparison
    *tmp = 0;
    for(help = tmp + 1; *help == '\r' || *help == '\n'; help++);
    if(*help == 0)
    {
        return 3;
    }
    
    // Look for command in topic list
    for(uint32_t j = 0; j < NUMEL(txtHelpTopics); j++)
    {
        if(strcmp(cmd, txtHelpTopics[j].cmd) == 0)
        {
            txtHelpTopics[j].text = help;
            *tmp = '.';
            return 0;
        }
    }
    
    // Should not happen, means that command-line.txt contains help for an unknown command
    *tmp = '!';
    return 4;
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
static const Console_Arg* Console_GetArg(char *arg, const Console_Arg *args, uint32_t count)
{
    // If we need support for single letter arguments, this is the place to add it
    char term;
    char *end;
    const Console_Arg *ret = NULL;
    
    if(arg[0] != '-' || arg[0] != '-')
    {
        return NULL;
    }
    
    arg += 2;
    end = arg;
    while(*end != 0 && *end != '=')
    {
        end++;
    }
    term = *end;
    *end = 0;
    
    for(uint32_t j = 0; j < count; j++)
    {
        if(strcmp(arg, args[j].arg) == 0)
        {
            ret = &args[j];
            break;
        }
    }
    
    *end = term;
    return ret;
}

/**
 * Gets the value of an argument, that is the string after the equals sign.
 * 
 * @param arg The argument to get the value from
 * @return Pointer to the substring containing the value, or {@code NULL} in case of an error
 */
static char* Console_GetArgValue(char *arg)
{
    char *val;
    if(arg[0] != '-' || arg[0] != '-')
    {
        return NULL;
    }
    
    val = strchr(arg + 2, '=');
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
            VCP_SendLine(txtUnknownOption);
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
        case AD_FINISH:
            temp = txtAdStatusIdle;
            break;
        case AD_MEASURE_TEMP:
            temp = txtAdStatusTemp;
            break;
        case AD_MEASURE_IMPEDANCE:
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
    VCP_SendString("\r\nUSB");
    VCP_SendLine(txtNotInstalled);
#endif
    
    // Ethernet info
#if defined(BOARD_HAS_ETHERNET) && BOARD_HAS_ETHERNET == 1
    VCP_SendLine(NULL);
    // TODO print Ethernet info
    VCP_SendLine(txtNotImplemented);
#else
    VCP_SendString("\r\nEthernet");
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
    VCP_SendString("\r\nEEPROM");
    VCP_SendLine(txtNotInstalled);
#endif
    
#if defined(BOARD_HAS_SRAM) && BOARD_HAS_SRAM == 1
    VCP_SendLine(NULL);
    // TODO print SRAM info
    VCP_SendLine(txtNotImplemented);
#elif defined(MEMORY_FLAG)
    VCP_SendString("\r\nSRAM");
    VCP_SendLine(txtNotInstalled);
#endif
    
#if defined(BOARD_HAS_FLASH) && BOARD_HAS_FLASH == 1
    VCP_SendLine(NULL);
    // TODO print Flash info
    VCP_SendLine(txtNotImplemented);
#elif defined(MEMORY_FLAG)
    VCP_SendString("\r\nFlash");
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
            
        default:
            // Should not happen
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
    AD5933_Status status = AD5933_GetStatus();
    if(status != AD_FINISH && status != AD_IDLE)
    {
        VCP_SendLine(txtNoReadWhileBusy);
        VCP_CommandFinish();
        return;
    }
    
    // Process additional arguments, if any
    for(uint32_t j = 1; j < argc; j++)
    {
        const Console_Arg *arg = Console_GetArg(argv[j], args, NUMEL(args));
        char *value = Console_GetArgValue(argv[j]);
        
        uint32_t intval = 0;
        
        if(arg == NULL)
        {
            // Complain about unknown arguments and bail out
            VCP_SendLine(txtUnknownOption);
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
        char *value = Console_GetArgValue(argv[j]);
        
        Console_FlagValue flag = CON_FLAG_INVALID;
        uint32_t intval = 0;
        
        if(arg == NULL)
        {
            // Complain about unknown arguments but ignore otherwise
            VCP_SendLine(txtUnknownOption);
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
                AD5933_Status status = AD5933_GetStatus();
                if(status != AD_FINISH && status != AD_IDLE)
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

static void Console_BoardStart(uint32_t argc, char **argv)
{
    // Arguments: port

    // TODO implement 'board start'
    VCP_SendLine(txtNotImplemented);
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
            VCP_SendLine(txtAdStatusIdle);
            if(status.interrupted)
            {
                VCP_SendLine(txtLastInterrupted);
            }
            break;
            
        case AD_FINISH:
            VCP_SendString(txtAdStatusFinish);
            snprintf(buf, NUMEL(buf), "%u", status.point);
            VCP_SendLine(buf);
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
        if(AD5933_GetStatus() == AD_MEASURE_IMPEDANCE)
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
        snprintf(buf, NUMEL(buf), "%.1f °C", temp);
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
static void Console_Eth(uint32_t argc, char **argv)
{
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
}

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

/**
 * Calls the appropriate subcommand processing function for {@code usb} commands.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Usb(uint32_t argc, char **argv)
{
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
}

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

/**
 * Processes the {@code help} command.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Help(uint32_t argc, char **argv)
{
    uint32_t j;
    
    switch(argc)
    {
        case 1:
            // Command without arguments, print usage
            VCP_SendBuffer((uint8_t *)txtHelp, strlen(txtHelp));
            break;
        case 2:
            // Command with topic, look for help message
            for(j = 0; j < NUMEL(txtHelpTopics); j++)
            {
                if(strcmp(argv[1], txtHelpTopics[j].cmd) == 0)
                {
                    VCP_SendBuffer((uint8_t *)txtHelpTopics[j].text, strlen(txtHelpTopics[j].text));
                    break;
                }
            }
            if(j == NUMEL(txtHelpTopics))
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
        VCP_SendLine("tim");
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
