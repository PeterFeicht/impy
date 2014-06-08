/**
 * @file    strings_en.h
 * @author  Peter
 * @date    06.06.2014
 * @brief   Defines user interface strings in English.
 */

#ifndef STRINGS_EN_H_
#define STRINGS_EN_H_

// Console strings ------------------------------------------------------------
// XXX Maybe there should be some kind of error code prefix for parsing by PC software
const char *txtErrArgNum = "Wrong number of arguments.\r\n";
const char *txtErrNoSubcommand = "Missing command, type 'help' for possible commands.\r\n";
const char *txtUnknownTopic = "Unknown help topic, type 'help' for possible commands.\r\n";
const char *txtUnknownCommand = "Unknown command.\r\n";
const char *txtUnknownSubcommand = "Unknown subcommand.\r\n";
const char *txtNotImplemented = "Not yet implemented.\r\n";
const char *txtUnknownOption = "Unknown option.\r\n";
const char *txtInvalidValue = "Invalid value for this argument: ";
const char *txtSetOnlyWhenIdle = "This value cannot be set while a sweep is running: ";
const char *txtEffectiveNextSweep = "Autorange setting will take effect on the next sweep.\r\n";
const char *txtSetOnlyWhenAutorangeDisabled = "This value can only be set when autoranging is disabled: ";
const char *txtGetOnlyWhenAutorangeDisabled = "This value can only be read when autoranging is disabled.\r\n";

// Words ----------------------------------------------------------------------
const char *txtEnabled = "enabled";
const char *txtDisabled = "disabled";
const char *txtOn = "on";
const char *txtOff = "off";

// ----------------------------------------------------------------------------

#endif /* STRINGS_EN_H_ */
