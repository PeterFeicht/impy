function [ ] = impy_start( comport, port )
%IMPY_START Start a sweep on specified port
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%       port - The port number to measure

fprintf(comport, '@board start %d\n', port);

str = fgetl(comport);
if isempty(str)
    error('Error reading from serial device, check connection.');
elseif ~strcmp(str, 'OK')
    error(str);
end

end

