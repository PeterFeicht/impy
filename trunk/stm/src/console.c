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
    CON_ARG_READ_RAW,
    CON_ARG_READ_GAIN,
    // board set/get
    CON_ARG_SET_AUTORANGE,
    CON_ARG_SET_AVG,
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
    CON_ARG_SET_IP,
    // setup
    CON_CMD_SETUP_ATTENUATIONS,
    CON_CMD_SETUP_FEEDBACK,
    CON_CMD_SETUP_CALIBRATION,
    CON_CMD_SETUP_COUPL,
    CON_CMD_SETUP_SRAM,
    CON_CMD_SETUP_FLASH,
    CON_CMD_SETUP_ETH,
    CON_CMD_SETUP_USBH
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
__STATIC_INLINE void Console_Flush(void);
// Command line processors
static void Console_Board(uint32_t argc, char **argv);
static void Console_BoardCalibrate(uint32_t argc, char **argv);
static void Console_BoardGet(uint32_t argc, char **argv);
static void Console_BoardInfo(uint32_t argc, char **argv);
static void Console_BoardMeasure(uint32_t argc, char **argv);
static void Console_BoardRead(uint32_t argc, char **argv);
static void Console_BoardSet(uint32_t argc, char **argv);
static void Console_BoardStandby(uint32_t argc, char **argv);
static void Console_BoardStart(uint32_t argc, char **argv);
static void Console_BoardStatus(uint32_t argc, char **argv);
static void Console_BoardStop(uint32_t argc, char **argv);
static void Console_BoardTemp(uint32_t argc, char **argv);
static void Console_Eth(uint32_t argc, char **argv);
static void Console_Usb(uint32_t argc, char **argv);
static void Console_Help(uint32_t argc, char **argv);
static void Console_Setup(uint32_t argc, char **argv);
static void Console_Debug(uint32_t argc, char **argv);
static void Console_EthDisable(uint32_t argc, char **argv);
static void Console_EthEnable(uint32_t argc, char **argv);
static void Console_EthSet(uint32_t argc, char **argv);
static void Console_EthStatus(uint32_t argc, char **argv);
static void Console_UsbEject(uint32_t argc, char **argv);
static void Console_UsbInfo(uint32_t argc, char **argv);
static void Console_UsbLs(uint32_t argc, char **argv);
static void Console_UsbStatus(uint32_t argc, char **argv);
static void Console_UsbWrite(uint32_t argc, char **argv);

// Private variables ----------------------------------------------------------
static char *arguments[CON_MAX_ARGUMENTS];
static uint32_t format_spec;
static Buffer board_read_data = {
    .data = NULL,
    .size = 0
};
static Console_Interface *interface = NULL;

// Console definition
static String strHelp = {
    .data = NULL,
    .length = 0
};
// All help topics from command-line.txt need to be added here
#define TOPIC(X)    { X, { NULL, 0 } }
static Console_HelpEntry txtHelpTopics[] = {
    TOPIC("options"),
    TOPIC("eth"),
    TOPIC("usb"),
    TOPIC("format"),
    TOPIC("settl"),
    TOPIC("voltage"),
    TOPIC("autorange"),
    TOPIC("calibrate"),
    TOPIC("ranges"),
    TOPIC("echo"),
    TOPIC("setup"),
};
// Those are the top level commands, subcommands are called from their respective processing functions
static const Console_Command commands[] = {
    { "board", Console_Board },
    { "eth", Console_Eth },
    { "usb", Console_Usb },
    { "help", Console_Help },
    { "setup", Console_Setup },
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
    { "avg", CON_ARG_SET_AVG, CON_INT },
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
    // Defined in helptext.asm
    extern const char helptext_start;
    extern const char helptext_end;
    
    // This text is at the beginning of a help topic start line
    static const char *topic = "\r\nhelp ";
    uint32_t topicLen = strlen(topic);
    const char *help;
    String *prev = NULL;
    
    if(strHelp.data != NULL)
    {
        // If Console_Init is called repeatedly, just do nothing.
        return;
    }
    
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

/**
 * Inline wrapper for {@code interface->Flush()} to allow NULL pointer for back ends that don't support it.
 */
__STATIC_INLINE void Console_Flush(void)
{
    if(interface->Flush != NULL)
        interface->Flush();
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
        { "standby", Console_BoardStandby },
        { "read", Console_BoardRead }
    };
    
    if(argc == 1)
    {
        interface->SendLine(txtErrNoSubcommand);
        interface->CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        interface->SendLine(txtUnknownSubcommand);
        interface->CommandFinish();
    }
}

