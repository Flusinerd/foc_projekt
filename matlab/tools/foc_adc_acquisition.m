function foc_adc_acquisition()
%FOC_ADC_ACQUISITION  ADC-Abtastfenster verlaengern (Hardware-Review HW07).
%
%   foc_adc_acquisition()
%
%   Parameter "SOCAcquisitionWindow" der ADC-Bloecke (Einheit SYSCLK-Takte, 5 ns; ACQPS = Wert - 1):
%     Stroeme (Messung SOC0/1 und Offsetkalibrierung SOC2/3):  target_f2838.adc_acq_cur_ns
%     PVDD, GVDD, Temperatur, Poti:                            target_f2838.adc_acq_slow_ns
%   Bisher 65 ns (Strommessung), 75 ns (Kalibrierung), 35..75 ns (langsame Kanaele).
%   Alle Bloecke liegen in focLib (Input/Torque Controller, ADC Offset Function).
%   Nur Codegenerierung. Vorher/nachher werden ausgegeben; Modelle werden gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
b = foc_cfg_board_carpediem();
cur = num2str(round(b.adc_acq_cur_ns * b.CPU_frequency / 1e9));
slow = num2str(round(b.adc_acq_slow_ns * b.CPU_frequency / 1e9));

ic = [lib '/Input Controller'];
bv = find_system([ic '/BatteryVoltage_CodeGen'], 'LookUnderMasks', 'all', 'ReferenceBlock', 'c2802xlib/ADC');
blocks = {
    [lib '/Torque Controller/Iab_Meas_ADC/ADC'], cur
    [lib '/Torque Controller/Iab_Meas_ADC/ADC1'], cur
    [lib '/ADC Offset Function/Calculate ADC Offset /For Iterator Subsystem/ADC_B_IN0'], cur
    [lib '/ADC Offset Function/Calculate ADC Offset /For Iterator Subsystem/ADC_B_IN2'], cur
    [ic '/GVDD_Calculation/ADC'], slow
    [ic '/SpeedCommand_CodeGen/ADC1'], slow
    [ic '/Subsystem/ADC1'], slow
    bv{1}, slow};
set_param(lib, 'Lock', 'off');
for k = 1:size(blocks, 1)
    blk = blocks{k, 1};
    before = get_param(blk, 'SOCAcquisitionWindow');
    set_param(blk, 'SOCAcquisitionWindow', blocks{k, 2});
    fprintf('%-80s vorher %4s -> nachher %4s Takte (%s ns)\n', strrep(blk, newline, ' '), before, ...
        get_param(blk, 'SOCAcquisitionWindow'), num2str(str2double(blocks{k, 2}) * 1e9 / b.CPU_frequency));
end
save_system(lib);
% Die Kalibrierbloecke liegen in focLib/ADC Offset Function. Fruehere Fassung dieses Skripts hatte
% sie an der Verknuepfung in app_speed gesetzt (parametrierte Bibliotheksverknuepfung) - verwerfen.
close_system(app, 0);
load_system(fullfile(root, 'models', [app '.slx']));
lnk = [app '/ADC Offset Function'];
if ~isempty(get_param(lnk, 'LinkData'))
    fprintf('%s: lokale Parameter verworfen (LinkData)\n', lnk);
    set_param(lnk, 'LinkData', []);
end
save_system(app);
fprintf('%s und %s gespeichert.\n', lib, app);
end
