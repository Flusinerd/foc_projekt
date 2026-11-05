function foc_current_pi_reset()
%FOC_CURRENT_PI_RESET  I-Anteile der Stromregler (d/q) im Freemode zuruecksetzen (schaltbar).
%
%   foc_current_pi_reset()
%
%   Im Freemode ist die PWM aus, die Stromregler rechnen aber weiter: Der I-Anteil integriert den
%   Messoffset bis an die Spannungsgrenze (+-1 PU). Beim Freigeben liegt diese Spannung sofort an;
%   gemessen wurde ein Stromstoss von -31 A gegen die Sollrichtung
%   (reports/08_FOCP_Einbau_Plan_2026-11-04.md). Neu in focLib/Torque Controller:
%     Goto "FreemodeSig_I" (scoped) am Eingang FreemodeSig, Goto Tag Visibility im Torque Controller
%     PI_Controller_Id/Iq: PID Controller ExternalReset = level,
%       Reset-Eingang = From FreemodeSig_I AND ctrl_limits.reset_I_cur_freemode
%   legacy_f2838: reset_I_cur_freemode = 0 -> bitgleich zum Altmodell; datasheet: 1.
%   Vorher/nachher werden ausgegeben; focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');
tc = [lib '/Torque Controller'];
cc = [tc '/FOC/Current_Controllers'];
axes = {'Id', 'Iq'};
for k = 1:numel(axes)
    pid = sprintf('%s/PI_Controller_%s/PID Controller', cc, axes{k});
    fprintf('%s\n  vorher:  ExternalReset=%s\n', pid, get_param(pid, 'ExternalReset'));
end
if getSimulinkBlockHandle([tc '/FreemodeSig_I_Goto']) ~= -1
    fprintf('  bereits mit Reset, keine Aenderung.\n');
    return;
end

% Freemode-Signal im Torque Controller bereitstellen
fm = find_system(tc, 'SearchDepth', 1, 'BlockType', 'Inport', 'Name', 'FreemodeSig');
fm = fm{1};
p = get_param(fm, 'Position');
add_block('simulink/Signal Routing/Goto', [tc '/FreemodeSig_I_Goto'], 'GotoTag', 'FreemodeSig_I', ...
    'TagVisibility', 'scoped', 'Position', [p(3) + 40, p(4) + 20, p(3) + 120, p(4) + 40]);
add_line(tc, get_param(fm, 'PortHandles').Outport(1), ...
    get_param([tc '/FreemodeSig_I_Goto'], 'PortHandles').Inport(1), 'autorouting', 'on');
add_block('simulink/Signal Routing/Goto Tag Visibility', [tc '/FreemodeSig_I_Visibility'], ...
    'GotoTag', 'FreemodeSig_I', 'Position', [p(3) + 40, p(4) + 60, p(3) + 120, p(4) + 80]);

for k = 1:numel(axes)
    S = sprintf('%s/PI_Controller_%s', cc, axes{k});
    pid = [S '/PID Controller'];
    set_param(pid, 'ExternalReset', 'level');
    q = get_param(pid, 'Position');
    add_block('simulink/Signal Routing/From', [S '/FreemodeSig_I'], 'GotoTag', 'FreemodeSig_I', ...
        'Position', [q(1) - 220, q(4) + 20, q(1) - 150, q(4) + 40]);
    add_block('simulink/Sources/Constant', [S '/Reset_I_Cur_Enable'], ...
        'Value', 'ctrl_limits.reset_I_cur_freemode', 'OutDataTypeStr', 'boolean', ...
        'Position', [q(1) - 220, q(4) + 60, q(1) - 150, q(4) + 80]);
    add_block('simulink/Logic and Bit Operations/Logical Operator', [S '/Reset_I_Cur_And'], ...
        'Operator', 'AND', 'AllPortsSameDT', 'off', 'OutDataTypeStr', 'boolean', ...
        'Position', [q(1) - 100, q(4) + 20, q(1) - 60, q(4) + 80]);
    add_line(S, 'FreemodeSig_I/1', 'Reset_I_Cur_And/1', 'autorouting', 'on');
    add_line(S, 'Reset_I_Cur_Enable/1', 'Reset_I_Cur_And/2', 'autorouting', 'on');
    ph = get_param(pid, 'PortHandles');
    add_line(S, get_param([S '/Reset_I_Cur_And'], 'PortHandles').Outport(1), ph.Inport(end), 'autorouting', 'on');
    fprintf('%s\n  nachher: ExternalReset=%s, Reset <- FreemodeSig AND ctrl_limits.reset_I_cur_freemode\n', ...
        pid, get_param(pid, 'ExternalReset'));
end
save_system(lib);
fprintf('focLib gespeichert.\n');
end
