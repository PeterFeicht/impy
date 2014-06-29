function [ sweep ] = impy_getall( comport )
%IMPY_GETALL Get the current sweep parameters from the board
%   Arguments:
%       comport - Serial port object that has been 'fopen'ed
%   Returns:
%       sweep - structure with current sweep parameters

fprintf(comport, '@board get all');
sweep = struct;

line = fgetl(comport);
while ~isempty(line)
    % 'board get all' prints key value pairs separated by '='
    split = strsplit(line, '=');
    
    if length(split) ~= 2
        warning('Encountered odd line in response from board, ignored: %s', line);
        line = fgetl(comport);
        continue;
    end
    
    % Try conversion to number (most values are numbers)
    [num, status] = str2num(split{2});
    if status
        sweep.(split{1}) = num;
    else
        % Not a number, try conversion to flag
        if any(strcmp(split{2}, {'enabled', 'on'}))
            sweep.(split{1}) = true;
        elseif any(strcmp(split{2}, {'disabled', 'off'}))
            sweep.(split{1}) = false;
        else
            % Not a flag, just save the string (shouldn't happen)
            sweep.(split{1}) = split{2};
        end
    end
    
    line = fgetl(comport);
end

end