/**
 * Processes the 'board calibrate' command. This command finishes when {@link Console_CalibrateCallback} is called.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardCalibrate(uint32_t argc, char **argv)
{
    // Arguments: ohms
    Board_Error err;
    const char *end;
    uint32_t ohms;
    
    if(argc != 2)
    {
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
        return;
    }
    
    ohms = IntFromSiString(argv[1], &end);
    if(end == NULL)
    {
        interface->SendString(txtInvalidValue);
        interface->SendLine("ohms");
    }

    err = Board_Calibrate(ohms);
    switch(err)
    {
        case BOARD_OK:
            break;
        case BOARD_BUSY:
            interface->SendLine(txtBoardBusy);
            interface->CommandFinish();
            break;
        case BOARD_ERROR:
            interface->SendLine(txtWrongCalibValue);
            interface->CommandFinish();
            break;
    }
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
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
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
            interface->SendLine(autorange ? txtEnabled : txtDisabled);
            break;
            
        case CON_ARG_SET_AVG:
            snprintf(buf, NUMEL(buf), "%u", Board_GetAverages());
            interface->SendLine(buf);
            break;
            
        case CON_ARG_SET_ECHO:
            // Well, do you see what you're typing or not?
            interface->SendLine(interface->GetEcho() ? txtEnabled : txtDisabled);
            break;
            
        case CON_ARG_SET_FEEDBACK:
            if(autorange)
            {
                interface->SendLine(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                const AD5933_RangeSettings *range = Board_GetRangeSettings();
                SiStringFromInt(buf, NUMEL(buf), range->Feedback_Value);
                interface->SendLine(buf);
            }
            break;
            
        case CON_ARG_SET_FORMAT:
            Convert_FormatSpecToString(buf, NUMEL(buf), format_spec);
            interface->SendLine(buf);
            break;
            
        case CON_ARG_SET_GAIN:
            if(autorange)
            {
                interface->SendLine(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                interface->SendLine(Board_GetRangeSettings()->PGA_Gain == AD5933_GAIN_1 ? txtDisabled : txtEnabled);
            }
            break;
            
        case CON_ARG_SET_SETTL:
            snprintf(buf, NUMEL(buf), "%u", Board_GetSettlingCycles());
            interface->SendLine(buf);
            break;
            
        case CON_ARG_SET_START:
            snprintf(buf, NUMEL(buf), "%lu", Board_GetStartFreq());
            interface->SendLine(buf);
            break;
            
        case CON_ARG_SET_STEPS:
            snprintf(buf, NUMEL(buf), "%u", Board_GetFreqSteps());
            interface->SendLine(buf);
            break;
            
        case CON_ARG_SET_STOP:
            snprintf(buf, NUMEL(buf), "%lu", Board_GetStopFreq());
            interface->SendLine(buf);
            break;
            
        case CON_ARG_SET_VOLTAGE:
            if(autorange)
            {
                interface->SendLine(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                const AD5933_RangeSettings *range = Board_GetRangeSettings();
                uint16_t voltage = AD5933_GetVoltageFromRegister(range->Voltage_Range);
                snprintf(buf, NUMEL(buf), "%u", voltage / range->Attenuation);
                interface->SendLine(buf);
            }
            break;
            
        default:
            if(strcmp(argv[1], "all") == 0)
            {
                // Send all relevant options, suitable for parsing
                interface->SendString("start=");
                snprintf(buf, NUMEL(buf), "%lu", Board_GetStartFreq());
                interface->SendLine(buf);
                
                interface->SendString("steps=");
                snprintf(buf, NUMEL(buf), "%u", Board_GetFreqSteps());
                interface->SendLine(buf);
                
                interface->SendString("stop=");
                snprintf(buf, NUMEL(buf), "%lu", Board_GetStopFreq());
                interface->SendLine(buf);
                
                interface->SendString("settl=");
                snprintf(buf, NUMEL(buf), "%u", Board_GetSettlingCycles());
                interface->SendLine(buf);
                
                interface->SendString("avg=");
                snprintf(buf, NUMEL(buf), "%u", Board_GetAverages());
                interface->SendLine(buf);
                
                interface->SendString("autorange=");
                interface->SendLine(autorange ? txtEnabled : txtDisabled);
                
                if(!autorange)
                {
                    const AD5933_RangeSettings *range = Board_GetRangeSettings();
                    
                    interface->SendString("gain=");
                    interface->SendLine(range->PGA_Gain == AD5933_GAIN_5 ? txtEnabled : txtDisabled);
                    
                    interface->SendString("voltage=");
                    uint16_t voltage = AD5933_GetVoltageFromRegister(range->Voltage_Range);
                    snprintf(buf, NUMEL(buf), "%u", voltage / range->Attenuation);
                    interface->SendLine(buf);
                    
                    interface->SendString("feedback=");
                    snprintf(buf, NUMEL(buf), "%lu", range->Feedback_Value);
                    interface->SendLine(buf);
                }
                
                interface->SendLine(NULL);
            }
            else
            {
                interface->SendString(txtUnknownOption);
                interface->SendLine(argv[1]);
            }
            break;
    }
    
    interface->CommandFinish();
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
    char buf[32];
    
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    // Board info and AD5933 status
    interface->SendLine(THIS_IS_IMPY BOARD_VERSION BUILT_ON __DATE__ ", " __TIME__ ".\r\n");
    interface->SendString(txtAdStatus);
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
    interface->SendLine(temp);
    
    interface->SendString(txtPortsAvailable);
    snprintf(buf, NUMEL(buf), "(out) %u", PORT_MAX + 1);
    interface->SendString(buf);
    interface->SendString(" (0..");
    snprintf(buf, NUMEL(buf), "%u", PORT_MAX);
    interface->SendString(buf);
    interface->SendLine(")");
    
    interface->SendString(txtFrequencyRange);
    interface->SendString("(frq) ");
    SiStringFromInt(buf, NUMEL(buf), AD5933_FREQ_MIN);
    interface->SendString(buf);
    interface->SendString("..");
    SiStringFromInt(buf, NUMEL(buf), AD5933_FREQ_MAX);
    interface->SendLine(buf);
    
    interface->SendString(txtMaxNumIncrements);
    interface->SendString("(inc) ");
    SiStringFromInt(buf, NUMEL(buf), AD5933_MAX_NUM_INCREMENTS);
    interface->SendLine(buf);
    
    interface->SendString(txtAttenuationsAvailable);
    interface->SendString("(att) ");
    for(uint32_t j = 0; j < NUMEL(board_config.attenuations) && board_config.attenuations[j]; j++)
    {
        SiStringFromInt(buf, NUMEL(buf), board_config.attenuations[j]);
        interface->SendString(buf);
        interface->SendString(" ");
    }
    interface->SendLine(NULL);
    
    interface->SendString(txtFeedbackResistorValues);
    interface->SendString("(rfb) ");
    for(uint32_t j = 0; j < NUMEL(board_config.feedback_resistors) && board_config.feedback_resistors[j]; j++)
    {
        SiStringFromInt(buf, NUMEL(buf), board_config.feedback_resistors[j]);
        interface->SendString(buf);
        interface->SendString(" ");
    }
    interface->SendLine(NULL);
    
    interface->SendString(txtCalibrationValues);
    interface->SendString("(rca) ");
    for(uint32_t j = 0; j < NUMEL(board_config.calibration_values) && board_config.calibration_values[j]; j++)
    {
        SiStringFromInt(buf, NUMEL(buf), board_config.calibration_values[j]);
        interface->SendString(buf);
        interface->SendString(" ");
    }
    interface->SendLine(NULL);
    
    // USB info
    if(board_config.peripherals.usbh)
    {
        interface->SendLine(NULL);
        // TODO print USB info
        interface->SendString(txtUSB);
        interface->SendString(": ");
        interface->SendLine(txtNotImplemented);
    }
    else
    {
        interface->SendLine(NULL);
        interface->SendString(txtUSB);
        interface->SendLine(txtNotInstalled);
    }
    
    // Ethernet info
    if(board_config.peripherals.eth)
    {
        interface->SendLine(NULL);
        interface->SendString(txtEthernetInstalledMacAddr);
        StringFromMacAddress(buf, NUMEL(buf), &board_config.eth_mac[0]);
        interface->SendLine(buf);
    }
    else
    {
        interface->SendLine(NULL);
        interface->SendString(txtEthernet);
        interface->SendLine(txtNotInstalled);
    }
    
    // Memory info
    uint8_t memory_flag = board_config.peripherals.sram || board_config.peripherals.flash || BOARD_HAS_EEPROM;
    interface->SendLine(NULL);
    
    if(BOARD_HAS_EEPROM)
    {
        interface->SendString(txtEEPROM);
        interface->SendString(txtInstalledSize);
        snprintf(buf, NUMEL(buf), "%d", EEPROM_SIZE);
        interface->SendLine(buf);
    }
    else if(memory_flag)
    {
        interface->SendString(txtEEPROM);
        interface->SendLine(txtNotInstalled);
    }
    
    if(board_config.peripherals.sram)
    {
        interface->SendString(txtSRAM);
        interface->SendString(txtInstalledSize);
        snprintf(buf, NUMEL(buf), "%lu", board_config.sram_size);
        interface->SendLine(buf);
    }
    else if(memory_flag)
    {
        interface->SendString(txtSRAM);
        interface->SendLine(txtNotInstalled);
    }
    
    if(board_config.peripherals.flash)
    {
        interface->SendString(txtFlash);
        interface->SendString(txtInstalledSize);
        snprintf(buf, NUMEL(buf), "%lu", board_config.flash_size);
        interface->SendLine(buf);
    }
    else if(memory_flag)
    {
        interface->SendString(txtFlash);
        interface->SendLine(txtNotInstalled);
    }
    
    if(!memory_flag)
    {
        interface->SendLine(txtNoMemory);
    }
    
    interface->SendLine(NULL);
    interface->CommandFinish();
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
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
        return;
    }
    
    port = IntFromSiString(argv[1], &end);
    if(end == NULL || port > PORT_MAX)
    {
        interface->SendString(txtInvalidValue);
        interface->SendLine("port");
        interface->CommandFinish();
        return;
    }
    
    freq = IntFromSiString(argv[2], &end);
    if(end == NULL || freq < AD5933_FREQ_MIN || freq > AD5933_FREQ_MAX)
    {
        interface->SendString(txtInvalidValue);
        interface->SendLine("freq");
        interface->CommandFinish();
        return;
    }
    
    ok = Board_MeasureSingleFrequency((uint8_t)port, freq, &result);
    
    switch(ok)
    {
        case BOARD_OK:
            interface->SendString(txtImpedance);
            buf = malloc(buflen);
            if(buf != NULL)
            {
                snprintf(buf, buflen, "%g < %g", result.Magnitude, result.Angle);
                interface->SendLine(buf);
                free(buf);
            }
            else
            {
                interface->SendLine("PANIC!");
            }
            break;
            
        case BOARD_BUSY:
            interface->SendLine(txtBoardBusy);
            break;
            
        case BOARD_ERROR:
            interface->SendLine(txtNoGain);
            break;
    }
    
    interface->CommandFinish();
}

/**
 * Processes the 'board read' command. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardRead(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "format", CON_ARG_READ_FORMAT, CON_STRING },
        { "raw", CON_ARG_READ_RAW, CON_FLAG },
        { "gain", CON_ARG_READ_GAIN, CON_FLAG }
    };
    
    uint32_t format = format_spec;
    const AD5933_ImpedancePolar *data;
    const AD5933_GainFactor *gain;
    const AD5933_ImpedanceData *raw;
    uint32_t count;
    Console_ArgID mode = CON_ARG_INVALID;
    const char *err = NULL;
    
    // In case data from the previous command has not been deallocated, do so now
    FreeBuffer(&board_read_data);
    
    // Check status, we also allow for incomplete data to be retrieved (status == AD_IDLE)
    if(AD5933_IsBusy())
    {
        interface->SendLine(txtNoReadWhileBusy);
        interface->CommandFinish();
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
            interface->SendString(txtUnknownOption);
            interface->SendLine(argv[j]);
            interface->CommandFinish();
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
                    interface->SendString(txtInvalidValue);
                    interface->SendLine(arg->arg);
                    interface->CommandFinish();
                    return;
                }
                break;
                
            case CON_ARG_READ_GAIN:
            case CON_ARG_READ_RAW:
                if(mode != CON_ARG_INVALID)
                {
                    interface->SendLine(txtOnlyOneArg);
                    interface->CommandFinish();
                    return;
                }
                mode = arg->id;
                break;
                
            default:
                // Should not happen, means that a defined argument has no switch case
                interface->SendLine(txtNotImplemented);
                interface->SendLine(arg->arg);
                interface->CommandFinish();
                return;
        }
    }
    
    switch(mode)
    {
        default:
            // Get and assemble data to be sent
            data = Board_GetDataPolar(&count);
            if(data == NULL)
            {
                err = txtNoData;
                break;
            }
            
            board_read_data = Convert_ConvertPolar(format, data, count);
            if(board_read_data.data != NULL)
            {
                interface->SendBuffer((uint8_t *)board_read_data.data, board_read_data.size);
            }
            else
            {
                err = txtOutOfMemory;
            }
            break;
            
        case CON_ARG_READ_GAIN:
            gain = Board_GetGainFactor();
            if(gain == NULL)
            {
                interface->SendLine(txtNotCalibrated);
                break;
            }

            board_read_data = Convert_ConvertGainFactor(gain);
            if(board_read_data.data != NULL)
            {
                interface->SendBuffer((uint8_t *)board_read_data.data, board_read_data.size);
            }
            else
            {
                interface->SendLine(txtOutOfMemory);
            }
            break;
            
        case CON_ARG_READ_RAW:
            raw = Board_GetDataRaw(&count);
            if(raw == NULL)
            {
                err = txtNoRawData;
                break;
            }

            board_read_data = Convert_ConvertRaw(format, raw, count);
            if(board_read_data.data != NULL)
            {
                interface->SendBuffer((uint8_t *)board_read_data.data, board_read_data.size);
            }
            else
            {
                err = txtOutOfMemory;
            }
            break;
    }
    
    if(err != NULL)
    {
        if(format & FORMAT_FLAG_ASCII)
        {
            interface->SendLine(err);
        }
        else
        {
            count = 0;
            interface->SendBuffer((uint8_t *)&count, 4);
        }
    }

    interface->CommandFinish();
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
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
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
            interface->SendString(txtUnknownOption);
            interface->SendLine(argv[j]);
            continue;
        }

        const char *end;
        switch(arg->type)
        {
            case CON_FLAG:
                flag = Console_GetFlag(value);
                if(flag == CON_FLAG_INVALID)
                {
                    interface->SendString(txtInvalidValue);
                    interface->SendLine(arg->arg);
                    continue;
                }
                break;
                
            case CON_INT:
                intval = IntFromSiString(value, &end);
                if(end == NULL)
                {
                    interface->SendString(txtInvalidValue);
                    interface->SendLine(arg->arg);
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
                    interface->SendLine(txtEffectiveNextSweep);
                }
                break;
                
            case CON_ARG_SET_AVG:
                if((intval & ~0xFFFF) == 0)
                {
                    ok = Board_SetAverages(intval);
                }
                else
                {
                    ok = BOARD_ERROR;
                }
                break;
                
            case CON_ARG_SET_ECHO:
                interface->SetEcho(flag == CON_FLAG_ON);
                break;
                
            case CON_ARG_SET_FEEDBACK:
                if(autorange)
                {
                    interface->SendString(txtSetOnlyWhenAutorangeDisabled);
                    interface->SendLine(arg->arg);
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
                    MarkSettingsDirty();
                }
                else
                {
                    ok = BOARD_ERROR;
                }
                break;
                
            case CON_ARG_SET_GAIN:
                if(autorange)
                {
                    interface->SendString(txtSetOnlyWhenAutorangeDisabled);
                    interface->SendLine(arg->arg);
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
                    interface->SendString(txtSetOnlyWhenAutorangeDisabled);
                    interface->SendLine(arg->arg);
                }
                else
                {
                    ok = Board_SetVoltageRange(intval);
                }
                break;
                
            default:
                // Should not happen, means that a defined argument has no switch case
                interface->SendLine(txtNotImplemented);
                break;
        }
        
        if(ok == BOARD_BUSY)
        {
            interface->SendString(txtSetOnlyWhenIdle);
            interface->SendLine(arg->arg);
        }
        else if(ok == BOARD_ERROR)
        {
            interface->SendString(txtInvalidValue);
            interface->SendLine(arg->arg);
        }
    }
    
    interface->CommandFinish();
}

/**
 * Processes the 'board standby' command. This command finishes immediately.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardStandby(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc == 1)
    {
        Board_Standby();
    }
    else
    {
        interface->SendLine(txtErrNoArgs);
    }
    
    interface->CommandFinish();
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
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
        return;
    }
    
    port = IntFromSiString(argv[1], &end);
    if(end == NULL || port > PORT_MAX)
    {
        interface->SendString(txtInvalidValue);
        interface->SendLine("port");
        interface->CommandFinish();
        return;
    }

    ok = Board_StartSweep(port);
    switch(ok)
    {
        case BOARD_OK:
            interface->SendLine(txtOK);
            break;
        case BOARD_BUSY:
            interface->SendLine(txtBoardBusy);
            break;
        case BOARD_ERROR:
            interface->SendLine(txtNoGain);
            break;
    }
    
    interface->CommandFinish();
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
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    Board_GetStatus(&status);
    switch(status.ad_status)
    {
        case AD_MEASURE_IMPEDANCE:
        case AD_MEASURE_IMPEDANCE_AUTORANGE:
            // Point count
            interface->SendString(txtAdStatusSweep);
            snprintf(buf, NUMEL(buf), "%u", status.point);
            interface->SendString(buf);
            interface->SendString(txtOf);
            snprintf(buf, NUMEL(buf), "%u", status.totalPoints);
            interface->SendLine(buf);
            // Autorange status
            interface->SendString(txtAutorangeStatus);
            interface->SendString(status.autorange ? txtEnabled : txtDisabled);
            interface->SendLine(".");
            break;
            
        case AD_IDLE:
        case AD_FINISH_TEMP:
        case AD_FINISH_CALIB:
            interface->SendLine(txtAdStatusIdle);
            if(status.interrupted)
            {
                interface->SendLine(txtLastInterrupted);
            }
            interface->SendLine(status.validData ? txtValidData : txtNoData);
            interface->SendLine(status.validGainFactor ? txtValidGain : txtNoGain);
            break;
            
        case AD_FINISH_IMPEDANCE:
            interface->SendString(txtAdStatusFinishImpedance);
            snprintf(buf, NUMEL(buf), "%u", status.point);
            interface->SendLine(buf);
            interface->SendLine(status.validData ? txtValidData : txtNoData);
            interface->SendLine(status.validGainFactor ? txtValidGain : txtNoGain);
            break;
            
        case AD_MEASURE_TEMP:
            interface->SendLine(txtAdStatusTemp);
            break;
            
        case AD_CALIBRATE:
            interface->SendLine(txtAdStatusCalibrate);
            break;
            
        default:
            // Should not happen, driver should be initialized by now.
            interface->SendLine(txtAdStatusUnknown);
            break;
    }
    
    interface->CommandFinish();
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
            interface->SendLine(txtOK);
        }
        else
        {
            interface->SendLine(txtAdStatusIdle);
        }
    }
    else
    {
        interface->SendLine(txtErrNoArgs);
    }
    interface->CommandFinish();
}

/**
 * Processes the 'board temp' command. This command finishes when {@link Console_TempCallback} is called.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_BoardTemp(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    if(Board_MeasureTemperature(TEMP_AD5933) != BOARD_OK)
    {
        interface->SendLine(txtTempFail);
        interface->CommandFinish();
    }
}

/**
 * Calls the appropriate subcommand processing function for {@code eth} commands.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Eth(uint32_t argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    static const Console_Command cmds[] = {
        { "set", Console_EthSet },
        { "status", Console_EthStatus },
        { "enable", Console_EthEnable },
        { "disable", Console_EthDisable }
    };
    
    if(board_config.peripherals.eth)
    {
        if(argc == 1)
        {
            interface->SendLine(txtErrNoSubcommand);
            interface->CommandFinish();
        }
        else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
        {
            interface->SendLine(txtUnknownSubcommand);
            interface->CommandFinish();
        }
    }
    else
    {
        interface->SendString(txtEthernet);
        interface->SendLine(txtNotInstalled);
        interface->CommandFinish();
    }
}

static void Console_EthDisable(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_EthEnable(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_EthSet(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "dhcp", CON_ARG_SET_DHCP, CON_FLAG },
        { "ip", CON_ARG_SET_IP, CON_STRING }
    };

    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_EthStatus(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

/**
 * Calls the appropriate subcommand processing function for {@code usb} commands.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Usb(uint32_t argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    static const Console_Command cmds[] = {
        { "status", Console_UsbStatus },
        { "info", Console_UsbInfo },
        { "eject", Console_UsbEject },
        { "write", Console_UsbWrite },
        { "ls", Console_UsbLs }
    };
    
    if(board_config.peripherals.usbh)
    {
        if(argc == 1)
        {
            interface->SendLine(txtErrNoSubcommand);
            interface->CommandFinish();
        }
        else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
        {
            interface->SendLine(txtUnknownSubcommand);
            interface->CommandFinish();
        }
    }
    else
    {
        interface->SendString(txtUSB);
        interface->SendLine(txtNotInstalled);
        interface->CommandFinish();
    }
}

static void Console_UsbEject(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_UsbInfo(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_UsbLs(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_UsbStatus(uint32_t argc, char **argv __attribute__((unused)))
{
    if(argc != 1)
    {
        interface->SendLine(txtErrNoArgs);
        interface->CommandFinish();
        return;
    }
    
    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

static void Console_UsbWrite(uint32_t argc, char **argv)
{
    // Arguments: file
    
    if(argc != 2)
    {
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
        return;
    }

    interface->SendLine(txtNotImplemented);
    interface->CommandFinish();
}

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
            interface->SendBuffer((uint8_t *)strHelp.data, strHelp.length);
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
                interface->SendBuffer((const uint8_t *)topic->text.data, topic->text.length);
            }
            else
            {
                interface->SendLine(txtUnknownTopic);
            }
            break;
        default:
            // Wrong number of arguments, print error message
            interface->SendLine(txtErrArgNum);
            break;
    }
    
    interface->CommandFinish();
}

/**
 * Processes the {@code setup} command.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Setup(uint32_t argc, char **argv)
{
    static const Console_Arg cmds[] = {
        { "attenuation", CON_CMD_SETUP_ATTENUATIONS, CON_STRING },
        { "feedback", CON_CMD_SETUP_FEEDBACK, CON_STRING },
        { "calibration", CON_CMD_SETUP_CALIBRATION, CON_STRING },
        { "coupl", CON_CMD_SETUP_COUPL, CON_STRING },
        { "sram", CON_CMD_SETUP_SRAM, CON_STRING },
        { "flash", CON_CMD_SETUP_FLASH, CON_STRING },
        { "eth", CON_CMD_SETUP_ETH, CON_STRING },
        { "usbh", CON_CMD_SETUP_USBH, CON_STRING }
    };
    
    Console_ArgID cmd = CON_ARG_INVALID;
    const char *err = NULL;
    Console_FlagValue flag;
    uint32_t ints[8] = { 0 };
    uint32_t intval = 0;
    const char *end;
    
    // Every command has at least one argument
    if(argc <= 2)
    {
        interface->SendLine(txtErrArgNum);
        interface->CommandFinish();
        return;
    }
    
    for(uint32_t j = 0; j < NUMEL(cmds); j++)
    {
        if(strcmp(argv[1], cmds[j].arg) == 0)
        {
            cmd = cmds[j].id;
            break;
        }
    }
    
    switch(cmd)
    {
        case CON_CMD_SETUP_ATTENUATIONS:
            if(argc > 2 + NUMEL(board_config.attenuations))
            {
                err = txtErrArgNum;
                break;
            }
            for(uint32_t j = 2; j < argc; j++)
            {
                ints[intval++] = IntFromSiString(argv[j], &end);
                if(end == NULL)
                {
                    err = txtWrongNumber;
                    break;
                }
            }
            if(err == NULL)
            {
                for(uint32_t j = 0; j < NUMEL(board_config.attenuations); j++)
                {
                    board_config.attenuations[j] = ints[j];
                }
            }
            break;
            
        case CON_CMD_SETUP_FEEDBACK:
            if(argc > 2 + NUMEL(board_config.feedback_resistors))
            {
                err = txtErrArgNum;
                break;
            }
            for(uint32_t j = 2; j < argc; j++)
            {
                ints[intval++] = IntFromSiString(argv[j], &end);
                if(end == NULL)
                {
                    err = txtWrongNumber;
                    break;
                }
            }
            if(err == NULL)
            {
                for(uint32_t j = 0; j < NUMEL(board_config.feedback_resistors); j++)
                {
                    board_config.feedback_resistors[j] = ints[j];
                }
            }
            break;
            
        case CON_CMD_SETUP_CALIBRATION:
            if(argc > 2 + NUMEL(board_config.calibration_values))
            {
                err = txtErrArgNum;
                break;
            }
            for(uint32_t j = 2; j < argc; j++)
            {
                ints[intval++] = IntFromSiString(argv[j], &end);
                if(end == NULL)
                {
                    err = txtWrongNumber;
                    break;
                }
            }
            if(err == NULL)
            {
                for(uint32_t j = 0; j < NUMEL(board_config.calibration_values); j++)
                {
                    board_config.calibration_values[j] = ints[j];
                }
            }
            break;
            
        case CON_CMD_SETUP_COUPL:
            if(argc != 3)
            {
                err = txtErrArgNum;
                break;
            }
            intval = IntFromSiString(argv[2], &end);
            if(end == NULL || intval > 1000)
            {
                err = txtWrongTau;
                break;
            }
            board_config.coupling_tau = intval;
            break;
            
        case CON_CMD_SETUP_SRAM:
            if(argc > 4)
            {
                err = txtErrArgNum;
                break;
            }
            flag = Console_GetFlag(argv[2]);
            switch(flag)
            {
                case CON_FLAG_ON:
                    if(argc != 4)
                    {
                        err = txtErrArgNum;
                        break;
                    }
                    intval = IntFromSiString(argv[3], &end);
                    if(end == NULL)
                    {
                        err = txtWrongNumber;
                        break;
                    }
                    board_config.peripherals.sram = 1;
                    board_config.sram_size = intval;
                    break;
                    
                case CON_FLAG_OFF:
                    board_config.peripherals.sram = 0;
                    board_config.sram_size = 0;
                    break;
                    
                case CON_FLAG_INVALID:
                    err = txtWrongFlag;
                    break;
            }
            break;
            
        case CON_CMD_SETUP_FLASH:
            if(argc > 4)
            {
                err = txtErrArgNum;
                break;
            }
            flag = Console_GetFlag(argv[2]);
            switch(flag)
            {
                case CON_FLAG_ON:
                    if(argc != 4)
                    {
                        err = txtErrArgNum;
                        break;
                    }
                    intval = IntFromSiString(argv[3], &end);
                    if(end == NULL)
                    {
                        err = txtWrongNumber;
                        break;
                    }
                    board_config.peripherals.flash = 1;
                    board_config.flash_size = intval;
                    break;
                    
                case CON_FLAG_OFF:
                    board_config.peripherals.flash = 0;
                    board_config.flash_size = 0;
                    break;
                    
                case CON_FLAG_INVALID:
                    err = txtWrongFlag;
                    break;
            }
            break;
            
        case CON_CMD_SETUP_ETH:
            if(argc > 4)
            {
                err = txtErrArgNum;
                break;
            }
            flag = Console_GetFlag(argv[2]);
            switch(flag)
            {
                case CON_FLAG_ON:
                    if(argc != 4)
                    {
                        err = txtErrArgNum;
                        break;
                    }
                    uint8_t mac[NUMEL(board_config.eth_mac)];
                    if(MacAddressFromString(argv[3], &mac[0]) < 0)
                    {
                        err = txtWrongMac;
                        break;
                    }
                    board_config.peripherals.eth = 1;
                    memcpy(&board_config.eth_mac[0], mac, sizeof(mac));
                    break;
                    
                case CON_FLAG_OFF:
                    board_config.peripherals.eth = 0;
                    break;
                    
                case CON_FLAG_INVALID:
                    err = txtWrongFlag;
                    break;
            }
            break;
            
        case CON_CMD_SETUP_USBH:
            if(argc != 3)
            {
                err = txtErrArgNum;
                break;
            }
            flag = Console_GetFlag(argv[2]);
            if(flag == CON_FLAG_INVALID)
            {
                err = txtWrongFlag;
                break;
            }
            board_config.peripherals.usbh = (flag == CON_FLAG_ON ? 1 : 0);
            break;
            
        default:
            err = txtUnknownConfig;
            break;
    }
    
    if(err == NULL)
    {
        WriteConfiguration();
    }
    else
    {
        interface->SendLine(err);
    }
    
    interface->CommandFinish();
}

/**
 * Processes the {@code debug} command.
 * 
 * @param argc Number of arguments
 * @param argv Array of arguments
 */
