function foc_focp_telemetry()
%FOC_FOCP_TELEMETRY  FOCP: Udc, i_d/i_q (Ist und Soll) und Hall-Zustand an FOC Studio melden.
%
%   foc_focp_telemetry()
%
%   Voraussetzung: foc_add_focp (Subsystem app_speed/FOCP). Aenderungen:
%     focLib/Input Controller   neuer Ausgang 7 "Udc" (Ausgang von Variant Source1, uint16 V,
%                               dieselbe Groesse wie fuer Unterspannung und CAN 0x110)
%     focLib/Torque Controller  neuer Ausgang 3 "Idq_PU" (Park Transform, [i_d i_q] in PU)
%                               ueber neuen Ausgang 2 "Idq_PU" des Subsystems FOC
%     app_speed                 Rate Transitions auf Ts_speed fuer Udc und Idq_PU, Data Store Read
%                               GlobalHallState, FOCP-Eingaenge 8..11, MATLAB Function erweitert
%   Die neuen Ausgaenge speisen nur das FOCP-Subsystem; Regelung und Simulation bleiben
%   unveraendert (foc_equivalence_p1a). Vorher/nachher werden ausgegeben; Modelle gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
ic = [lib '/Input Controller']; tc = [lib '/Torque Controller']; foc = [tc '/FOC'];
sub = [app '/FOCP'];
if getSimulinkBlockHandle(sub) == -1, error('foc:focp', 'Zuerst foc_add_focp ausfuehren.'); end
before = state();
fprintf('vorher:  %s\n', before);
if getSimulinkBlockHandle([sub '/udc']) ~= -1
    if addRefDelay(sub)
        save_system(app);
        fprintf('nachher: Unit Delay auf idq_ref nachgeruestet, %s gespeichert.\n', app);
    else
        fprintf('bereits erweitert, keine Aenderung.\n');
    end
    return;
end

% --- focLib (wiederholbar: bereits vorhandene Ausgaenge werden nicht erneut angelegt) ---------
set_param(lib, 'Lock', 'off');
if getSimulinkBlockHandle([ic '/Udc']) == -1
nIc = numel(find_system(ic, 'SearchDepth', 1, 'BlockType', 'Outport'));
p = get_param([ic '/Variant Source1'], 'Position');
add_block('simulink/Sinks/Out1', [ic '/Udc'], 'Port', num2str(nIc + 1), ...
    'Position', [p(3) + 60, p(4) + 30, p(3) + 90, p(4) + 44]);
add_line(ic, 'Variant Source1/1', 'Udc/1', 'autorouting', 'on');
end
nIc = str2double(get_param([ic '/Udc'], 'Port')) - 1;
if getSimulinkBlockHandle([tc '/Idq_PU']) == -1

p = get_param([foc '/Park Transform'], 'Position');
add_block('simulink/Signal Routing/Mux', [foc '/Idq_Mux'], 'Inputs', '2', ...
    'Position', [p(3) + 40, p(4) + 30, p(3) + 45, p(4) + 70]);
add_line(foc, 'Park Transform/1', 'Idq_Mux/1', 'autorouting', 'on');
add_line(foc, 'Park Transform/2', 'Idq_Mux/2', 'autorouting', 'on');
nFoc = numel(find_system(foc, 'SearchDepth', 1, 'BlockType', 'Outport'));
add_block('simulink/Sinks/Out1', [foc '/Idq_PU'], 'Port', num2str(nFoc + 1), ...
    'Position', [p(3) + 90, p(4) + 43, p(3) + 120, p(4) + 57]);
add_line(foc, 'Idq_Mux/1', 'Idq_PU/1', 'autorouting', 'on');
nTc = numel(find_system(tc, 'SearchDepth', 1, 'BlockType', 'Outport'));
p = get_param(foc, 'Position');
add_block('simulink/Sinks/Out1', [tc '/Idq_PU'], 'Port', num2str(nTc + 1), ...
    'Position', [p(3) + 200, p(4) + 20, p(3) + 230, p(4) + 34]);
add_line(tc, sprintf('FOC/%d', nFoc + 1), 'Idq_PU/1', 'autorouting', 'on');
end
nTc = str2double(get_param([tc '/Idq_PU'], 'Port')) - 1;
if strcmp(get_param(lib, 'Dirty'), 'on'), save_system(lib); end
% app_speed neu laden, damit die Bibliotheksverknuepfungen die neuen Ausgaenge zeigen
close_system(app, 0);
load_system(fullfile(root, 'models', [app '.slx']));

% --- app_speed -------------------------------------------------------------------------------
pf = get_param(sub, 'Position');
rtU = [app '/Rate Transition Udc']; rtI = [app '/Rate Transition Idq'];
add_block([app '/Rate Transition1'], rtU, 'Position', [pf(1) - 160, pf(4) + 20, pf(1) - 120, pf(4) + 60]);
add_block([app '/Rate Transition7'], rtI, 'Position', [pf(1) - 160, pf(4) + 80, pf(1) - 120, pf(4) + 120]);
hall = [app '/Hall State Read'];
add_block('simulink/Signal Routing/Data Store Read', hall, 'DataStoreName', 'GlobalHallState', ...
    'SampleTime', 'Ts_speed', 'Position', [pf(1) - 180, pf(4) + 140, pf(1) - 100, pf(4) + 170]);
