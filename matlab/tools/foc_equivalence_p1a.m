function r = foc_equivalence_p1a()
%FOC_EQUIVALENCE_P1A  app_speed gegen impmodel mit P1a-Reglern, ohne focLib zu speichern.
%
%   foc_startup('legacy_f2838'); r = foc_equivalence_p1a()
%
%   Uebernimmt P, I, UseKiTs und Grenzen der drei PID-Bloecke aus impmodel nur im Speicher
%   in focLib. Beim Drehzahlregler bleiben die Grenzen als ctrl_limits.Iq_min/Iq_max stehen
%   (legacy: -0,5 / 1), damit die Parametrierung mitgeprueft wird. Dann foc_check_equivalence.
%   Erwartung: alle Abweichungen 0. focLib wird am Ende ohne Speichern geschlossen.

root = fileparts(fileparts(mfilename('fullpath')));
if ~bdIsLoaded('impmodel'), load_system(fullfile(root, 'models', 'legacy', 'impmodel.slx')); end
if ~bdIsLoaded('focLib'), load_system(fullfile(root, 'models', 'focLib.slx')); end
set_param('focLib', 'Lock', 'off');
pids = {'Speed Control/PI_Controller_Speed/PID Controller', ...
        'Torque Controller/FOC/Current_Controllers/PI_Controller_Id/PID Controller', ...
        'Torque Controller/FOC/Current_Controllers/PI_Controller_Iq/PID Controller'};
for k = 1:numel(pids)
    src = ['impmodel/' pids{k}]; dst = ['focLib/' pids{k}];
    if getSimulinkBlockHandle(src) == -1, src = findLegacy(pids{k}); end
    p = {'P', 'I', 'UseKiTs', 'UpperSaturationLimit', 'LowerSaturationLimit'};
    if k == 1, p = p(1:3); end
    for j = 1:numel(p), set_param(dst, p{j}, get_param(src, p{j})); end
    fprintf('%s (Speicher): P=%s I=%s UseKiTs=%s [%s %s]\n', dst, get_param(dst, 'P'), ...
        get_param(dst, 'I'), get_param(dst, 'UseKiTs'), get_param(dst, 'LowerSaturationLimit'), ...
        get_param(dst, 'UpperSaturationLimit'));
end
r = foc_check_equivalence();
close_system('focLib', 0);
end

function src = findLegacy(rel)
% Bloecke in impmodel liegen evtl. eine Ebene tiefer: per Endpfad suchen.
parts = strsplit(rel, '/');
c = find_system('impmodel', 'LookUnderMasks', 'all', 'FollowLinks', 'on', 'Name', parts{end});
c = c(endsWith(c, [parts{end-1} '/' parts{end}]));
if numel(c) ~= 1, error('foc:eq', 'PID-Block in impmodel nicht eindeutig: %s', rel); end
src = c{1};
end
