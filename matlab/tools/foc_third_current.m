function foc_third_current()
%FOC_THIRD_CURRENT  Dritten Phasenstrom I_C (ADCINB3) messen und auf Plausibilitaet pruefen (HW06, A3).
%
%   foc_startup('datasheet'); foc_third_current()
%
%   focLib/Torque Controller/Iab_Meas_ADC: neuer ADC-Block "ADC_IC" (Kopie von "ADC": ADCB SOC4,
%     Trigger ePWM6 SOCA, gleiches Abtastfenster) auf ADCIN3 -> neuer Ausgang "Ic_ADC" (Rohwert).
%   focLib/Torque Controller: Ic_ADC -> neuer Eingang "Ic_ADC" von FOC -> FOCP ISR (5. Eingang,
%     uint16). Offset, Skalierung und Plausibilitaet |ia+ib+ic| > ctrl_limits.I_sum_max (1 ms)
%     rechnet firmware/focp_c28x.c (Fehler "Stromsumme", gemeinsamer Trip mit dem Ueberstrom).
%   app_speed/FOCP: Konstante ctrl_limits.I_sum_max als 17. Eingang von FOCP Service.
%   Die Regelung nutzt weiterhin nur I_A/I_B; die Simulation ist nicht betroffen (die ADC-Bloecke
%   und die MATLAB Functions wirken nur im erzeugten Code). Vorher/nachher werden ausgegeben;
%   Modelle werden gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
tc = [lib '/Torque Controller'];
meas = [tc '/Iab_Meas_ADC'];
foc = [tc '/FOC'];
fprintf('vorher:  %s\n', state());
set_param(lib, 'Lock', 'off');
if getSimulinkBlockHandle([meas '/ADC_IC']) == -1
    p = get_param([meas '/ADC1'], 'Position');
    add_block([meas '/ADC'], [meas '/ADC_IC'], 'Position', p + [0 80 0 80]);
    set_param([meas '/ADC_IC'], 'conv1_Type4', 'ADCIN3', 'useSOC1', 'SOC4', 'postInterrupt', 'off');
    add_block('simulink/Sinks/Out1', [meas '/Ic_ADC'], 'Port', '2', 'Position', p + [200 90 230 70]);
    add_line(meas, 'ADC_IC/1', 'Ic_ADC/1', 'autorouting', 'on');

    nIn = numel(find_system(foc, 'SearchDepth', 1, 'BlockType', 'Inport'));
    q = get_param([foc '/FOCP ISR'], 'Position');
    add_block('simulink/Sources/In1', [foc '/Ic_ADC'], 'Port', num2str(nIn + 1), ...
        'Position', [q(1) - 160, q(4) + 20, q(1) - 130, q(4) + 34]);
    fcn = [foc '/FOCP ISR'];
    ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
    ch.Script = strjoin({
        'function focp_isr(iab, pos, idq, vdq, ic_raw)'
        '% FOCP-Telemetrie und Schutz im Stromregler-Interrupt, nur im erzeugten Code (firmware/focp_c28x.c).'
        'if coder.target(''Rtw'')'
        '    coder.cinclude(''focp_c28x.h'');'
        '    coder.ceval(''focp_c28x_isr'', coder.rref(iab), pos, coder.rref(idq), coder.rref(vdq), ic_raw);'
        'end'
        'end'}, newline);
    in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
    for k = 1:numel(in)
        if strcmp(in(k).Name, 'ic_raw')
            in(k).DataType = 'uint16';
        else
            in(k).DataType = 'single';
            if ~strcmp(in(k).Name, 'pos'), in(k).Props.Array.Size = '2'; end
        end
    end
    add_line(foc, 'Ic_ADC/1', 'FOCP ISR/5', 'autorouting', 'on');
    mph = get_param(meas, 'PortHandles');
    fph = get_param(foc, 'PortHandles');
    add_line(tc, mph.Outport(2), fph.Inport(nIn + 1), 'autorouting', 'on');
    save_system(lib);
end
if ~strcmp(get_param([meas '/ADC_IC'], 'useSOC1'), 'SOC4')   % ADCB: SOC0/1 = I_A/I_B, SOC2/3 = Offsetkalibrierung, SOC4 frei
    set_param([meas '/ADC_IC'], 'useSOC1', 'SOC4');
    save_system(lib);
end

if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
sub = [app '/FOCP'];
if getSimulinkBlockHandle([sub '/I_sum_max']) == -1
    add_block('simulink/Sources/Constant', [sub '/I_sum_max'], 'Value', 'ctrl_limits.I_sum_max', ...
        'OutDataTypeStr', 'single', 'SampleTime', '-1', 'Position', [120, 780, 180, 800]);
    fcn = [sub '/FOCP Service'];
    ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
    ch.Script = strjoin({
        'function [active, freemode, ref] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base, udc, idq_ref, idq, i_base, hall, v_base, temp, ov, i_trip, i_sum_max)'
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
        '        single(i_sum_max), coder.wref(a), coder.wref(f), coder.wref(r));'
        '    active = a ~= 0; freemode = f ~= 0; ref = r;'
        'end'
        'end'}, newline);
    in = ch.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
    for k = 1:numel(in)
        if any(strcmp(in(k).Name, {'idq_ref', 'idq'})), in(k).Props.Array.Size = '2'; end
    end
    add_line(sub, 'I_sum_max/1', 'FOCP Service/17');
    save_system(app);
end
fprintf('nachher: %s\n', state());
fprintf('%s und %s gespeichert.\n', lib, app);

    function s = state()
        s = sprintf('ADC_IC %s, Iab_Meas_ADC %d Ausgaenge, FOC %d Eingaenge', ...
            yesno(getSimulinkBlockHandle([meas '/ADC_IC']) ~= -1), ...
            numel(find_system(meas, 'SearchDepth', 1, 'BlockType', 'Outport')), ...
            numel(find_system(foc, 'SearchDepth', 1, 'BlockType', 'Inport')));
    end
end

function s = yesno(b), if b, s = 'ja'; else, s = 'nein'; end, end
