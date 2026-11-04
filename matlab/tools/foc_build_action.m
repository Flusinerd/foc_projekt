function foc_build_action(model, action)
%FOC_BUILD_ACTION  Build action des Anwendungsmodells dauerhaft setzen (Standard: nur 'Build').
%
%   foc_build_action()                      app_speed -> 'Build' (kein Laden auf die Hardware)
%   foc_build_action(model, action)
%
%   Anlass: 'Build, load and run' war im Modell gespeichert; ein reines slbuild hat dadurch
%   ungewollt geflasht (reports/08_S5_Fehler_Flash_2026-11-04.md). Flashen nur mit foc_flash.
%   Ausgabe vorher/nachher; nur Runtime.BuildAction darf sich aendern; Modell wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(model), model = 'app_speed'; end
if nargin < 2 || isempty(action), action = 'Build'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
cs = getActiveConfigSet(model);
keys = {'Runtime.BuildAction', 'Runtime.DeviceID', 'Runtime.LoadCommandArg', 'Runtime.FlashLoad', ...
        'Clocking.UseInternalOsc', 'Clocking.OSCCLK', 'Clocking.PLLCR', 'Clocking.ClosestCpuClock'};
before = readAll(cs, keys);
fprintf('%s\n  vorher:  %s, ExtMode=%s\n', model, strjoin(before, ', '), get_param(model, 'ExtMode'));
if strcmp(codertarget.data.getParameterValue(cs, 'Runtime.BuildAction'), action)
    fprintf('  BuildAction bereits "%s", keine Aenderung.\n', action);
    return;
end
codertarget.data.setParameterValue(cs, 'Runtime.BuildAction', action);
after = readAll(cs, keys);
fprintf('  nachher: %s, ExtMode=%s\n', strjoin(after, ', '), get_param(model, 'ExtMode'));
if ~isequal(before(2:end), after(2:end))
    error('foc:buildaction', 'Andere Einstellungen haben sich mitgeaendert - nicht gespeichert.');
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
