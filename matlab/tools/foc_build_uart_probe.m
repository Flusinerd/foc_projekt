function foc_build_uart_probe(overwrite, txPin, rxPin)
%FOC_BUILD_UART_PROBE  UART-Grundtest SCI_A <-> FT2232 Kanal B (COM4), ohne PWM, ohne External Mode.
%
%   foc_build_uart_probe()        legt matlab/models/test/foc_uart_probe.slx an
%   foc_build_uart_probe(true)    vorhandenes Modell ersetzen
%
%   Ein uint8-Zaehler (alle 10 ms) wird ueber SCI_A (GPIO84 TX / GPIO85 RX, 115200 Bd) als
%   Rahmen 'S' <n> 'E' gesendet. Auf COM4 muss die Folge S 00 E S 01 E ... ankommen. Damit ist
%   die Hardwarestrecke unabhaengig von XCP/External Mode geprueft
%   (reports/08_ExtMode_Test_2026-11-03.md, Nachtrag 3). Heartbeat-LED GPIO56 wie im Pruefmodell.
%   Takt wie app_speed (interner Oszillator 10 MHz -> 200 MHz).

if nargin < 1, overwrite = false; end
% Pins: Laut Schaltplan MA Darici (Abb. 5.8) gehen FT2232 BDBUS0/1 auf die Netze USB_P/USB_N,
% vermutlich GPIO43 (USB0DP, SCIA_RX) / GPIO42 (USB0DM, SCIA_TX). GPIO84/85 lieferte nichts.
if nargin < 2 || isempty(txPin), txPin = 'GPIO42'; end
if nargin < 3 || isempty(rxPin), rxPin = 'GPIO43'; end
root = fileparts(fileparts(mfilename('fullpath')));
mdl = 'foc_uart_probe';
mdlFile = fullfile(root, 'models', 'test', [mdl '.slx']);
if exist(mdlFile, 'file')
    if ~overwrite, error('foc:uart', '%s existiert bereits.', mdlFile); end
    delete(mdlFile);
end
if bdIsLoaded(mdl), close_system(mdl, 0); end

new_system(mdl);
set_param(mdl, 'HardwareBoard', 'TI F2838x');
cs = getActiveConfigSet(mdl);
set_param(mdl, 'SolverType', 'Fixed-step', 'Solver', 'FixedStepDiscrete', 'FixedStep', '1e-3', ...
    'StopTime', 'inf', 'ExtMode', 'off');
foc_probe_clock(cs);   % Takt wie app_speed (200 MHz)
codertarget.data.setParameterValue(cs, 'SCI_A.PinAssignment_Tx', txPin);
codertarget.data.setParameterValue(cs, 'SCI_A.PinAssignment_Rx', rxPin);
if ~strcmp(codertarget.data.getParameterValue(cs, 'SCI_A.PinAssignment_Tx'), txPin) || ...
        ~strcmp(codertarget.data.getParameterValue(cs, 'SCI_A.PinAssignment_Rx'), rxPin)
    error('foc:uart', 'Pinzuordnung %s/%s fuer SCI_A nicht uebernommen.', txPin, rxPin);
end
codertarget.data.setParameterValue(cs, 'SCI_A.UserBaudRate', 115200);

add_block('simulink/Sources/Counter Free-Running', [mdl '/Counter'], 'NumBits', '8', ...
    'tsamp', '0.01', 'Position', [60, 60, 120, 100]);
add_block('simulink/Signal Attributes/Data Type Conversion', [mdl '/ToUint8'], ...
    'OutDataTypeStr', 'uint8', 'Position', [160, 60, 220, 100]);
add_block('c2838xlib/SCI Transmit', [mdl '/SCI_A_Tx'], 'sciModule', 'A', ...
    'dataHead', '''S''', 'dataTail', '''E''', 'frameSize', '1', 'Position', [260, 55, 360, 105]);
add_line(mdl, 'Counter/1', 'ToUint8/1');
add_line(mdl, 'ToUint8/1', 'SCI_A_Tx/1');
add_block('c2838xlib/Digital Output', [mdl '/LED'], 'Position', [260, 160, 340, 200], ...
    'GPIO_Number', 'GPIO56~GPIO63', 'GPIO_Bit0', 'on', 'GPIO_BIT0_Toggle', 'on');
add_block('simulink/Sources/Constant', [mdl '/One'], 'Value', '1', 'SampleTime', '0.25', ...
    'OutDataTypeStr', 'boolean', 'Position', [100, 165, 140, 195]);
add_line(mdl, 'One/1', 'LED/1');

save_system(mdl, mdlFile);
fprintf('Gespeichert: %s\n', mdlFile);
for p = {'SCI_A.PinAssignment_Tx', 'SCI_A.PinAssignment_Rx', 'SCI_A.BaudRate', 'Clocking.ClosestCpuClock', 'Runtime.BuildAction'}
    v = codertarget.data.getParameterValue(cs, p{1}); if isnumeric(v), v = num2str(v); end
    fprintf('  %-24s = %s\n', p{1}, v);
end
pw = find_system(mdl, 'LookUnderMasks', 'all', 'Regexp', 'on', 'MaskType', 'ePWM');
if ~isempty(pw), error('foc:uart', 'Testmodell enthaelt ePWM-Bloecke.'); end
close_system(mdl, 0);
end
