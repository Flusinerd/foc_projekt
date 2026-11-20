function foc_overvoltage()
%FOC_OVERVOLTAGE  Ueberspannungsabschaltung (Hardware-Review HW19, offene Aufgabe A1).
%
%   foc_overvoltage()
%
%   focLib/Input Controller:
%     Relay "Overvoltage_Relay" auf das Udc in V (Variant Source Udc): EIN ab focp_tune_udc_max,
%     AUS unter focp_tune_udc_max - ctrl_limits.Udc_hyst; AND ctrl_limits.ov_enable
%     -> neuer Ausgang "Overvoltage_Sig"
%   app_speed (Wurzel):
%     Rate Transition auf Ts_speed, ODER mit Undervoltage_Sig vor dem Speed-Control-Eingang
%     (Speed Control und die MIL-Harnesses bleiben unveraendert), Overvoltage_Sig an FOCP
%     (STATUS-Fehlerbit FOCP_FAULT_OVERVOLTAGE; die Schwelle ist ueber inv.Udc_max aus FOC Studio
%     aenderbar, focp_tune_udc_max).
%   legacy: ov_enable = 0 -> bitgleich. Einschraenkung: Der Input Controller laeuft mit 0,2 s.
%   Vorher/nachher werden ausgegeben; Modelle werden gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
ic = [lib '/Input Controller'];
sub = [app '/FOCP'];
fprintf('vorher:  %s\n', state());
if getSimulinkBlockHandle([app '/Voltage_Fault_Or']) ~= -1
    fprintf('bereits umgesetzt, keine Aenderung.\n');
    return;
end

% --- focLib -------------------------------------------------------------------------------
if getSimulinkBlockHandle([ic '/Overvoltage_Sig']) == -1
    set_param(lib, 'Lock', 'off');
    q = get_param([ic '/Variant Source Udc'], 'Position');
    rel = [ic '/Overvoltage_Relay'];
    add_block('simulink/Discontinuities/Relay', rel, 'OnSwitchValue', 'focp_tune_udc_max', ...
        'OffSwitchValue', 'focp_tune_udc_max - ctrl_limits.Udc_hyst', 'OnOutputValue', '1', ...
        'OffOutputValue', '0', 'OutDataTypeStr', 'boolean', 'Position', q + [120 60 140 60]);
    add_block('simulink/Sources/Constant', [ic '/OV_Enable'], 'Value', 'ctrl_limits.ov_enable', ...
        'OutDataTypeStr', 'boolean', 'Position', q + [120 110 140 110]);
    add_block('simulink/Logic and Bit Operations/Logical Operator', [ic '/OV_And'], 'Operator', 'AND', ...
        'AllPortsSameDT', 'off', 'OutDataTypeStr', 'boolean', 'Position', q + [220 60 230 110]);
    n = numel(find_system(ic, 'SearchDepth', 1, 'BlockType', 'Outport'));
    add_block('simulink/Sinks/Out1', [ic '/Overvoltage_Sig'], 'Port', num2str(n + 1), 'Position', q + [300 75 310 75]);
    add_line(ic, 'Variant Source Udc/1', 'Overvoltage_Relay/1', 'autorouting', 'on');
    add_line(ic, 'Overvoltage_Relay/1', 'OV_And/1', 'autorouting', 'on');
    add_line(ic, 'OV_Enable/1', 'OV_And/2', 'autorouting', 'on');
    add_line(ic, 'OV_And/1', 'Overvoltage_Sig/1', 'autorouting', 'on');
    save_system(lib);
end
nOv = str2double(get_param([ic '/Overvoltage_Sig'], 'Port'));
close_system(app, 0);                         % Bibliotheksverknuepfungen neu laden
load_system(fullfile(root, 'models', [app '.slx']));

