function foc_build_gpio_can_probe(overwrite)
%FOC_BUILD_GPIO_CAN_PROBE  Pruefung des CAN-Sende-/Empfangspfads MCU <-> ISO1050 ohne CAN-Controller.
%
%   foc_build_gpio_can_probe()        legt matlab/models/test/foc_gpio_can_probe.slx an
%   foc_build_gpio_can_probe(true)    vorhandenes Modell ersetzen
%
%   Hintergrund: CAN-A meldet Bit0Error -> Bus-off (reports/08_CAN_Test_2026-11-03.md); die
%   Platine kann den Bus nicht dominant ziehen. Dieses Modell treibt GPIO4 (CANTX-Netz) als
%   normalen GPIO:
%     abwechselnd 1 s ein 1-kHz-Rechteck (50 %, ISO1050 TXD-Dominant-Timeout wird nicht erreicht)
%     und 1 s dauerhaft high (rezessiv).
%   Erwartung bei intaktem Sendepfad: CAN_H - CAN_L (Klemme 2 - 3) im Mittel ca. 1 V in den
%   Ein-Phasen, ca. 0 V in den Aus-Phasen; ein Mitleser am Bus sieht nur in den Ein-Phasen Fehler.
%   GPIO5 (CANRX-Netz, ISO1050 RXD) wird gelesen und auf die LED GPIO56 gespiegelt.
%   Kein CAN-Controller, keine PWM. Takt wie app_speed (interner Oszillator 10 MHz -> 200 MHz).

if nargin < 1, overwrite = false; end
root = fileparts(fileparts(mfilename('fullpath')));
mdl = 'foc_gpio_can_probe';
mdlFile = fullfile(root, 'models', 'test', [mdl '.slx']);
if exist(mdlFile, 'file')
    if ~overwrite, error('foc:gpio', '%s existiert bereits.', mdlFile); end
    delete(mdlFile);
end
if bdIsLoaded(mdl), close_system(mdl, 0); end

ts = 0.25e-3;
new_system(mdl);
set_param(mdl, 'HardwareBoard', 'TI F2838x');
cs = getActiveConfigSet(mdl);
set_param(mdl, 'SolverType', 'Fixed-step', 'Solver', 'FixedStepDiscrete', 'FixedStep', num2str(ts), ...
    'StopTime', 'inf', 'ExtMode', 'off');
foc_probe_clock(cs);   % Takt wie app_speed (200 MHz)

% Rechteck 1 kHz und Freigabe 1 s ein / 1 s aus (abtastbasiert)
add_block('simulink/Sources/Pulse Generator', [mdl '/Square_1kHz'], 'PulseType', 'Sample based', ...
    'Period', '4', 'PulseWidth', '2', 'SampleTime', num2str(ts), 'Position', [40, 40, 90, 80]);
add_block('simulink/Sources/Pulse Generator', [mdl '/Enable_1s'], 'PulseType', 'Sample based', ...
    'Period', num2str(round(2 / ts)), 'PulseWidth', num2str(round(1 / ts)), 'SampleTime', num2str(ts), ...
    'Position', [40, 110, 90, 150]);
% GPIO4 = NOT(Square AND Enable): Aus-Phase -> high (rezessiv)
add_block('simulink/Logic and Bit Operations/Logical Operator', [mdl '/NAND'], 'Operator', 'NAND', ...
    'AllPortsSameDT', 'off', 'OutDataTypeStr', 'boolean', 'Position', [150, 70, 190, 120]);
add_line(mdl, 'Square_1kHz/1', 'NAND/1');
add_line(mdl, 'Enable_1s/1', 'NAND/2');
add_block('c2838xlib/Digital Output', [mdl '/GPIO4_TX'], 'Position', [240, 75, 320, 115], ...
    'GPIO_Number', 'GPIO0~GPIO7', 'GPIO_Bit0', 'off', 'GPIO_Bit4', 'on', 'GPIO_BIT4_Toggle', 'off');
add_line(mdl, 'NAND/1', 'GPIO4_TX/1');

% GPIO5 lesen -> LED
add_block('c2838xlib/Digital Input', [mdl '/GPIO5_RX'], 'Position', [40, 200, 120, 240], ...
    'GPIO_Number', 'GPIO0~GPIO7', 'GPIO_Bit0', 'off', 'GPIO_Bit5', 'on', 'sampleTime', '0.01');
add_block('c2838xlib/Digital Output', [mdl '/LED'], 'Position', [240, 200, 320, 240], ...
    'GPIO_Number', 'GPIO56~GPIO63', 'GPIO_Bit0', 'on', 'GPIO_BIT0_Toggle', 'off');
add_line(mdl, 'GPIO5_RX/1', 'LED/1');

save_system(mdl, mdlFile);
fprintf('Gespeichert: %s\n', mdlFile);
fprintf('  GPIO4: 1 kHz-Rechteck 1 s / high 1 s; GPIO5 -> LED GPIO56; Ts = %g s\n', ts);
pw = find_system(mdl, 'LookUnderMasks', 'all', 'Regexp', 'on', 'MaskType', 'ePWM|eCAN');
if ~isempty(pw), error('foc:gpio', 'Testmodell enthaelt ePWM-/eCAN-Bloecke.'); end
close_system(mdl, 0);
end
