/**
 * @file    console.c
 * @author  Peter
 * @date    16.05.2014
 * @brief   This file contains the definition of the console interface.
 */

// Includes -------------------------------------------------------------------
#include "console.h"
#include <string.h>

// Private type definitions ---------------------------------------------------
typedef enum
{
    CON_ARG_GET_OPTION = 1,
    CON_ARG_MEAS_FREQ,
    CON_ARG_MEAS_PORT,
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
    CON_ARG_START_PORT,
    CON_ARG_SET_DHCP,
    CON_ARG_SET_IP,
    CON_ARG_HELP_TOPIC,
    CON_ARG_WRITE_FILE
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
    uint8_t optional;       //!< Whether this argument must be present or not
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

// Private variables ----------------------------------------------------------
static char *arguments[CON_MAX_ARGUMENTS];

// Console definition
extern char helptext_start;
static char *txtHelp;
static Console_HelpEntry txtHelpTopics[] = {
    { "eth", NULL },
    { "usb", NULL },
    { "format", NULL },
    { "settl", NULL },
    { "voltage", NULL },
    { "autorange", NULL },
    { "echo", NULL }
};
static const Console_Command cmdsBoard[] = {
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
static const Console_Command cmdsEth[] = {
    { "set", Console_EthSet },
    { "status", Console_EthStatus },
    { "enable", Console_EthEnable },
    { "disable", Console_EthDisable }
};
static const Console_Command cmdsUsb[] = {
    { "status", Console_UsbStatus },
    { "info", Console_UsbInfo },
    { "eject", Console_UsbEject },
    { "write", Console_UsbWrite },
    { "ls", Console_UsbLs }
};
static const Console_Command commands[] = {
    { "board", Console_Board },
    { "eth", Console_Eth },
    { "usb", Console_Usb },
    { "help", Console_Help }
};

// Strings
const char *txtErrArgNum = "Wrong number of arguments.\r\n";
const char *txtUnknownTopic = "Unknown help topic, type 'help' for possible commands.\r\n";
const char *txtUnknownCommand = "Unknown command.\r\n";
const char *txtUnknownSubcommand = "Unknown subcommand.\r\n";

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
        }
        
        if(newline && *help == 'h')
        {
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
 * @param help Pointer to a new line in the help text that may contain a help topic
 * @return {@code 0} if a help topic was added, nonzero value on error.
 */
static uint8_t Console_AddHelpTopic(char *help)
{
    // This text is at the beginning of a help topic start line
    static const char txt[] = "help ";
    char *cmd;
    char *tmp;
    
    for(uint32_t j = 0; j < sizeof(txt); j++)
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
    for(uint32_t j = 0; j < sizeof(txtHelpTopics); j++)
    {
        if(strcmp(cmd, txtHelpTopics[j].cmd) == 0)
        {
            txtHelpTopics[j].text = help;
            return 0;
        }
    }
    
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

static void Console_Board(uint32_t argc, char **argv)
{
    
}

static void Console_BoardGet(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "option", CON_ARG_GET_OPTION, CON_STRING, 0 }
    };
    
}

static void Console_BoardInfo(uint32_t argc, char **argv)
{
    
}

static void Console_BoardMeasure(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "port", CON_ARG_MEAS_PORT, CON_STRING, 0 },
        { "freq", CON_ARG_MEAS_FREQ, CON_INT, 0 }
    };
    
}

static void Console_BoardRead(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "format", CON_ARG_READ_FORMAT, CON_STRING, 1 }
    };
    
}

static void Console_BoardSet(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "start", CON_ARG_SET_START, CON_INT, 1 },
        { "stop", CON_ARG_SET_STOP, CON_INT, 1 },
        { "steps", CON_ARG_SET_STEPS, CON_INT, 1 },
        { "settl", CON_ARG_SET_SETTL, CON_STRING, 1 },
        { "voltage", CON_ARG_SET_VOLTAGE, CON_STRING, 1 },
        { "gain", CON_ARG_SET_GAIN, CON_FLAG, 1 },
        { "format", CON_ARG_SET_FORMAT, CON_STRING, 1 },
        { "autorange", CON_ARG_SET_AUTORANGE, CON_FLAG, 1 },
        { "echo", CON_ARG_SET_ECHO, CON_FLAG, 1 }
    };
    
}

static void Console_BoardStart(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "port", CON_ARG_START_PORT, CON_STRING, 0 }
    };
    
}

static void Console_BoardStatus(uint32_t argc, char **argv)
{
    
}

static void Console_BoardStop(uint32_t argc, char **argv)
{
    
}

static void Console_BoardTemp(uint32_t argc, char **argv)
{
    
}

static void Console_Eth(uint32_t argc, char **argv)
{
    
}

static void Console_EthDisable(uint32_t argc, char **argv)
{
    
}

static void Console_EthEnable(uint32_t argc, char **argv)
{
    
}

static void Console_EthSet(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "dhcp", CON_ARG_SET_DHCP, CON_FLAG, 1 },
        { "ip", CON_ARG_SET_IP, CON_STRING, 1 }
    };
    
}

static void Console_EthStatus(uint32_t argc, char **argv)
{
    
}

static void Console_Usb(uint32_t argc, char **argv)
{
    
}

static void Console_UsbEject(uint32_t argc, char **argv)
{
    
}

static void Console_UsbInfo(uint32_t argc, char **argv)
{
    
}

static void Console_UsbLs(uint32_t argc, char **argv)
{
    
}

static void Console_UsbStatus(uint32_t argc, char **argv)
{
    
}

static void Console_UsbWrite(uint32_t argc, char **argv)
{
    static const Console_Arg args[] = {
        { "file", CON_ARG_WRITE_FILE, CON_STRING, 0 }
    };
    
}

/**
 * Process {@code help} command.
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
            for(j = 0; j < sizeof(txtHelpTopics); j++)
            {
                if(strcmp(argv[1], txtHelpTopics[j].cmd) == 0)
                {
                    VCP_SendBuffer((uint8_t *)txtHelpTopics[j].text, strlen(txtHelpTopics[j].text));
                }
            }
            if(j == sizeof(txtHelpTopics))
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
        return;
    }
    
    argc = Console_GetArguments(str);
    Console_CallProcessor(argc, arguments, commands, sizeof(commands));
}

// ----------------------------------------------------------------------------
