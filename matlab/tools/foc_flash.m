function foc_flash(model, freigabe)
%FOC_FLASH  Anwendungsmodell bauen und ueber JTAG (XDS100) ins Flash laden und starten.
%
%   foc_startup(<Profil>); foc_flash('app_speed', 'FREIGABE')
%
%   Nur nach ausdruecklicher Freigabe des Menschen in derselben Sitzung (AGENTS.md Regel 4).
%   Die Build action wird nur im Speicher auf 'Build, load and run' gestellt; das Modell wird
%   danach ohne Speichern geschlossen. Nach dem Flashen die Platine neu starten (CCS 21 stellt
%   den Takt nicht wieder her).

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 2 || ~ischar(freigabe) || ~strcmp(freigabe, 'FREIGABE')
    error('foc:flash', 'Flashen nur mit foc_flash(model, ''FREIGABE'') nach Freigabe des Menschen.');
end
if isempty(model), model = 'app_speed'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
if strcmp(get_param(model, 'Dirty'), 'on')
    error('foc:flash', 'Modell %s hat ungespeicherte Aenderungen - Abbruch.', model);
end
cs = getActiveConfigSet(model);
prof = '?';
if evalin('base', 'exist(''foc_active_profile'', ''var'')'), prof = evalin('base', 'foc_active_profile'); end
codertarget.data.setParameterValue(cs, 'Runtime.BuildAction', 'Build, load and run');
fprintf('foc_flash: %s, Profil %s, DeviceID=%s, Build, load and run\n', model, prof, ...
        codertarget.data.getParameterValue(cs, 'Runtime.DeviceID'));
try
    slbuild(model);
catch err
    close_system(model, 0);
    rethrow(err);
end
close_system(model, 0);
fprintf('Geflasht. Platine jetzt neu starten.\n');
end
