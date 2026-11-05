function foc_speed_ref_deadband()
%FOC_SPEED_REF_DEADBAND  I-Anteil des Drehzahlreglers bei Sollwert nahe 0 zuruecksetzen.
%
%   foc_speed_ref_deadband()
%
%   S5-Befund: Bei Poti 0 liefert der ADC einen kleinen positiven Sollwert. Der I-Anteil integriert
%   ihn, bis das Moment die Reibung ueberwindet; der Motor ruckt alle ~20 s einen Hall-Sektor
%   weiter (wegen Iq_min = 0 kann er nicht nach unten ausgleichen).
%   Neu in focLib/Speed Control/PI_Controller_Speed:
%     Reset-Eingang des PID = (bisheriger Freemode-Reset) OR (|N_ref| < ctrl_limits.N_ref_min)
%   legacy_f2838: N_ref_min = 0 -> Bedingung nie wahr, bitgleich; datasheet: 0,005 PU (20 rpm).
%   Vorher/nachher werden ausgegeben; focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');
S = [lib '/Speed Control/PI_Controller_Speed'];
pid = [S '/PID Controller'];
andb = [S '/Reset_I_And'];
ph = get_param(pid, 'PortHandles');
src = get_param(get_param(ph.Inport(end), 'Line'), 'SrcBlockHandle');
fprintf('%s\n  vorher:  Reset-Eingang <- %s\n', pid, get_param(src, 'Name'));
if getSimulinkBlockHandle([S '/Reset_I_Or']) ~= -1
    fprintf('  bereits mit Totzone, keine Aenderung.\n');
    return;
end
nref = find_system(S, 'SearchDepth', 1, 'BlockType', 'Inport', 'Name', 'N_ref');
nref = nref{1};

q = get_param(andb, 'Position');
add_block('simulink/Math Operations/Abs', [S '/N_ref_abs'], 'Position', [q(1) - 160, q(4) + 40, q(1) - 130, q(4) + 70]);
add_block('simulink/Logic and Bit Operations/Compare To Constant', [S '/N_ref_small'], ...
    'relop', '<', 'const', 'ctrl_limits.N_ref_min', 'OutDataTypeStr', 'boolean', ...
    'Position', [q(1) - 100, q(4) + 40, q(1) - 40, q(4) + 70]);
add_block('simulink/Logic and Bit Operations/Logical Operator', [S '/Reset_I_Or'], 'Operator', 'OR', ...
    'AllPortsSameDT', 'off', 'OutDataTypeStr', 'boolean', 'Position', [q(3) + 20, q(2), q(3) + 60, q(4) + 60]);
delete_line(get_param(ph.Inport(end), 'Line'));
add_line(S, get_param(nref, 'PortHandles').Outport(1), get_param([S '/N_ref_abs'], 'PortHandles').Inport(1), 'autorouting', 'on');
add_line(S, 'N_ref_abs/1', 'N_ref_small/1', 'autorouting', 'on');
add_line(S, 'Reset_I_And/1', 'Reset_I_Or/1', 'autorouting', 'on');
add_line(S, 'N_ref_small/1', 'Reset_I_Or/2', 'autorouting', 'on');
add_line(S, get_param([S '/Reset_I_Or'], 'PortHandles').Outport(1), ph.Inport(end), 'autorouting', 'on');
save_system(lib);
fprintf('  nachher: Reset-Eingang <- Reset_I_Or (Freemode-Reset OR |N_ref| < ctrl_limits.N_ref_min)\n');
fprintf('focLib gespeichert.\n');
end
