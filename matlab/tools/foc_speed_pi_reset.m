function foc_speed_pi_reset()
%FOC_SPEED_PI_RESET  A2: I-Anteil des Drehzahlreglers im Freemode zuruecksetzen (schaltbar).
%
%   foc_speed_pi_reset()
%
%   Mit Untergrenze ctrl_limits.Iq_min = 0 haelt die Anti-Windup-Begrenzung einen kleinen
%   positiven I-Rest; im Freemode (Regelfehler genullt) gibt der Regler ihn als Iq_ref aus
%   (bis 0,024 PU, MIL-Test SW_ANF_05 verletzt). Neu im Block Speed Control/PI_Controller_Speed:
%     PID Controller: ExternalReset = level (Integrator = 0, solange Reset-Eingang ~= 0)
%     Reset-Eingang  = FreemodeEnabled? AND ctrl_limits.reset_I_freemode
%   legacy_f2838: reset_I_freemode = 0 -> bitgleich zum Altmodell; datasheet: 1.
%   Vorher/nachher werden ausgegeben; focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');
S = [lib '/Speed Control/PI_Controller_Speed'];
pid = [S '/PID Controller'];
fm = find_system(S, 'SearchDepth', 1, 'BlockType', 'Inport', 'Port', '3');
fm = fm{1};
fprintf('%s\n  vorher:  ExternalReset=%s, Freemode-Eingang "%s"\n', pid, get_param(pid, 'ExternalReset'), ...
    strtrim(get_param(fm, 'Name')));
if ~strcmp(get_param(pid, 'ExternalReset'), 'none')
    fprintf('  bereits mit Reset, keine Aenderung.\n');
    return;
end

set_param(pid, 'ExternalReset', 'level');
p = get_param(pid, 'Position');
cst = [S '/Reset_I_Freemode_Enable'];
add_block('simulink/Sources/Constant', cst, 'Value', 'ctrl_limits.reset_I_freemode', ...
    'OutDataTypeStr', 'boolean', 'Position', [p(1) - 200, p(4) + 60, p(1) - 140, p(4) + 80]);
andb = [S '/Reset_I_And'];
add_block('simulink/Logic and Bit Operations/Logical Operator', andb, 'Operator', 'AND', ...
    'AllPortsSameDT', 'off', 'OutDataTypeStr', 'boolean', ...
    'Position', [p(1) - 100, p(4) + 20, p(1) - 60, p(4) + 70]);
phA = get_param(andb, 'PortHandles');
add_line(S, get_param(fm, 'PortHandles').Outport(1), phA.Inport(1), 'autorouting', 'on');
add_line(S, get_param(cst, 'PortHandles').Outport(1), phA.Inport(2), 'autorouting', 'on');
phP = get_param(pid, 'PortHandles');
add_line(S, phA.Outport(1), phP.Inport(2), 'autorouting', 'on');
save_system(lib);
fprintf('  nachher: ExternalReset=%s, Reset <- %s AND ctrl_limits.reset_I_freemode\n', ...
    get_param(pid, 'ExternalReset'), strtrim(get_param(fm, 'Name')));
fprintf('focLib gespeichert.\n');
end
