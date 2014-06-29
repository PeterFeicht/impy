%liest frequenz und impedanz
function [ freq, data ] = readdata( fnam )

f = fopen(fnam,'rt');
s = fgets(f);

while isempty(strfind(s, 'Frequency'))
    s = fgets(f);
end;

k = 1;
while ~feof(f)
    d = textscan(f, '%n %n %n\r\n');
    if ~isempty(d{1})
        freq(k) = d{1};
        data(k) = d{2} + 1i*d{3};
    else
        break;
    end;
    
    k = k + 1;    
end;

fclose(f);
