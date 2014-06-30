function [ finished, numpoints ] = impy_poll( comport )
%IMPY_POLL Determine whether a running sweep has finished
%   Note that this function does not return true if, for example, a temperature or calibration measurement was performed
%   since the sweep finished. It's only meant to be used while a sweep is running.
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%   Returns:
%       finished - Logical indicating whether measurement has finished
%       numpoints - Number of points measured, if finished is true

fprintf(comport, '@board status');
status = fgetl(comport);

% Status can have more lines, we only need the first one, so eat up the following ones
while get(comport, 'BytesAvailable') > 0
    fgetl(comport);
end

if isempty(status)
    error('Error reading from serial device, check connection.');
elseif ~isempty(strfind(status, 'finished'))
    finished = true;
    split = strsplit(status, ': ');
    [numpoints, status] = str2num(split{2});
    if ~status
        warning('Odd reponse from the board, I don''t know what to do: status');
    end
else
    finished = false;
    numpoints = 0;
end

end