icph = get_param([app '/Input Controller'], 'PortHandles');
tcph = get_param([app '/Torque Controller'], 'PortHandles');
scph = get_param([app '/Speed Control'], 'PortHandles');
add_line(app, icph.Outport(nIc + 1), get_param(rtU, 'PortHandles').Inport(1), 'autorouting', 'on');
add_line(app, tcph.Outport(nTc + 1), get_param(rtI, 'PortHandles').Inport(1), 'autorouting', 'on');

% FOCP-Subsystem: neue Eingaenge, Umrechnung auf single, MATLAB Function erweitern
newIn = {'udc', 'idq_ref', 'idq', 'hall'};
for k = 1:numel(newIn)
    add_block('simulink/Sources/In1', [sub '/' newIn{k}], 'Port', num2str(7 + k), ...
        'Position', [20, 340 + 40 * k, 50, 354 + 40 * k]);
    add_block('simulink/Signal Attributes/Data Type Conversion', [sub '/' newIn{k} '_conv'], ...
        'OutDataTypeStr', ifelse(strcmp(newIn{k}, 'hall'), 'uint16', 'single'), ...
        'Position', [120, 340 + 40 * k, 180, 360 + 40 * k]);
end
add_block('simulink/Sources/Constant', [sub '/I_base'], 'Value', 'PU_System.I_base', ...
    'OutDataTypeStr', 'single', 'SampleTime', '-1', 'Position', [120, 560, 180, 580]);
fcn = [sub '/FOCP Service'];
ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
ch.Script = strjoin({
    'function [active, freemode, ref] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base, udc, idq_ref, idq, i_base, hall)'
    '% FOCP fuer FOC Studio, nur im erzeugten Code (firmware/focp_c28x.c). Simulation: inaktiv.'
    'active = false; freemode = false; ref = single(0);'
    'if coder.target(''Rtw'')'
    '    coder.cinclude(''focp_c28x.h'');'
    '    a = uint16(0); f = uint16(0); r = single(0);'
    '    iref = single(idq_ref); imeas = single(idq);'
    '    coder.ceval(''focp_c28x_step'', uint16(uv), uint16(fm_can), uint16(hot), uint16(safe), ...'
    '        uint16(mode), single(speed_pu), single(n_base), single(udc), coder.rref(iref), ...'
    '        coder.rref(imeas), single(i_base), uint16(hall), coder.wref(a), coder.wref(f), coder.wref(r));'
    '    active = a ~= 0; freemode = f ~= 0; ref = r;'
    'end'
    'end'}, newline);
in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
for k = 1:numel(in)
    if any(strcmp(in(k).Name, {'idq_ref', 'idq'})), in(k).Props.Array.Size = '2'; end
end
add_line(sub, 'udc/1', 'udc_conv/1');         add_line(sub, 'udc_conv/1', 'FOCP Service/8');
add_line(sub, 'idq_ref/1', 'idq_ref_conv/1');
add_line(sub, 'idq_ref_conv/1', 'FOCP Service/9');
add_line(sub, 'idq/1', 'idq_conv/1');         add_line(sub, 'idq_conv/1', 'FOCP Service/10');
add_line(sub, 'I_base/1', 'FOCP Service/11');
add_line(sub, 'hall/1', 'hall_conv/1');       add_line(sub, 'hall_conv/1', 'FOCP Service/12');
addRefDelay(sub);

subph = get_param(sub, 'PortHandles');
add_line(app, get_param(rtU, 'PortHandles').Outport(1), subph.Inport(8), 'autorouting', 'on');
add_line(app, scph.Outport(1), subph.Inport(9), 'autorouting', 'on');
add_line(app, get_param(rtI, 'PortHandles').Outport(1), subph.Inport(10), 'autorouting', 'on');
add_line(app, get_param(hall, 'PortHandles').Outport(1), subph.Inport(11), 'autorouting', 'on');
save_system(app);

fprintf('nachher: %s\n', state());
fprintf('%s und %s gespeichert.\n', lib, app);

    function s = state()
        s = sprintf(['Input Controller %d Ausgaenge, FOC %d Ausgaenge, Torque Controller %d Ausgaenge, ' ...
            'FOCP %d Eingaenge'], ...
            numel(find_system(ic, 'SearchDepth', 1, 'BlockType', 'Outport')), ...
            numel(find_system(foc, 'SearchDepth', 1, 'BlockType', 'Outport')), ...
            numel(find_system(tc, 'SearchDepth', 1, 'BlockType', 'Outport')), ...
            numel(find_system(sub, 'SearchDepth', 1, 'BlockType', 'Inport')));
    end
end

function added = addRefDelay(sub)
% idq_ref kommt aus Speed Control, das FOCP selbst speist (Sollwert, Freemode): ohne Verzoegerung
% entsteht eine algebraische Schleife. Telemetrie um einen Takt (0,5 ms) verzoegert.
added = getSimulinkBlockHandle([sub '/idq_ref_delay']) == -1;
if ~added, return; end
l = get_param([sub '/idq_ref'], 'LineHandles');
delete_line(l.Outport(1));
add_block('simulink/Discrete/Unit Delay', [sub '/idq_ref_delay'], 'SampleTime', '-1', ...
    'Position', [70, 415, 100, 445]);
add_line(sub, 'idq_ref/1', 'idq_ref_delay/1');
add_line(sub, 'idq_ref_delay/1', 'idq_ref_conv/1');
end

function v = ifelse(c, a, b), if c, v = a; else, v = b; end, end