static void Console_Debug(uint32_t argc, char **argv __attribute__((unused)))
{
#ifdef DEBUG
    if(argc == 1)
    {
        interface->SendLine("send, echo, printf-float, malloc, leak, usb-paksize, heap, tim, mux, output,");
        interface->SendLine("dump");
        interface->CommandFinish();
        return;
    }
    
    if(strcmp(argv[1], "send") == 0)
    {
        // Send some strings to test how the VCP copes with multiple calls in close succession
        interface->SendString("this is a test string\r\n");
        interface->SendString("second SendString call with a string that is longer than before.\r\n");
        interface->SendString("short line\r\n");
    }
    else if(strcmp(argv[1], "echo") == 0)
    {
        // Echo back all received arguments
        for(uint32_t j = 2; j < argc; j++)
        {
            interface->SendLine(argv[j]);
        }
    }
    else if(strcmp(argv[1], "printf-float") == 0)
    {
        // Test the number format of floating point numbers
        char *buf;
        const uint32_t size = 100;
        buf = malloc(size);
        if(buf != NULL)
        {
            float f1 = 1.5378f;
            float f2 = atan2f(0.5f, 0.5f);
            snprintf(buf, size, "%g < %g", f1, f2);
            interface->SendLine(buf);
            for(uint32_t j = 0; j < 10; j++)
            {
                f1 *= 10.0f;
                f2 /= 10.0f;
                snprintf(buf, size, "%g < %g", f1, f2);
                interface->SendLine(buf);
            }
            free(buf);
        }
        else
        {
            interface->SendLine("Pointer was NULL.");
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
            interface->SendLine("Could not allocate string buffer and pointer buffer.");
        }
        else
        {
            interface->SendLine("Trying to allocate some buffers...");
            Console_Flush();
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
                    interface->SendString(buf);
                    Console_Flush();
                }
                buffers++;
            }
            snprintf(buf, len, "\r\nCould allocate %lu buffers with %lu bytes each.", buffers, size);
            interface->SendLine(buf);
            snprintf(buf, len, "For everyone too lazy to use their brain, that's %lu bytes in total.", buffers * size);
            interface->SendLine(buf);
            interface->SendLine("Now freeing...");
            Console_Flush();
            for(uint32_t j = 0; j < buffers; j++)
            {
                free(allocs[j]);
            }
            interface->SendLine("Finished.");
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
                interface->SendLine("Don't be silly.");
            }
            else if(malloc(bytes) != NULL)
            {
                interface->SendLine("Allocation successful, memory leaked.");
            }
            else
            {
                interface->SendLine("Allocation failed.");
            }
        }
        else
        {
            interface->SendLine(txtErrArgNum);
        }
    }
    else if(strcmp(argv[1], "usb-paksize") == 0)
    {
        // Test VCP with multiples of USB packet size
        static const char *txtTest = "This is a string with 64 bytes of data to be sent over the VCP..";
        static const char *txtTest2 = "This is an even longer text that should hold 128 bytes, which is exactly two packet sizes, to test the failure with two packets.";
        interface->SendBuffer((uint8_t *)(argc == 2 ? txtTest : txtTest2), strlen(argc == 2 ? txtTest : txtTest2));
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
        interface->SendString(buf);
        snprintf(buf, NUMEL(buf), "Current break: %p\r\nFree bytes: %lu\r\n",
                brk, (uint32_t)((void *)&_Heap_Limit - brk));
        interface->SendString(buf);
    }
    else if(strcmp(argv[1], "tim") == 0)
    {
        const char *end;
        uint32_t intval;
        
        // Enable or disable TIM10 OC output
        if(argc != 3)
        {
            interface->SendLine(txtErrArgNum);
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
                    intval = IntFromSiString(argv[2], &end);
                    if(end != NULL && intval < 0xFFFF)
                    {
                        HAL_TIM_OC_Stop(&htim10, TIM_CHANNEL_1);
                        htim10.Instance->PSC = intval;
                    }
                    break;
            }
        }
    }
    else if(strcmp(argv[1], "mux") == 0)
    {
        // Set output mux port, or disable
        if(argc != 3)
        {
            interface->SendLine(txtErrArgNum);
        }
        else
        {
            const char *end;
            uint8_t port = IntFromSiString(argv[2], &end);
            
            if(strcmp(argv[2], "off") == 0)
            {
                port = ADG725_CHIP_ENABLE_NOT;
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
                HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
                interface->SendLine("Switched off.");
            }
            else if(end != NULL && port <= 15)
            {
                port &= ADG725_MASK_PORT;
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
                HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
                interface->SendLine("Port set.");
            }
            else
            {
                interface->SendLine("Unknown port.");
            }
        }
    }
    else if(strcmp(argv[1], "output") == 0)
    {
        // Output a single frequency on specified port (arguments: freq port)
        if(argc != 4)
        {
            interface->SendLine(txtErrArgNum);
        }
        else
        {
            const char *end1, *end2;
            uint32_t freq = IntFromSiString(argv[2], &end1);
            uint8_t port = IntFromSiString(argv[3], &end2);
            
            if(end1 == NULL)
            {
                interface->SendLine("Bad frequency.");
            }
            else if(end2 == NULL || port > 15)
            {
                interface->SendLine("Unknown port.");
            }
            else
            {
                // Set mux
                port &= ADG725_MASK_PORT;
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_RESET);
                HAL_SPI_Transmit(&hspi3, &port, 1, BOARD_SPI_TIMEOUT);
                HAL_GPIO_WritePin(BOARD_SPI_SS_GPIO_PORT, BOARD_SPI_SS_GPIO_MUX, GPIO_PIN_SET);
                
                AD5933_Debug_OutputFreq(freq, Board_GetRangeSettings());
            }
        }
    }
    else if(strcmp(argv[1], "dump") == 0)
    {
        // Dump contents of the EEPROM in binary format to the console
        const size_t size = 1024;
        const uint8_t addr = 0xA0;
        
        uint8_t *buffer = malloc(size);
        if(buffer == NULL)
        {
            interface->SendLine("Failed to allocate memory.");
        }
        else
        {
            HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c1, addr, 0, 1, buffer, size, 200);
            switch(ret)
            {
                case HAL_OK:
                    for(uint32_t j = 0; j < size; j++)
                    {
                        interface->SendChar(buffer[j]);
                    }
                    break;
                case HAL_TIMEOUT:
                case HAL_ERROR:
                case HAL_BUSY:
                    interface->SendLine("HAL_I2C_Mem_Read error.");
                    break;
            }
            free(buffer);
        }
    }
    else
    {
        interface->SendLine(txtUnknownSubcommand);
    }
