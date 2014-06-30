% Test script: get current sweep parameters from board using impy_getall
% 
% The output of the 'board get all' command gets all relevant sweep parameters with one command and is designed
% specifically for parsing (key=value lines). The function impy_getall sends this command and reads all key/value pairs
% into a structure. The structure obtained this way can be customized to the measurement need at hand and then used with
% the impy_setsweep function to send the sweep parameters to the board.

%% Clean up
clear all;
clc;

%% Open COM port
impy = serial('COM6', 'BaudRate', 115200);
set(impy, 'Terminator', { 'CR/LF', 'LF' }, 'Timeout', 10);
fopen(impy);

%% Get sweep parameters
sweep = impy_getall(impy);
disp('Test impy_getall:');
disp(sweep);

%% Close COM port
fclose(impy);
delete(impy);
clear impy;
