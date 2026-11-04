function foc_device_f28386d(model)
%FOC_DEVICE_F28386D  DeviceID des Anwendungsmodells auf den bestueckten TMS320F28386D stellen.
%
%   foc_device_f28386d()           matlab/models/app_speed.slx
%   foc_device_f28386d(model)
%
%   Bisher (Altmodell der MA): Runtime.DeviceID = F28388D. Die CarPediem-Platine traegt einen
%   F28386D. Das C2000 Microcontroller Blockset R2023a bietet F28386D an und behandelt ihn in
%   Linkerdatei (c2838x.cmd), Speicherkarte (MW_F2838x_MemoryMap.h), Init- und IPC-Code wie
%   F28388D; beide nutzen dieselbe Ladekonfiguration f28388D.ccxml. Einziger Unterschied der
%   Bausteine: F28386D hat kein EtherCAT (ESCSS), das app_speed nicht nutzt
%   (reports/08_DeviceID_2026-11-04.md).
%   Gesetzt wird nur Runtime.DeviceID; der Auswahl-Callback der GUI (DeviceIDSelectionCallback)
%   wuerde bei 200 MHz weder Takt noch Ladekonfiguration aendern. Takt und Ladekonfiguration
%   werden zur Kontrolle vorher/nachher ausgegeben; Modell wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(model), model = 'app_speed'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
cs = getActiveConfigSet(model);
keys = {'Runtime.DeviceID', 'Runtime.LoadCommandArg', 'Clocking.UseInternalOsc', 'Clocking.OSCCLK', ...
        'Clocking.PLLCR', 'Clocking.DIVSEL', 'Clocking.ClosestCpuClock'};
before = readAll(cs, keys);
fprintf('%s\n  vorher:  %s\n', model, strjoin(before, ', '));
newId = 'F28386D';
if strcmp(codertarget.data.getParameterValue(cs, 'Runtime.DeviceID'), newId)
    fprintf('  DeviceID bereits %s, keine Aenderung.\n', newId);
    return;
end
codertarget.data.setParameterValue(cs, 'Runtime.DeviceID', newId);
after = readAll(cs, keys);
fprintf('  nachher: %s\n', strjoin(after, ', '));
if ~isequal(before(2:end), after(2:end))
    error('foc:device', 'Takt oder Ladekonfiguration haben sich mitgeaendert - nicht gespeichert.');
end
save_system(model);
fprintf('%s gespeichert.\n', model);
end

function v = readAll(cs, keys)
v = cell(size(keys));
for k = 1:numel(keys)
    x = codertarget.data.getParameterValue(cs, keys{k});
    if isnumeric(x) || islogical(x), x = num2str(x); end
    v{k} = sprintf('%s=%s', keys{k}, x);
end
end
