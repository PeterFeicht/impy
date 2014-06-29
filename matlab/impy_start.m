function [ ] = impy_start( comport, port )
%IMPY_START Start a sweep on specified port
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%       port - The port number to measure

fprintf(comport, '@board start %d\n', port);

str = fgetl(comport);
if ~strcmp(str, 'OK')
    error(str);
end

end

