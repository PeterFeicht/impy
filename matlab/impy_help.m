function [ ] = impy_help( comport, varargin )
%IMPY_HELP Read help text from the board and display
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%       topic (optional) - The help topic to get
%   Returns:
%       nothing

%% Process arguments
if nargin == 1
    cmd = { '@help' };
elseif nargin == 2
    if ~ischar(varargin{1})
        error('Argument needs to be a string.');
    end
    cmd = { '@help', varargin{1} };
else
    error('One or two arguments expected.');
end

%% Send command and read response from board
fprintf(comport, strjoin(cmd));

text = fgetl(comport);
disp(text);
while get(comport, 'BytesAvailable') > 0
    text = fgetl(comport);
    disp(text);
end

end

