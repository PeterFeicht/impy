function [ freq, out ] = impy_read( comport, varargin )
%IMPY_READ Read measurement data from board
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%       format (optional) - Format of the data (can be 'polar', 'cartesian' or 'raw')
%   Returns:
%       freq - Vector with frequencies
%       data - Either a 2xN array with magnitude and phase values (for polar format), or a 1xN array of complex values
%              (for cartesian format)

%% Process arguments
format = 'APHFS';
polar = true;
raw = false;

if nargin == 2
    if strcmp(varargin{1}, 'cartesian')
        format = 'ACHFS';
        polar = false;
    elseif strcmp(varargin{1}, 'raw')
        raw = true;
    elseif ~strcmp(varargin{1}, 'polar')
        warning('Unknown format "%s", using polar instead.', varargin{1});
    end
elseif nargin ~= 1
    error('Only two arguments expected.');
end

%% Send command and read header from board
if ~raw
    fprintf(comport, '@board read --format=%s\n', format);
else
    fprintf(comport, '@board read --format=%s --raw\n', format);
end

line = fgetl(comport);
if isempty(line)
    error('Error reading from serial device, check connection.');
elseif isempty(strfind(line, 'Frequency'))
    freq = [];
    out = [];
    error(line);
end

%% Parse received data
k = 1;
line = fgetl(comport);
while ~isempty(line)
    if raw
        data = sscanf(line, '%u %d %d');
        freq(k) = data(1);
        out(:,k) = data(2:3);
    else
        data = sscanf(line, '%u %g %g');
        freq(k) = data(1);

        if polar
            out(:,k) = data(2:3);
        else
            out(k) = data(2) + 1i * data(3);
        end
    end
    
    k = k + 1;
    line = fgetl(comport);
end

end