#else
    interface->SendLine("This is a release build, no debug code compiled in.");
#endif

    interface->CommandFinish();
}

// Exported functions ---------------------------------------------------------

/**
 * This function sets up the console and should be called before any other console functions.
 */
void Console_Init(void)
{
    Console_InitHelp();
    
    format_spec = FORMAT_DEFAULT;
}

/**
 * Processes the specified 0-terminated string to extract commands and arguments and calls the appropriate functions.
 * 
 * @param itf Pointer to an interface structure with functions to use for communication
 * @param str Pointer to a command line string
 */
void Console_ProcessLine(Console_Interface *itf, char *str)
{
    uint32_t argc;
    
    assert_param(str != NULL);
    assert_param(itf != NULL);
    
    interface = itf;
    argc = Console_GetArguments(str);
    
    if(argc == 0)
    {
        // Command line is empty, do nothing
        interface->CommandFinish();
    }
    else if(!Console_CallProcessor(argc, arguments, commands, NUMEL(commands)))
    {
        interface->SendLine(txtUnknownCommand);
        interface->CommandFinish();
    }
}

/**
 * Gets the current format specification.
 */
uint32_t Console_GetFormat(void)
{
   return format_spec; 
}

/**
 * Sets the format specification to the specified value.
 * @param spec Format specification
 */
void Console_SetFormat(uint32_t spec)
{
    format_spec = spec;
}

// Callbacks ------------------------------------------------------------------

/**
 * Called when a calibration is finished.
 */
void Console_CalibrateCallback(void)
{
    interface->SendLine(txtOK);
    Console_Flush();
    interface->CommandFinish();
}

/**
 * Called when a temperature measurement is finished.
 * 
 * @param temp The measured temperature
 */
void Console_TempCallback(float temp)
{
    char buf[16];
    snprintf(buf, NUMEL(buf), "%.1f %cC", temp, '\xB0' /* Degree symbol in ISO 8859-1 and -15 */); 
    interface->SendLine(buf);
    Console_Flush();
    interface->CommandFinish();
}

// ----------------------------------------------------------------------------
