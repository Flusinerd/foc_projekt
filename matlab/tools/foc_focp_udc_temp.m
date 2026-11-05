function foc_focp_udc_temp()
%FOC_FOCP_UDC_TEMP  FOCP: Zwischenkreisspannung fein (single, V) und MOSFET-Temperatur melden.
%
%   foc_focp_udc_temp()
%
%   Voraussetzung: foc_focp_telemetry. Bisher ist Udc der uint16-Ausgang von Variant Source1
%   (auf ganze Volt gerundet, wie fuer Unterspannung und CAN 0x110) und die Temperatur 0.
%   focLib/Input Controller:
%     BatteryVoltage_CodeGen  neuer Ausgang 2 "batteryVoltage_V" = Product2 (single, V, vor dem Runden)
%     Udc_Sim_V               Konstante wie BatteryVoltage_Sim, single
%     Variant Source Udc      Kopie von Variant Source1 (gleiche Bedingungen) -> Ausgang 7 "Udc"
%     neuer Ausgang 8 "Temp"  = Variant Source3 (MOSFET-Temperatur in degC, wie fuer hot_temp)
%   app_speed: Rate Transition fuer Temp auf Ts_speed, FOCP-Eingang "temp"; die MATLAB Function
%   bekommt v_base und temp (firmware/focp_c28x.c, neue Signatur). Unterspannung, CAN 0x110 und
%   hot_temp bleiben unveraendert. Vorher/nachher werden ausgegeben; Modelle werden gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
ic = [lib '/Input Controller'];
sub = [app '/FOCP'];
fprintf('vorher:  %s\n', state());
if getSimulinkBlockHandle([sub '/temp']) ~= -1
    fprintf('bereits umgesetzt, keine Aenderung.\n');
    return;
end

% --- focLib -------------------------------------------------------------------------------
set_param(lib, 'Lock', 'off');
if getSimulinkBlockHandle([ic '/Temp']) == -1
    bv = [ic '/BatteryVoltage_CodeGen'];
    p = get_param([bv '/Product2'], 'Position');
    add_block('simulink/Sinks/Out1', [bv '/batteryVoltage_V'], 'Port', '2', ...
        'Position', [p(3) + 60, p(4) + 40, p(3) + 90, p(4) + 54]);
    add_line(bv, 'Product2/1', 'batteryVoltage_V/1', 'autorouting', 'on');

    vs1 = [ic '/Variant Source1'];
    q = get_param(vs1, 'Position');
    vsu = [ic '/Variant Source Udc'];
    add_block(vs1, vsu, 'Position', q + [0 120 0 120]);
    add_block('simulink/Sources/Constant', [ic '/Udc_Sim_V'], ...
        'Value', get_param([ic '/BatteryVoltage_Sim'], 'Value'), 'OutDataTypeStr', 'single', ...
        'Position', [q(1) - 160, q(4) + 140, q(1) - 100, q(4) + 160]);
    bvph = get_param(bv, 'PortHandles');
    vsuph = get_param(vsu, 'PortHandles');
    add_line(ic, bvph.Outport(2), vsuph.Inport(1), 'autorouting', 'on');
    add_line(ic, get_param([ic '/Udc_Sim_V'], 'PortHandles').Outport(1), vsuph.Inport(2), 'autorouting', 'on');
    udc = [ic '/Udc'];
    delete_line(get_param(get_param(udc, 'PortHandles').Inport(1), 'Line'));
    add_line(ic, vsuph.Outport(1), get_param(udc, 'PortHandles').Inport(1), 'autorouting', 'on');

    nIc = numel(find_system(ic, 'SearchDepth', 1, 'BlockType', 'Outport'));
    q = get_param([ic '/Variant Source3'], 'Position');
    add_block('simulink/Sinks/Out1', [ic '/Temp'], 'Port', num2str(nIc + 1), ...
        'Position', [q(3) + 80, q(4) + 30, q(3) + 110, q(4) + 44]);
    add_line(ic, 'Variant Source3/1', 'Temp/1', 'autorouting', 'on');
    save_system(lib);
