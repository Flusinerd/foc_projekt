function foc_focp_speedmode()
%FOC_FOCP_SPEEDMODE  speedMode im FOCP-Betrieb von der Firmware waehlen lassen (2026-11-24).
%
%   foc_startup('datasheet'); foc_focp_speedmode()
%
%   Befund: Ohne CAN-Botschaft 0x102 steht speedMode auf 1, der Speed calculator begrenzt
%   N_ref auf floor(N_base/3) = 1333 rpm. Ein FOCP-Sollwert von 2000 rpm wurde stillschweigend
%   auf 1333 rpm gekappt (matlab/reports/09_Hall_Qualifikation_2026-11-10.md).
%
%   app_speed/FOCP/FOCP Service bekommt den Ausgang mode_out:
%     Simulation: mode_out = mode (unveraendert durchgereicht, Aequivalenz bleibt bitgleich)
%     Code:       focp_c28x_step(..., &mode_out); im Poti-Betrieb = Eingang, im FOCP-Betrieb die
%                 kleinste Stufe 1..3, deren Bereich den Sollwert abdeckt.
%   Neuer Ausgang FOCP/speedMode_out -> Speed Control Eingang speedMode (statt Rate Transition4).
%   Speed Control und die MIL-Harnesses bleiben unveraendert. Vorher/nachher werden ausgegeben,
%   das Modell wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
app = 'app_speed';
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
sub = [app '/FOCP'];
sc = [app '/Speed Control'];
fcn = [sub '/FOCP Service'];
iMode = 6;                                     % Speed Control Eingang speedMode
fprintf('vorher:  %s\n', state());

if getSimulinkBlockHandle([sub '/speedMode_out']) == -1
    ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fcn);
    ch.Script = strjoin({
        'function [active, freemode, ref, mode_out] = focp_service(uv, fm_can, hot, safe, mode, speed_pu, n_base, udc, idq_ref, idq, i_base, hall, v_base, temp, ov, i_trip, i_sum_max)'
        '% FOCP fuer FOC Studio, nur im erzeugten Code (firmware/focp_c28x.c). Simulation: inaktiv,'
        '% speedMode wird unveraendert durchgereicht.'
        '% idq wird nicht mehr benutzt (Stroeme kommen aus focp_c28x_isr im Stromregler-Interrupt).'
        'active = false; freemode = false; ref = single(0); mode_out = mode;'
        'if coder.target(''Rtw'')'
        '    coder.cinclude(''focp_c28x.h'');'
        '    a = uint16(0); f = uint16(0); r = single(0); md = uint16(0);'
        '    iref = single(idq_ref);'
        '    coder.ceval(''focp_c28x_step'', uint16(uv), uint16(ov), uint16(fm_can), uint16(hot), uint16(safe), ...'
        '        uint16(mode), single(speed_pu), single(n_base), single(udc), coder.rref(iref), ...'
        '        single(i_base), single(v_base), single(temp), uint16(hall), single(i_trip), ...'
        '        single(i_sum_max), coder.wref(a), coder.wref(f), coder.wref(r), coder.wref(md));'
        '    active = a ~= 0; freemode = f ~= 0; ref = r; mode_out = cast(md, ''like'', mode);'
        'end'
        'end'}, newline);
    d = ch.find('-isa', 'Stateflow.Data', 'Name', 'mode_out');
    d.DataType = 'Inherit: Same as Simulink';
    p = get_param(fcn, 'Position');
    add_block('simulink/Sinks/Out1', [sub '/speedMode_out'], 'Port', '3', ...
        'Position', [p(3) + 260, p(4) - 20, p(3) + 290, p(4) - 6]);
    add_line(sub, 'FOCP Service/4', 'speedMode_out/1', 'autorouting', 'on');

    ph = get_param(sc, 'PortHandles');
    l = get_param(ph.Inport(iMode), 'Line');
    delete_line(l);
    fph = get_param(sub, 'PortHandles');
    add_line(app, fph.Outport(3), ph.Inport(iMode), 'autorouting', 'on');
    save_system(app);
end
fprintf('nachher: %s\n', state());

    function s = state()
        ph_ = get_param(sc, 'PortHandles');
        l_ = get_param(ph_.Inport(iMode), 'Line');
        src = '-';
        if l_ ~= -1
            src = sprintf('%s:%d', getfullname(get_param(l_, 'SrcBlockHandle')), ...
                get_param(get_param(l_, 'SrcPortHandle'), 'PortNumber'));
        end
        s = sprintf('Speed Control/speedMode <- %s, FOCP Ausgaenge %d', src, ...
            numel(find_system(sub, 'SearchDepth', 1, 'BlockType', 'Outport')));
    end
end
