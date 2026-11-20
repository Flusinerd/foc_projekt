function foc_overcurrent_trip()
%FOC_OVERCURRENT_TRIP  Hardware-Ueberstromabschaltung ueber CMPSS3 und ePWM-Trip-Zone (HW16, A4).
%
%   foc_startup('datasheet'); foc_overcurrent_trip()
%
%   Nur Codegenerierungspfad (Hardware-Einstellungen und Bloecke in der Variante CodeGeneration);
%   die Simulation bleibt unveraendert.
%   1. Hardware-Einstellungen app_speed:
%      CMPSS3 COMPH und COMPL mit internem DAC (Referenz VDDA), Digitalfilter ca. 1 us
%      (Takt SYSCLK/10 = 50 ns, Fenster 21, Schwelle 16), COMPL invertiert (Ausloesung unterhalb
%      der unteren Schwelle), Ausgang an die ePWM-X-BAR = Filterausgang.
%      ePWM-X-BAR TRIP4: MUX 4 Eingang 1 = CMPSS3_CTRIPH_OR_CTRIPL (beide Stromrichtungen);
%      EXBAR_TRIP4_MUXSELECT hat ein Zeichen je MUX ('X' = aus).
%   2. focLib/Torque Controller/Output/CodeGeneration:
%      CMPSS-Bloecke CMPSS3_COMPH (DACH = ctrl_limits.cmpss_dac_hi) und CMPSS3_COMPL
%      (DACL = ctrl_limits.cmpss_dac_lo), Ausgaenge auf Terminatoren.
%      ePWM1..3 (ePWM6..8): DCAH = TRIPIN4 (GPTRIP4SEL), DCAEVT1 bei DCAH high (asynchron),
%      DCAEVT1 als One-Shot-Trip, Aktion A low / B high (DRV8300DI: INL invertiert = Freemode).
%   Quittieren und Melden: firmware/focp_c28x.c. Vorher/nachher werden ausgegeben; Modelle werden
%   gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end

% --- 1. Hardware-Einstellungen -------------------------------------------------------------
cs = getActiveConfigSet(app);
hw = {
    'CMPSS.Enable_MODULE_CMPSS3', 1
    'CMPSS.CMPSS3_COMPH', 1
    'CMPSS.CMPSS3_COMPH_COMPHINV', 0
    'CMPSS.CMPSS3_COMPH_DIGITALFILTER', 1
    'CMPSS.CMPSS3_COMPH_CLKPRESCALE', '9'
    'CMPSS.CMPSS3_COMPH_SAMPWIN', '20'
    'CMPSS.CMPSS3_COMPH_THRESH', '15'
    'CMPSS.CMPSS3_COMPH_CTRIPH', 'Digital filter output (COMPHSTS)'
    'CMPSS.CMPSS3_COMPL', 1
    'CMPSS.CMPSS3_COMPL_COMPLINV', 1
    'CMPSS.CMPSS3_COMPL_DIGITALFILTER', 1
    'CMPSS.CMPSS3_COMPL_CLKPRESCALE', '9'
    'CMPSS.CMPSS3_COMPL_SAMPWIN', '20'
    'CMPSS.CMPSS3_COMPL_THRESH', '15'
    'CMPSS.CMPSS3_COMPL_CTRIPL', 'Digital filter output (COMPLSTS)'
    'CMPSS.CMPSS3_COMP_SELREF', 'Internal reference voltage (VDDA)'
    'ePWM.EXBAR_TRIP4', 'MUX 4'
    'ePWM.EXBAR_TRIP4_MUXSELECT', muxsel(4, '1')};   % MUX4 Eingang 1 = CMPSS3_CTRIPH_OR_CTRIPL
for k = 1:size(hw, 1)
    before = show(codertarget.data.getParameterValue(cs, hw{k, 1}));
    codertarget.data.setParameterValue(cs, hw{k, 1}, hw{k, 2});
    fprintf('%-36s vorher [%s] -> nachher [%s]\n', hw{k, 1}, before, ...
        show(codertarget.data.getParameterValue(cs, hw{k, 1})));
end
save_system(app);
close_system(app, 0);

