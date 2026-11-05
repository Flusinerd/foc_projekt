function foc_add_focp(model)
%FOC_ADD_FOCP  FOCP (CAN-Protokoll fuer FOC Studio) in das Anwendungsmodell einbauen.
%
%   foc_add_focp()          matlab/models/app_speed.slx
%   foc_add_focp(model)
%
%   Neues Subsystem "FOCP" auf der Wurzelebene (focLib bleibt unveraendert) zwischen den
%   Rate Transitions des Input Controllers und der Speed Control (Takt Ts_speed):
%     Speed_Ref -> Switch(active ? FOCP-Sollwert : Poti)  -> Speed Control
%     Freemode  -> OR(Freemode, FOCP-Freemode)           -> Speed Control
%   Die MATLAB Function ruft firmware/focp_c28x.c nur bei der Codegenerierung auf
%   (coder.target('Rtw')). In der Simulation ist active = 0: Verhalten bitgleich.
%   Custom Code (nur Codegenerierung): protocol/src/*.c, firmware/focp_target.c, focp_c28x.c.
%   Handgeschriebener C-Code: bewusste Abweichung von AGENTS.md, Entscheidung des Menschen
%   (reports/08_FOCP_Einbau_Plan_2026-11-04.md). Vorher/nachher werden ausgegeben; Modell
%   wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(model), model = 'app_speed'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
sub = [model '/FOCP'];
fprintf('%s\n  vorher:  FOCP-Subsystem %s, CustomSource=[%s], CustomInclude=[%s]\n', model, ...
    yesno(getSimulinkBlockHandle(sub) ~= -1), oneLine(get_param(model, 'CustomSource')), ...
    oneLine(get_param(model, 'CustomInclude')));
if getSimulinkBlockHandle(sub) ~= -1
    setCustomCode(model);
    fprintf('  bereits eingebaut; nachher: CustomSource=[%s], CustomInclude=[%s]\n', ...
        oneLine(get_param(model, 'CustomSource')), oneLine(get_param(model, 'CustomInclude')));
    if strcmp(get_param(model, 'Dirty'), 'on'), save_system(model); fprintf('%s gespeichert.\n', model); end
    return;
end

sc = [model '/Speed Control'];
% Quellen (Ausgaenge der Rate Transitions) und die Speed-Control-Eingaenge, die sie speisen
src = struct('name', {'Speed_Ref', 'Undervoltage', 'Freemode', 'hot_temp', 'speedMode', 'SafeState', 'speed_pu'}, ...
             'blk',  {'Rate Transition5', 'Rate Transition1', 'Rate Transition2', 'Rate Transition3', ...
                      'Rate Transition4', 'Rate Transition13', 'From2'});
for k = 1:numel(src)
    ph = get_param([model '/' src(k).blk], 'PortHandles');
    src(k).port = ph.Outport(1);
end
dstRef = scInputFed(src(1).port, sc);
dstFm  = scInputFed(src(3).port, sc);
fprintf('  Speed_Ref: %s -> Speed Control In%d, Freemode: %s -> Speed Control In%d\n', ...
    src(1).blk, dstRef, src(3).blk, dstFm);

% --- Subsystem aufbauen ------------------------------------------------------------------
p = get_param(sc, 'Position');
add_block('simulink/Ports & Subsystems/Subsystem', sub, 'Position', [p(1) - 260, p(4) + 60, p(1) - 120, p(4) + 260]);
delete_line(sub, 'In1/1', 'Out1/1');
delete_block([sub '/In1']); delete_block([sub '/Out1']);
for k = 1:numel(src)
    add_block('simulink/Sources/In1', sprintf('%s/%s', sub, src(k).name), 'Port', num2str(k), ...
        'Position', [20, 20 + 40 * k, 50, 34 + 40 * k]);
end
add_block('simulink/Sinks/Out1', [sub '/Speed_Ref_out'], 'Port', '1', 'Position', [760, 60, 790, 74]);
add_block('simulink/Sinks/Out1', [sub '/Freemode_out'], 'Port', '2', 'Position', [760, 140, 790, 154]);

fcn = [sub '/FOCP Service'];
add_block('simulink/User-Defined Functions/MATLAB Function', fcn, 'Position', [300, 80, 480, 340]);
ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
ch.Script = strjoin({
    'function [active, freemode, ref] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base)'
    '% FOCP fuer FOC Studio, nur im erzeugten Code (firmware/focp_c28x.c). Simulation: inaktiv.'
    'active = false; freemode = false; ref = single(0);'
    'if coder.target(''Rtw'')'
    '    coder.cinclude(''focp_c28x.h'');'
    '    a = uint16(0); f = uint16(0); r = single(0);'
    '    coder.ceval(''focp_c28x_step'', uint16(uv), uint16(fm_can), uint16(hot), uint16(safe), ...'
    '        uint16(mode), single(speed_pu), single(n_base), coder.wref(a), coder.wref(f), coder.wref(r));'
    '    active = a ~= 0; freemode = f ~= 0; ref = r;'
    'end'
    'end'}, newline);
out = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Output');
types = struct('active', 'boolean', 'freemode', 'boolean', 'ref', 'single');
for k = 1:numel(out), out(k).DataType = types.(out(k).Name); end

add_block('simulink/Signal Attributes/Data Type Conversion', [sub '/speed_single'], ...
    'OutDataTypeStr', 'single', 'Position', [120, 300, 180, 320]);
add_block('simulink/Sources/Constant', [sub '/N_base'], 'Value', 'PU_System.N_base', ...
    'OutDataTypeStr', 'single', 'SampleTime', '-1', 'Position', [120, 340, 180, 360]);
add_block('simulink/Signal Attributes/Data Type Conversion', [sub '/ref_fix'], ...
    'OutDataTypeStr', 'fixdt(1,32,17)', 'RndMeth', 'Nearest', 'Position', [540, 40, 600, 60]);
add_block('simulink/Signal Routing/Switch', [sub '/Ref_Switch'], 'Criteria', 'u2 ~= 0', ...
    'Position', [660, 50, 700, 90]);
add_block('simulink/Logic and Bit Operations/Logical Operator', [sub '/Freemode_Or'], 'Operator', 'OR', ...
    'Position', [660, 130, 700, 170]);

% Verdrahtung im Subsystem
add_line(sub, 'Undervoltage/1', 'FOCP Service/1');
add_line(sub, 'Freemode/1', 'FOCP Service/2');
add_line(sub, 'hot_temp/1', 'FOCP Service/3');
add_line(sub, 'SafeState/1', 'FOCP Service/4');
add_line(sub, 'speedMode/1', 'FOCP Service/5');
add_line(sub, 'speed_pu/1', 'speed_single/1');
add_line(sub, 'speed_single/1', 'FOCP Service/6');
add_line(sub, 'N_base/1', 'FOCP Service/7');
add_line(sub, 'FOCP Service/3', 'ref_fix/1');
add_line(sub, 'ref_fix/1', 'Ref_Switch/1');
add_line(sub, 'FOCP Service/1', 'Ref_Switch/2');
add_line(sub, 'Speed_Ref/1', 'Ref_Switch/3');
add_line(sub, 'Ref_Switch/1', 'Speed_Ref_out/1');
add_line(sub, 'Freemode/1', 'Freemode_Or/1');
add_line(sub, 'FOCP Service/2', 'Freemode_Or/2');
add_line(sub, 'Freemode_Or/1', 'Freemode_out/1');

% --- Wurzelebene umverdrahten --------------------------------------------------------------
scph = get_param(sc, 'PortHandles');
delete_line(get_param(scph.Inport(dstRef), 'Line'));
delete_line(get_param(scph.Inport(dstFm), 'Line'));
subph = get_param(sub, 'PortHandles');
for k = 1:numel(src)
    add_line(model, src(k).port, subph.Inport(k), 'autorouting', 'on');
end
add_line(model, subph.Outport(1), scph.Inport(dstRef), 'autorouting', 'on');
add_line(model, subph.Outport(2), scph.Inport(dstFm), 'autorouting', 'on');

setCustomCode(model);

fprintf('  nachher: FOCP-Subsystem ja, CustomSource=[%s], CustomInclude=[%s]\n', ...
    oneLine(get_param(model, 'CustomSource')), oneLine(get_param(model, 'CustomInclude')));
save_system(model);
fprintf('%s gespeichert.\n', model);
end

function setCustomCode(model)
% Custom Code nur fuer die Codegenerierung. Simulink loest die Pfade relativ zum Modellordner
% (matlab/models) auf.
set_param(model, 'CustomSource', strjoin({'../../protocol/src/focp.c', '../../protocol/src/focp_dev.c', ...
    '../../firmware/focp_target.c', '../../firmware/focp_c28x.c'}, newline), ...
    'CustomInclude', strjoin({'../../protocol/include', '../../protocol/include/focp', '../../firmware'}, newline));
end

function k = scInputFed(srcPort, sc)
% Nummer des Speed-Control-Eingangs, der von srcPort gespeist wird
l = get_param(srcPort, 'Line');
dst = get_param(l, 'DstPortHandle');
scph = get_param(sc, 'PortHandles');
k = find(ismember(scph.Inport, dst), 1);
if isempty(k), error('foc:focp', 'Quelle speist keinen Eingang von %s.', sc); end
end

function s = oneLine(s), s = strrep(strtrim(s), newline, ', '); end
function s = yesno(b), if b, s = 'ja'; else, s = 'nein'; end, end
