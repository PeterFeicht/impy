/**
 * @file    console.c
 * @author  Peter
 * @date    16.05.2014
 * @brief   This file contains the definition of the console interface.
 */

// Includes -------------------------------------------------------------------
#include "console.h"

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
 * @param argc Number of arguments in the command line
 */
typedef void (*Console_CommandFunc)(uint32_t argc);

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
// Command line processors
static void Console_Board(uint32_t argc);
static void Console_BoardGet(uint32_t argc);
static void Console_BoardInfo(uint32_t argc);
static void Console_BoardMeasure(uint32_t argc);
static void Console_BoardRead(uint32_t argc);
static void Console_BoardSet(uint32_t argc);
static void Console_BoardStart(uint32_t argc);
static void Console_BoardStatus(uint32_t argc);
static void Console_BoardStop(uint32_t argc);
static void Console_BoardTemp(uint32_t argc);
static void Console_Eth(uint32_t argc);
static void Console_EthDisable(uint32_t argc);
static void Console_EthEnable(uint32_t argc);
static void Console_EthSet(uint32_t argc);
static void Console_EthStatus(uint32_t argc);
static void Console_Usb(uint32_t argc);
static void Console_UsbEject(uint32_t argc);
static void Console_UsbInfo(uint32_t argc);
static void Console_UsbLs(uint32_t argc);
static void Console_UsbStatus(uint32_t argc);
static void Console_UsbWrite(uint32_t argc);
static void Console_Help(uint32_t argc);

// Private variables ----------------------------------------------------------
static char *arguments[CON_MAX_ARGUMENTS];

// Console definition
static const char* txtHelp = (char *)
        #include "../command-line.txt"
        ;
static const Console_HelpEntry txtHelpTopics[] = {
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

// Private functions ----------------------------------------------------------

static void Console_InitHelp(void)
{
    
}

static void Console_Board(uint32_t argc)
{
    
}

static void Console_BoardGet(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "option", CON_ARG_GET_OPTION, CON_STRING, 0 }
    };
    
}

static void Console_BoardInfo(uint32_t argc)
{
    
}

static void Console_BoardMeasure(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "port", CON_ARG_MEAS_PORT, CON_STRING, 0 },
        { "freq", CON_ARG_MEAS_FREQ, CON_INT, 0 }
    };
    
}

static void Console_BoardRead(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "format", CON_ARG_READ_FORMAT, CON_STRING, 1 }
    };
    
}

static void Console_BoardSet(uint32_t argc)
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

static void Console_BoardStart(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "port", CON_ARG_START_PORT, CON_STRING, 0 }
    };
    
}

static void Console_BoardStatus(uint32_t argc)
{
    
}

static void Console_BoardStop(uint32_t argc)
{
    
}

static void Console_BoardTemp(uint32_t argc)
{
    
}

static void Console_Eth(uint32_t argc)
{
    
}

static void Console_EthDisable(uint32_t argc)
{
    
}

static void Console_EthEnable(uint32_t argc)
{
    
}

static void Console_EthSet(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "dhcp", CON_ARG_SET_DHCP, CON_FLAG, 1 },
        { "ip", CON_ARG_SET_IP, CON_STRING, 1 }
    };
    
}

static void Console_EthStatus(uint32_t argc)
{
    
}

static void Console_Usb(uint32_t argc)
{
    
}

static void Console_UsbEject(uint32_t argc)
{
    
}

static void Console_UsbInfo(uint32_t argc)
{
    
}

static void Console_UsbLs(uint32_t argc)
{
    
}

static void Console_UsbStatus(uint32_t argc)
{
    
}

static void Console_UsbWrite(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "file", CON_ARG_WRITE_FILE, CON_STRING, 0 }
    };
    
}

static void Console_Help(uint32_t argc)
{
    static const Console_Arg args[] = {
        { "topic", CON_ARG_HELP_TOPIC, CON_STRING, 0 }
    };
    
}


// Exported functions ---------------------------------------------------------

/**
 * TODO documentation
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
    if(str == NULL)
    {
        return;
    }
    
    
}

// ----------------------------------------------------------------------------
