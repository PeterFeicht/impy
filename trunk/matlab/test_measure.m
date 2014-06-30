% Test script: make a measurement with specified parameters and plot magnitude and phase
%
% The basic procedure is:
%  1) Get a structure with current sweep parameters using `impy_getall` and modify it, or make your own one, with the
%     desired sweep parameters
%  2) Send sweep parameters to board using `impy_setsweep`
%  3) Calibrate with a suitable calibration resistor using `impy_calibrate`
%  4) Start a sweep on any port with `impy_start`
%  5) Wait for the sweep to complete using `impy_poll`
%  6) Read data in desired format using `impy_read`
%  7) Repeat from 4) for other ports if needed, calibration is only necessary when sweep parameters change

%% Clean up
clear all;
clc;

%% Sweep parameters
% See also test_getall
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
impy = serial('COM6', 'BaudRate', 115200);
% IMPORTANT: Set the Terminator property to either 'CR/LF' or { 'CR/LF', 'LF' } for proper operation
% Timeout needs to be set high when low frequencies are used, because in this case calibration can take a long time
% I think InputBufferSize needs to be large enough to hold all data sent by 'board read', so 128KB should be big enough
set(impy, 'Terminator', { 'CR/LF', 'LF' }, 'Timeout', 120, 'InputBufferSize', 128*1024);
fopen(impy);


%% Start sweep and wait for completion
impy_setsweep(impy, sweep);
impy_calibrate(impy, cal);
impy_start(impy, port);

while ~impy_poll(impy)
    pause(1);
end

[freq, data] = impy_read(impy, 'polar');
[~, raw] = impy_read(impy, 'raw');
freq = freq / 1e3;


%% Close COM port
fclose(impy);
delete(impy);
clear impy;


%% Plot data
close all;
scrsz = get(0, 'ScreenSize');

% Set this variable to compare with data from Agilent 4294A impedance analyzer
%file = 'data/LCP.txt';
if exist('file', 'var') ~= 0
    [f2, mag2] = readdata(file);
    [~, phase2] = readdata_p(file);
    f2 = f2 / 1e3;
end

% Plot polar data
figure('Name', 'LC_par', 'NumberTitle', 'off', 'Position', ...
    [scrsz(3)/20 0.25*scrsz(4) scrsz(3)/2 scrsz(4)*5/8]);
% magnitude
subplot(2,1,1);
plot(freq, data(1,:), 'k-', 'Linewidth', 1.5);
grid on;
hold on;
if exist('file', 'var') ~= 0
    plot(f2, mag2, 'k--', 'LineWidth', 1.5);
end
set(gca, 'XLim', [min(freq) max(freq)]);
xlabel('Frequency in kHz');
ylabel('Impedance in Ohm');
title('Magnitude');
% phase
subplot(2,1,2);
plot(freq, 180/pi * data(2,:), 'k-', 'Linewidth', 1.5);
grid on;
hold on;
if exist('file', 'var') ~= 0
    plot(f2, phase2, 'k--', 'LineWidth', 1.5);
end
set(gca, 'XLim', [min(freq) max(freq)]);
xlabel('Frequency in kHz');
ylabel('Phase in °');
title('Phase');

% Plot raw data
figure('Name', 'LC_par_raw', 'NumberTitle', 'off', 'Position', ...
    [9*scrsz(3)/20 0.05*scrsz(4) scrsz(3)/2 scrsz(4)*5/8]);
plot(freq, raw(1,:) / 1e3, 'k-', 'LineWidth', 1.5);
grid on;
hold on;
plot(freq, raw(2,:) / 1e3, 'k--', 'LineWidth', 1.5);
xlabel('Frequency in kHz');
ylabel('DFT Value in 10^3');
legend('Real', 'Imaginary', 'Location', 'SouthEast');
title('Raw data');
