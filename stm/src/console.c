/**
 * @file    console.c
 * @author  Peter
 * @date    16.05.2014
 * @brief   This file contains the definition of the console interface.
 */

// Includes -------------------------------------------------------------------
#include <string.h>
#include "console.h"
#include "main.h"

// Private type definitions ---------------------------------------------------
typedef enum
{
    CON_ARG_INVALID = 0,
    CON_ARG_READ_FORMAT,
    CON_ARG_SET_AUTORANGE,
    CON_ARG_SET_ECHO,
    CON_ARG_SET_FORMAT,
    CON_ARG_SET_GAIN,
    CON_ARG_SET_SETTL,
    CON_ARG_SET_START,
    CON_ARG_SET_STEPS,
    CON_ARG_SET_STOP,
    CON_ARG_SET_VOLTAGE,
    CON_ARG_SET_DHCP,
    CON_ARG_SET_IP
} Console_ArgID;

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
// Command line processors
static void Console_Board(uint32_t argc, char **argv);
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
    { "voltage", CON_ARG_SET_VOLTAGE, CON_STRING },
    { "gain", CON_ARG_SET_GAIN, CON_FLAG },
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
        { "start", Console_BoardStart },
        { "stop", Console_BoardStop },
        { "status", Console_BoardStatus },
        { "temp", Console_BoardTemp },
        { "measure", Console_BoardMeasure },
        { "read", Console_BoardRead }
    };
    
    if(argc == 1)
    {
        VCP_SendString(txtErrNoSubcommand);
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        VCP_SendString(txtUnknownSubcommand);
        VCP_CommandFinish();
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
        VCP_SendString(txtErrArgNum);
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
            
        case CON_ARG_SET_FORMAT:
            // TODO get format
            VCP_SendString(txtNotImplemented);
            break;
            
        case CON_ARG_SET_GAIN:
            VCP_SendLine(Board_GetRangeSettings()->PGA_Gain == AD5933_GAIN_1 ? txtDisabled : txtEnabled);
            break;
            
        case CON_ARG_SET_SETTL:
            if(autorange)
            {
                VCP_SendString(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                snprintf(buf, NUMEL(buf), "%u", Board_GetSettlingCycles());
                VCP_SendLine(buf);
            }
            break;
            
        case CON_ARG_SET_START:
            if(autorange)
            {
                VCP_SendString(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                snprintf(buf, NUMEL(buf), "%lu", Board_GetStartFreq());
                VCP_SendLine(buf);
            }
            break;
        case CON_ARG_SET_STEPS:
            if(autorange)
            {
                VCP_SendString(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                snprintf(buf, NUMEL(buf), "%u", Board_GetFreqSteps());
                VCP_SendLine(buf);
            }
            break;
            
        case CON_ARG_SET_STOP:
            if(autorange)
            {
                VCP_SendString(txtGetOnlyWhenAutorangeDisabled);
            }
            else
            {
                snprintf(buf, NUMEL(buf), "%lu", Board_GetStopFreq());
                VCP_SendLine(buf);
            }
            break;
            
        case CON_ARG_SET_VOLTAGE:
            if(autorange)
            {
                VCP_SendString(txtGetOnlyWhenAutorangeDisabled);
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
            VCP_SendString(txtUnknownOption);
            break;
    }
    
    VCP_CommandFinish();
}

static void Console_BoardInfo(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_BoardMeasure(uint32_t argc, char **argv)
{
    // Arguments: port, freq

    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_BoardRead(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "format", CON_ARG_READ_FORMAT, CON_STRING }
    };

    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_BoardSet(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_BoardStart(uint32_t argc, char **argv)
{
    // Arguments: port

    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_BoardStatus(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_BoardStop(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
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
        VCP_SendString(txtErrArgNum);
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
        VCP_SendString(txtErrNoSubcommand);
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        VCP_SendString(txtUnknownSubcommand);
        VCP_CommandFinish();
    }
}

static void Console_EthDisable(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_EthEnable(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_EthSet(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "dhcp", CON_ARG_SET_DHCP, CON_FLAG },
        { "ip", CON_ARG_SET_IP, CON_STRING }
    };

    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_EthStatus(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
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
        VCP_SendString(txtErrNoSubcommand);
        VCP_CommandFinish();
    }
    else if(!Console_CallProcessor(argc - 1, argv + 1, cmds, NUMEL(cmds)))
    {
        VCP_SendString(txtUnknownSubcommand);
        VCP_CommandFinish();
    }
}

static void Console_UsbEject(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbInfo(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbLs(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbStatus(uint32_t argc, char **argv)
{
    VCP_SendString(txtNotImplemented);
    VCP_CommandFinish();
}

static void Console_UsbWrite(uint32_t argc, char **argv)
{
    // Arguments: file

    VCP_SendString(txtNotImplemented);
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
                VCP_SendString(txtUnknownTopic);
            }
            break;
        default:
            // Wrong number of arguments, print error message
            VCP_SendString(txtErrArgNum);
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
static void Console_Debug(uint32_t argc, char **argv)
{
    if(argc == 1)
    {
        VCP_CommandFinish();
        return;
    }
    
    if(strcmp(argv[1], "send") == 0)
    {
        VCP_SendString("this is a test string\r\n");
        VCP_SendString("second SendString call with a string that is longer than before.\r\n");
        VCP_SendString("short line\r\n");
    }
    else if(strcmp(argv[1], "echo") == 0)
    {
        for(uint32_t j = 2; j < argc; j++)
        {
            VCP_SendLine(argv[j]);
        }
    }
    else
    {
        VCP_SendString(txtUnknownSubcommand);
    }

    VCP_CommandFinish();
}

// Exported functions ---------------------------------------------------------

/**
 * This function sets up the console and should be called before any other console functions.
 */
void Console_Init(void)
{
    Console_InitHelp();
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
        VCP_SendString(txtUnknownCommand);
        VCP_CommandFinish();
    }
}

// ----------------------------------------------------------------------------