% --- app_speed ----------------------------------------------------------------------------
rt1 = [app '/Rate Transition1'];              % Undervoltage_Sig auf Ts_speed
sc = [app '/Speed Control'];
rt1ph = get_param(rt1, 'PortHandles');
dst = get_param(get_param(rt1ph.Outport(1), 'Line'), 'DstPortHandle');
scph = get_param(sc, 'PortHandles');
scIn = dst(ismember(dst, scph.Inport));
if numel(scIn) ~= 1, error('foc:ov', 'Undervoltage-Eingang von Speed Control nicht eindeutig.'); end
p = get_param(rt1, 'Position');
rtO = [app '/Rate Transition OV'];
add_block(rt1, rtO, 'Position', p + [0 60 0 60]);
orb = [app '/Voltage_Fault_Or'];
add_block('simulink/Logic and Bit Operations/Logical Operator', orb, 'Operator', 'OR', ...
    'Position', p + [80 0 110 50]);
icph = get_param([app '/Input Controller'], 'PortHandles');
add_line(app, icph.Outport(nOv), get_param(rtO, 'PortHandles').Inport(1), 'autorouting', 'on');
delete_line(app, rt1ph.Outport(1), scIn);
orph = get_param(orb, 'PortHandles');
add_line(app, rt1ph.Outport(1), orph.Inport(1), 'autorouting', 'on');
add_line(app, get_param(rtO, 'PortHandles').Outport(1), orph.Inport(2), 'autorouting', 'on');
add_line(app, orph.Outport(1), scIn, 'autorouting', 'on');

% FOCP: neuer Eingang "ov" (letzte Stelle der MATLAB Function, damit sich die Ports nicht verschieben)
nIn = numel(find_system(sub, 'SearchDepth', 1, 'BlockType', 'Inport'));
add_block('simulink/Sources/In1', [sub '/ov'], 'Port', num2str(nIn + 1), 'Position', [20, 700, 50, 714]);
fcn = [sub '/FOCP Service'];
ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
ch.Script = strjoin({
    'function [active, freemode, ref] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base, udc, idq_ref, idq, i_base, hall, v_base, temp, ov)'
    '% FOCP fuer FOC Studio, nur im erzeugten Code (firmware/focp_c28x.c). Simulation: inaktiv.'
    '% idq wird nicht mehr benutzt (Stroeme kommen aus focp_c28x_isr im Stromregler-Interrupt).'
    'active = false; freemode = false; ref = single(0);'
    'if coder.target(''Rtw'')'
    '    coder.cinclude(''focp_c28x.h'');'
    '    a = uint16(0); f = uint16(0); r = single(0);'
    '    iref = single(idq_ref);'
    '    coder.ceval(''focp_c28x_step'', uint16(uv), uint16(ov), uint16(fm_can), uint16(hot), uint16(safe), ...'
    '        uint16(mode), single(speed_pu), single(n_base), single(udc), coder.rref(iref), ...'
    '        single(i_base), single(v_base), single(temp), uint16(hall), coder.wref(a), coder.wref(f), coder.wref(r));'
    '    active = a ~= 0; freemode = f ~= 0; ref = r;'
    'end'
    'end'}, newline);
in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
for k = 1:numel(in)
    if any(strcmp(in(k).Name, {'idq_ref', 'idq'})), in(k).Props.Array.Size = '2'; end
end
add_line(sub, 'ov/1', 'FOCP Service/15');
subph = get_param(sub, 'PortHandles');
add_line(app, get_param(rtO, 'PortHandles').Outport(1), subph.Inport(nIn + 1), 'autorouting', 'on');
save_system(app);
fprintf('nachher: %s\n', state());
fprintf('%s und %s gespeichert.\n', lib, app);

    function s = state()
        s = sprintf('Input Controller %d Ausgaenge, Overvoltage_Sig %s, Speed-Control-Unterspannung <- %s, FOCP %d Eingaenge', ...
            numel(find_system(ic, 'SearchDepth', 1, 'BlockType', 'Outport')), ...
            yesno(getSimulinkBlockHandle([ic '/Overvoltage_Sig']) ~= -1), uvSource(), ...
            numel(find_system(sub, 'SearchDepth', 1, 'BlockType', 'Inport')));
    end
    function n = uvSource()
        if getSimulinkBlockHandle([app '/Voltage_Fault_Or']) ~= -1, n = 'Voltage_Fault_Or'; else, n = 'Rate Transition1'; end
    end
end

function s = yesno(b), if b, s = 'ja'; else, s = 'nein'; end, end
