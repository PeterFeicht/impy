/**
 * @file    strings_en.h
 * @author  Peter Feichtinger
 * @date    06.06.2014
 * @brief   Defines user interface strings in English.
 */

#ifndef STRINGS_EN_H_
#define STRINGS_EN_H_

// Constants ------------------------------------------------------------------
#define THIS_IS_IMPY    "This is impy version "
#define BUILT_ON        " built on "

// Console strings ------------------------------------------------------------
// XXX Maybe there should be some kind of error code prefix for parsing by PC software
const char *txtErrArgNum = "Wrong number of arguments.";
const char *txtErrNoSubcommand = "Missing command, type 'help' for possible commands.";
const char *txtUnknownTopic = "Unknown help topic, type 'help' for possible commands.";
const char *txtUnknownCommand = "Unknown command.";
const char *txtUnknownSubcommand = "Unknown subcommand.";
const char *txtNotImplemented = "Not yet implemented.";
const char *txtUnknownOption = "Unknown option.";
// board get, board set
const char *txtInvalidValue = "Invalid value for this argument: ";
const char *txtSetOnlyWhenIdle = "This value cannot be set while a sweep is running: ";
const char *txtEffectiveNextSweep = "Autorange setting will take effect on the next sweep.";
const char *txtSetOnlyWhenAutorangeDisabled = "This value can only be set when autoranging is disabled: ";
const char *txtGetOnlyWhenAutorangeDisabled = "This value can only be read when autoranging is disabled.";
// board status
const char *txtAdStatusUnknown = "Unknown AD5933 driver status, something went wrong.";
const char *txtAdStatusSweep = "Impedance measurement is running, points measured: ";
const char *txtAdStatusTemp = "Temperature measurement is running.";
const char *txtAdStatusIdle = "No measurement is running.";
const char *txtAdStatusFinish = "Measurement finished, points measured: ";
const char *txtAdStatusCalibrate = "Calibration measurement is running.";
const char *txtAutorangeStatus = "Autoranging is ";
const char *txtLastInterrupted = "The last measurement was interrupted.";
// board info
const char *txtAdStatus = "AD5933 driver status: ";
const char *txtAdStatusMeasureImpedance = "Impedance measurement is running.";
const char *txtPortsAvailable = "Ports available for measurements: ";
const char *txtAttenuationsAvailable = "Possible voltage attenuation factors: ";
const char *txtFeedbackResistorValues = "Feedback resistor values: ";
const char *txtCalibrationValues = "Calibration resistor values: ";
const char *txtNotInstalled = " not installed.";
const char *txtNoMemory = "\r\nNo external memory installed.";
const char *txtFrequencyRange = "The possible frequency range in Hz is ";
// board measure
const char *txtBoardBusy = "Another measurement is currently running.";
const char *txtImpedance = "Impedance (polar): ";
// board read
const char *txtNoReadWhileBusy = "Data can only be read after the measurement is finished.";
const char *txtOutOfMemory = "Not enough memory to send all data, try binary format or use fewer points.";

// Words ----------------------------------------------------------------------
const char *txtEnabled = "enabled";
const char *txtDisabled = "disabled";
const char *txtOn = "on";
const char *txtOff = "off";
const char *txtOf = " of ";
const char *txtOK = "OK";

// ----------------------------------------------------------------------------

#endif /* STRINGS_EN_H_ */
