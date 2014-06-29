function [ ] = impy_calibrate( comport, resistor )
%IMPY_CALIBRATE Perform a clibration with the specified calibration resistor
%   This function waits for the calibration to finish, which can take some time with low frequencies.
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%       resistor - Value of the calibration resistor in Ohm

fprintf(comport, '@board calibrate %d\n', resistor);

str = fgetl(comport);
if ~strcmp(str, 'OK')
    if isempty(str)
        error('Error reading from serial device, check connection.');
    else
        error(str);
    end
end

end

