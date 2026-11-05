function foc_apply_fixes(which, libFile)
%FOC_APPLY_FIXES  P1b: Korrekturen an den Reglern in focLib.slx (einzeln schaltbar).
%
%   foc_apply_fixes('all')
%   foc_apply_fixes({'current_pi'})          nur Stromregler
%   foc_apply_fixes({'speed_pi'})            nur Drehzahlregler
%
%   current_pi  Stromregler d/q:  P = Kp, I = Ki [1/s] mit UseKiTs = off,
%               Ausgang +/-1 statt [0, 1].
%               Vorher: P = Kp - 0,0002 (Handkorrektur), I = Ki_texas*Ts mit UseKiTs = on,
%               Untergrenze 0 -> keine negative d/q-Spannung moeglich.
%   speed_pi    Drehzahlregler:  I = Ki_speed [1/s] mit UseKiTs = off.
%               Vorher: I = Ki_speed_texas*Ts_speed (Ts/Tn ohne Kp, dann nochmals * Ts)
%               -> I-Anteil um Groessenordnungen zu klein, Regler arbeitet praktisch als P.
%
%   Die Korrekturen setzen Reglerwerte mit der Bedeutung aus foc_design_controllers
%   voraus. Danach mit foc_startup('datasheet') simulieren, NICHT mit legacy_f2838.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(which), which = 'all'; end
if nargin < 2 || isempty(libFile), libFile = fullfile(root, 'models', 'focLib.slx'); end
if ischar(which)
    if strcmp(which, 'all'), which = {'current_pi', 'speed_pi'}; else, which = {which}; end
end

lib = 'focLib';
if ~bdIsLoaded(lib), load_system(libFile); end
set_param(lib, 'Lock', 'off');

base = [lib '/Torque Controller/FOC/Current_Controllers'];
for k = 1:numel(which)
    switch which{k}
        case 'current_pi'
            setPid([base '/PI_Controller_Id/PID Controller'], 'focp_tune_cur_Kp', 'focp_tune_cur_Ki', '1', '-1');   % FOCP
            setPid([base '/PI_Controller_Iq/PID Controller'], 'focp_tune_cur_Kp', 'focp_tune_cur_Ki', '1', '-1');
        case 'speed_pi'
            setPid([lib '/Speed Control/PI_Controller_Speed/PID Controller'], ...
                'focp_tune_spd_Kp', 'focp_tune_spd_Ki', 'focp_tune_iq_max', 'focp_tune_iq_min');   % A2, Iq_max, FOCP
        otherwise
            error('foc:fix', 'Unbekannte Korrektur "%s"', which{k});
    end
end
save_system(lib);
fprintf('focLib gespeichert. Jetzt: foc_startup(''datasheet''), dann app_speed simulieren.\n');
end

function setPid(blk, P, I, upper, lower)
if getSimulinkBlockHandle(blk) == -1
    error('foc:fix', ['Block nicht gefunden: %s\n' ...
        'Pfad in foc_apply_fixes.m anpassen (Namen mit Zeilenumbruechen beachten).'], blk);
end
before = sprintf('P=%s, I=%s, UseKiTs=%s, Limits=[%s, %s]', get_param(blk, 'P'), get_param(blk, 'I'), ...
    get_param(blk, 'UseKiTs'), get_param(blk, 'LowerSaturationLimit'), get_param(blk, 'UpperSaturationLimit'));
set_param(blk, 'P', P, 'I', I, 'UseKiTs', 'off', ...
    'UpperSaturationLimit', upper, 'LowerSaturationLimit', lower);
fprintf('%s\n  vorher:  %s\n  nachher: P=%s, I=%s, UseKiTs=off, Limits=[%s, %s]\n', ...
    blk, before, P, I, lower, upper);
end
