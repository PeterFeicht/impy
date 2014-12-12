%% Clean up
clear all;
clc;

%% Sweep parameters
sweep = struct;
sweep.start = 10e3;         % Start frequency in Hz
sweep.stop = 20e3;          % Stop frequency in Hz
sweep.steps = 100;          % Number of frequency increments
sweep.voltage = 400;        % Output voltage in mV
sweep.settl = 160;          % Number of settling cycles
sweep.avg = 5;              % Number of averages per frequency point (optional)
sweep.gain = false;         % Enable or disable x5 input gain stage
sweep.feedback = 100;       % Feedback resistor value in Ohm
cal = 100;                  % Calibration resistor used
port = 0;

%% Open COM port
try
    impy = serial('COM6', 'BaudRate', 115200);
    % IMPORTANT: Set the Terminator property to either 'CR/LF' or { 'CR/LF', 'LF' }
    % for proper operation
    % Timeout needs to be set high when low frequencies are used, because in this
    % case calibration can take a long time
    % I think InputBufferSize needs to be large enough to hold all data sent by
    % 'board read', so 128KB should be plenty
    set(impy, 'Terminator', { 'CR/LF', 'LF' }, 'Timeout', 120, ...
        'InputBufferSize', 128*1024);
    fopen(impy);
catch ex
    % In case of error, close the port
    fclose(impy);
    delete(impy);
    clear impy;
    rethrow(ex);
end

%% Start sweep and wait for completion
impy_setsweep(impy, sweep);
impy_calibrate(impy, cal);
impy_start(impy, port);

while ~impy_poll(impy)
    pause(1);
end

%% Read results
[freq, data] = impy_read(impy, 'polar');
[~, raw] = impy_read(impy, 'raw');
freq = freq / 1e3;

%% Close COM port
fclose(impy);
delete(impy);
clear impy;