% --- 2. focLib: CMPSS-Bloecke und ePWM-Trip-Zone ---------------------------------------------
set_param(lib, 'Lock', 'off');
cg = [lib '/Torque Controller/Output/CodeGeneration'];
if getSimulinkBlockHandle([cg '/CMPSS3_COMPH']) == -1
    p = get_param([cg '/ePWM3'], 'Position');
    spec = {'CMPSS3_COMPH', 'DACHValue', 'ctrl_limits.cmpss_dac_hi'; 'CMPSS3_COMPL', 'DACLValue', 'ctrl_limits.cmpss_dac_lo'};
    for k = 1:2
        blk = [cg '/' spec{k, 1}];
        add_block('c2838xlib/CMPSS', blk, 'Position', [p(1), p(4) + 60 + 80 * (k - 1), p(1) + 90, p(4) + 110 + 80 * (k - 1)]);
        set_param(blk, 'Module', spec{k, 1}, 'NegativeInput', 'Internal DAC', 'DacValueVia', 'Dialog', ...
            spec{k, 2}, spec{k, 3}, 'SampleTime', '-1');
        ph = get_param(blk, 'PortHandles');
        for o = 1:numel(ph.Outport)
            t = sprintf('%s/%s_Term%d', cg, spec{k, 1}, o);
            add_block('simulink/Sinks/Terminator', t, 'Position', [p(1) + 130, p(4) + 70 + 80 * (k - 1) + 20 * (o - 1), ...
                p(1) + 150, p(4) + 90 + 80 * (k - 1) + 20 * (o - 1)]);
            add_line(cg, ph.Outport(o), get_param(t, 'PortHandles').Inport(1), 'autorouting', 'on');
        end
        fprintf('%s: Module=%s, %s=%s\n', blk, get_param(blk, 'Module'), spec{k, 2}, get_param(blk, spec{k, 2}));
    end
end
tz = {'dcah_type2', 'GPTRIP4SEL'; 'gendcaevt1', 'DCAH=high and DCAL=don''t care'; ...
      'dcaevt1src', 'DCAEVT1 with async'; 'tzselOSHDCAEV1', 'on'; ...
      'ePWMxAforced', 'Low'; 'ePWMxBforced', 'High'};
for e = 1:3
    blk = sprintf('%s/ePWM%d', cg, e);
    before = strjoin(cellfun(@(x) sprintf('%s=%s', x, get_param(blk, x)), tz(:, 1)', 'UniformOutput', false), ', ');
    for k = 1:size(tz, 1), set_param(blk, tz{k, 1}, tz{k, 2}); end
    after = strjoin(cellfun(@(x) sprintf('%s=%s', x, get_param(blk, x)), tz(:, 1)', 'UniformOutput', false), ', ');
    fprintf('%s\n  vorher:  %s\n  nachher: %s\n', blk, before, after);
end
save_system(lib);

% --- 3. FOCP: Software-Schwelle ctrl_limits.I_trip als Vorgabe an focp_c28x_step -------------
% (die Variable focp_tune_i_trip gehoert der C-Seite; FOC Studio aendert sie ueber inv.I_max)
load_system(fullfile(root, 'models', [app '.slx']));
sub = [app '/FOCP'];
if getSimulinkBlockHandle([sub '/I_trip']) == -1
    add_block('simulink/Sources/Constant', [sub '/I_trip'], 'Value', 'ctrl_limits.I_trip', ...
        'OutDataTypeStr', 'single', 'SampleTime', '-1', 'Position', [120, 740, 180, 760]);
    fcn = [sub '/FOCP Service'];
    ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
    ch.Script = strjoin({
        'function [active, freemode, ref] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base, udc, idq_ref, idq, i_base, hall, v_base, temp, ov, i_trip)'
        '% FOCP fuer FOC Studio, nur im erzeugten Code (firmware/focp_c28x.c). Simulation: inaktiv.'
        '% idq wird nicht mehr benutzt (Stroeme kommen aus focp_c28x_isr im Stromregler-Interrupt).'
        'active = false; freemode = false; ref = single(0);'
        'if coder.target(''Rtw'')'
        '    coder.cinclude(''focp_c28x.h'');'
        '    a = uint16(0); f = uint16(0); r = single(0);'
        '    iref = single(idq_ref);'
        '    coder.ceval(''focp_c28x_step'', uint16(uv), uint16(ov), uint16(fm_can), uint16(hot), uint16(safe), ...'
        '        uint16(mode), single(speed_pu), single(n_base), single(udc), coder.rref(iref), ...'
        '        single(i_base), single(v_base), single(temp), uint16(hall), single(i_trip), ...'
        '        coder.wref(a), coder.wref(f), coder.wref(r));'
        '    active = a ~= 0; freemode = f ~= 0; ref = r;'
        'end'
        'end'}, newline);
    in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
    for k = 1:numel(in)
        if any(strcmp(in(k).Name, {'idq_ref', 'idq'})), in(k).Props.Array.Size = '2'; end
    end
    add_line(sub, 'I_trip/1', 'FOCP Service/16');
    fprintf('%s: Eingang i_trip (ctrl_limits.I_trip) an FOCP Service\n', sub);
    save_system(app);
end
fprintf('%s und %s gespeichert.\n', app, lib);
end

function s = muxsel(mux, input)
% EXBAR_TRIPn_MUXSELECT: ein Zeichen je MUX 0..31 ('X' = aus, '0'..'3' = Eingang), MUX0 zuerst.
% EXBAR_TRIPn selbst waehlt nur, welcher MUX in der Oberflaeche angezeigt wird.
s = repmat('X', 1, 32);
s(mux + 1) = input;
end

function s = show(v)
if isnumeric(v) || islogical(v), s = num2str(v); else, s = char(v); end
end
