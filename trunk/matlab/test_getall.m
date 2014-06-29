% Test script: get current sweep parameters from board using impy_getall
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
