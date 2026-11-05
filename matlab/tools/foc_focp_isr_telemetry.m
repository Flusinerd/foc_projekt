function foc_focp_isr_telemetry()
%FOC_FOCP_ISR_TELEMETRY  FOCP-Telemetrie im Stromregler-Interrupt (20 kHz) abtasten.
%
%   foc_focp_isr_telemetry()
%
%   Bisher tastete focp_c28x_step die Telemetrie im Drehzahltakt (2 kHz) ab; Stroeme kamen nur mit
%   2 kHz und ueber eine Rate Transition. Neu in focLib/Torque Controller/FOC (laeuft im ADC-ISR):
%     MATLAB Function "FOCP ISR" ruft nur im erzeugten Code focp_c28x_isr auf
%     (coder.target('Rtw')); Eingaenge [ia ib] (Iab_PU), Pos_PU, [id iq] (Idq_Mux),
%     [Vd Vq] (Ausgaenge der Current_Controllers), alle als single.
%   In der Simulation ohne Wirkung (keine Ausgaenge). Vorher/nachher werden ausgegeben;
%   focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
foc = [lib '/Torque Controller/FOC'];
fcn = [foc '/FOCP ISR'];
fprintf('%s\n  vorher:  FOCP ISR %s\n', foc, yesno(getSimulinkBlockHandle(fcn) ~= -1));
if getSimulinkBlockHandle(fcn) ~= -1
    fprintf('  bereits vorhanden, keine Aenderung.\n');
    return;
end
set_param(lib, 'Lock', 'off');
p = get_param([foc '/Current_Controllers'], 'Position');
x = p(1); y = p(4) + 120;
add_block('simulink/User-Defined Functions/MATLAB Function', fcn, 'Position', [x + 200, y, x + 340, y + 160]);
ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
ch.Script = strjoin({
    'function focp_isr(iab, pos, idq, vdq)'
    '% FOCP-Telemetrie im Stromregler-Interrupt, nur im erzeugten Code (firmware/focp_c28x.c).'
    'if coder.target(''Rtw'')'
    '    coder.cinclude(''focp_c28x.h'');'
    '    coder.ceval(''focp_c28x_isr'', coder.rref(iab), pos, coder.rref(idq), coder.rref(vdq));'
    'end'
    'end'}, newline);
in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
for k = 1:numel(in)
    if ~strcmp(in(k).Name, 'pos'), in(k).Props.Array.Size = '2'; end
    in(k).DataType = 'single';
end

names = {'iab', 'pos', 'idq', 'vdq'};
for k = 1:numel(names)
    add_block('simulink/Signal Attributes/Data Type Conversion', [foc '/FOCP_' names{k}], ...
        'OutDataTypeStr', 'single', 'Position', [x + 100, y + 40 * (k - 1), x + 150, y + 20 + 40 * (k - 1)]);
    add_line(foc, ['FOCP_' names{k} '/1'], sprintf('FOCP ISR/%d', k), 'autorouting', 'on');
end
add_block('simulink/Signal Routing/Mux', [foc '/FOCP_Vdq_Mux'], 'Inputs', '2', ...
    'Position', [x + 60, y + 120, x + 65, y + 160]);
add_line(foc, 'Current_Controllers/1', 'FOCP_Vdq_Mux/1', 'autorouting', 'on');
add_line(foc, 'Current_Controllers/2', 'FOCP_Vdq_Mux/2', 'autorouting', 'on');
add_line(foc, 'FOCP_Vdq_Mux/1', 'FOCP_vdq/1', 'autorouting', 'on');
add_line(foc, 'Idq_Mux/1', 'FOCP_idq/1', 'autorouting', 'on');
add_line(foc, 'Iab_PU/1', 'FOCP_iab/1', 'autorouting', 'on');
add_line(foc, 'Pos_PU/1', 'FOCP_pos/1', 'autorouting', 'on');
save_system(lib);
fprintf('  nachher: FOCP ISR ja (iab, pos, idq, vdq -> focp_c28x_isr)\nfocLib gespeichert.\n');
end

function s = yesno(b), if b, s = 'ja'; else, s = 'nein'; end, end