end
nTemp = str2double(get_param([ic '/Temp'], 'Port'));
close_system(app, 0);                         % Bibliotheksverknuepfungen neu laden
load_system(fullfile(root, 'models', [app '.slx']));

% --- app_speed ----------------------------------------------------------------------------
pf = get_param(sub, 'Position');
rtT = [app '/Rate Transition Temp'];
add_block([app '/Rate Transition Udc'], rtT, 'Position', [pf(1) - 160, pf(4) + 190, pf(1) - 120, pf(4) + 230]);
icph = get_param([app '/Input Controller'], 'PortHandles');
add_line(app, icph.Outport(nTemp), get_param(rtT, 'PortHandles').Inport(1), 'autorouting', 'on');

nIn = numel(find_system(sub, 'SearchDepth', 1, 'BlockType', 'Inport'));
add_block('simulink/Sources/In1', [sub '/temp'], 'Port', num2str(nIn + 1), 'Position', [20, 620, 50, 634]);
add_block('simulink/Signal Attributes/Data Type Conversion', [sub '/temp_conv'], ...
    'OutDataTypeStr', 'single', 'Position', [120, 620, 180, 640]);
add_block('simulink/Sources/Constant', [sub '/V_base'], 'Value', 'PU_System.V_base', ...
    'OutDataTypeStr', 'single', 'SampleTime', '-1', 'Position', [120, 660, 180, 680]);
add_line(sub, 'temp/1', 'temp_conv/1');
fcn = [sub '/FOCP Service'];
ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
ch.Script = strjoin({
    'function [active, freemode, ref] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base, udc, idq_ref, idq, i_base, hall, v_base, temp)'
    '% FOCP fuer FOC Studio, nur im erzeugten Code (firmware/focp_c28x.c). Simulation: inaktiv.'
    '% idq wird nicht mehr benutzt (Stroeme kommen aus focp_c28x_isr im Stromregler-Interrupt).'
    'active = false; freemode = false; ref = single(0);'
    'if coder.target(''Rtw'')'
    '    coder.cinclude(''focp_c28x.h'');'
    '    a = uint16(0); f = uint16(0); r = single(0);'
    '    iref = single(idq_ref);'
    '    coder.ceval(''focp_c28x_step'', uint16(uv), uint16(fm_can), uint16(hot), uint16(safe), ...'
    '        uint16(mode), single(speed_pu), single(n_base), single(udc), coder.rref(iref), ...'
    '        single(i_base), single(v_base), single(temp), uint16(hall), coder.wref(a), coder.wref(f), coder.wref(r));'
    '    active = a ~= 0; freemode = f ~= 0; ref = r;'
    'end'
    'end'}, newline);
in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
for k = 1:numel(in)
    if any(strcmp(in(k).Name, {'idq_ref', 'idq'})), in(k).Props.Array.Size = '2'; end
end
add_line(sub, 'V_base/1', 'FOCP Service/13');
add_line(sub, 'temp_conv/1', 'FOCP Service/14');
subph = get_param(sub, 'PortHandles');
add_line(app, get_param(rtT, 'PortHandles').Outport(1), subph.Inport(nIn + 1), 'autorouting', 'on');
save_system(app);
fprintf('nachher: %s\n', state());
fprintf('%s und %s gespeichert.\n', lib, app);

    function s = state()
        s = sprintf('Input Controller %d Ausgaenge, Udc <- %s, FOCP %d Eingaenge', ...
            numel(find_system(ic, 'SearchDepth', 1, 'BlockType', 'Outport')), udcSource(), ...
            numel(find_system(sub, 'SearchDepth', 1, 'BlockType', 'Inport')));
    end
    function n = udcSource()
        l = get_param(get_param([ic '/Udc'], 'PortHandles').Inport(1), 'Line');
        n = get_param(get_param(l, 'SrcBlockHandle'), 'Name');
    end
end
