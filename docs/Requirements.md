Device Requirements
===================

The following lists contain some of the features the finished impedance
spectrometer needs to have or can have. This list is subject to changes as the
project progresses, but probably not much.

Must have
---------

Features the finished device must have, otherwise it can't be considered
finished:

 * PCB that plugs onto the STM32F4 Discovery board and contains the AD5933
   and peripherals.
 * Serial interface (via USB) of the AD5933 to a PC.
 * User interface in Matlab to control the device and handle the measurement data.


Nice to have
------------

Features that would be nice to have and will (more or less) likely be added to
the finished device, in order of decreasing desirability:

 * Extended measurement range of the AD5933 by means of an op-amp circuit.
 * Analog MUX on board to be able to measure multiple channels at once
   (and for calibration purposes).
 * EEPROM or similar on board to save configuration data.
 * Ethernet interface on board.
 * Additional memory on board to be able to take measurements with many points.
 * Support for multiple configurations (sweep range, voltage, etc).


Future extensions
-----------------

Features that would also be nice to have but are probably too much work to add
right away:

 * Flash memory on board to be able to save measurements on the device itself.
 * LXI compliance of the Ethernet interface.
 * Graphical user interface, either using the TI AM335x Starter Kit,
   or a display directly attached to the STM32F4.
 * USB host port to save measurements on a USB drive
   (only really useful with a GUI).
