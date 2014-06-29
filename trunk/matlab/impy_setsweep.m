function [ ] = impy_setsweep( comport, sweep )
%IMPY_SETSWEEP Send the specified sweep parameters to the board
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%       sweep - Sweep specifications, use impy_getall to obtain a structure with current values

%% Check parameters
if ~isstruct(sweep)
    error('sweep needs to be a structure.');
end

required = {'start', 'stop', 'steps', 'settl', 'voltage', 'feedback', 'gain'};
fields = isfield(sweep, required);
if ~all(fields)
    error('Required fields missing from sweep: %s', strjoin(required(~fields)));
end

%% Build command string
curr = impy_getall(comport);

cmd = cell(1, 8);
cmd{1} = '@board set';

% Frequency range
cmd{2} = sprintf('--start=%d', sweep.start);
cmd{3} = sprintf('--stop=%d', sweep.stop);

if sweep.start >= curr.stop
    tmp = cmd{3};
    cmd{3} = cmd{2};
    cmd{2} = tmp;
end

% Steps, voltage, feedback resistor
cmd{4} = sprintf('--steps=%d --voltage=%d --feedback=%d', ...
    sweep.steps, sweep.voltage, sweep.feedback);

% Settling time
mult = 1;
num = sweep.settl;
while num > 511
    mult = mult * 2;
    num = num / 2;
end
num = floor(num);
if mult > 4
    mult = 4;
    warning('Settling cycles too high, set to %d', num * 4);
end
cmd{5} = sprintf('--settl=%dx%d', num, mult);

% Input gain stage
if sweep.gain
    str = 'on';
else
    str = 'off';
end
cmd{6} = sprintf('--gain=%s', str);

% Averaging (optional)
if isfield(sweep, 'avg')
    cmd{7} = sprintf('--avg=%d', sweep.avg);
end

% Autoranging (optional)
if isfield(sweep, 'autorange')
    if sweep.autorange
        str = 'on';
    else
        str = 'off';
    end
    cmd{8} = sprintf('--autorange=%s', str);
end

% Build command
cmd = strjoin(cmd(~cellfun('isempty', cmd)));

%% Send command string and handle errors
fprintf(comport, cmd);

% Board doesn't normally respond to set command, only on error
if get(comport, 'BytesAvailable') > 0
    while get(comport, 'BytesAvailable') > 0
        warning(fgetl(comport));
    end
    error('Errors received from board, check sweep parameters.');
end

end

