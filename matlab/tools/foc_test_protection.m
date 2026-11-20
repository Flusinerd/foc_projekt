function ok = foc_test_protection()
%FOC_TEST_PROTECTION  Minimaler Simulationstest der Ueberspannungsabschaltung (Schritt C).
%
%   foc_startup('datasheet'); ok = foc_test_protection()
%
%   Simuliert app_speed 0,3 s mit der Sim-Variante des Zwischenkreises (Konstante Udc_Sim_V in
%   focLib/Input Controller, nur im Speicher geaendert) und prueft den Ausgang Overvoltage_Sig:
%     31 V, ov_enable = 1  -> true    (ueber Udc_max = 30 V)
%     24 V, ov_enable = 1  -> false
%     31 V, ov_enable = 0  -> false   (legacy-Verhalten)
%   Modelle werden nicht gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
load_system(fullfile(root, 'models', 'focLib.slx')); set_param('focLib', 'Lock', 'off');
load_system(fullfile(root, 'models', 'app_speed.slx'));
ic = 'app_speed/Input Controller';
n = str2double(get_param('focLib/Input Controller/Overvoltage_Sig', 'Port'));
ph = get_param(ic, 'PortHandles');
set_param(ph.Outport(n), 'DataLogging', 'on', 'DataLoggingNameMode', 'Custom', 'DataLoggingName', 'ov');
cases = {31, 1, true; 24, 1, false; 31, 0, false};
lim = evalin('base', 'ctrl_limits');
ok = true;
for k = 1:size(cases, 1)
    set_param('focLib/Input Controller/Udc_Sim_V', 'Value', num2str(cases{k, 1}));
    l = lim; l.ov_enable = cases{k, 2}; assignin('base', 'ctrl_limits', l);
    out = sim('app_speed', 'StopTime', '0.3', 'SignalLogging', 'on', 'SignalLoggingName', 'logsout', ...
              'ReturnWorkspaceOutputs', 'on');
    v = out.get('logsout').getElement('ov').Values.Data;
    pass = isequal(logical(v(end)), cases{k, 3});
    ok = ok && pass;
    fprintf('Udc = %g V, ov_enable = %d: Overvoltage_Sig = %d (erwartet %d) %s\n', cases{k, 1}, cases{k, 2}, ...
        v(end), cases{k, 3}, ternary(pass, 'OK', 'FEHLER'));
end
assignin('base', 'ctrl_limits', lim);
close_system('app_speed', 0); close_system('focLib', 0);
if ok, fprintf('foc_test_protection: ALL PASSED\n'); else, fprintf('foc_test_protection: FAILED\n'); end
end

function s = ternary(c, a, b), if c, s = a; else, s = b; end, end
