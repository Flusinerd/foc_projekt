function foc_build_can_probe(overwrite)
%FOC_BUILD_CAN_PROBE  CAN-Grundtest eCAN_A (GPIO4/5, ISO1050 U8), ohne PWM, ohne External Mode.
%
%   foc_build_can_probe()        legt matlab/models/test/foc_can_probe.slx an
%   foc_build_can_probe(true)    vorhandenes Modell ersetzen
%
%   Bitrate 1 Mbit/s wie app_speed (eCAN_A: Prescaler 20, TSEG1 5, TSEG2 4 bei 200 MHz).
%     Senden:  ID 0x1F0, 2 Byte (uint16-Zaehler, little endian), alle 200 ms, Mailbox 1
%     Echo:    Empfang ID 0x1F1 (Mailbox 9, uint16) -> Senden ID 0x1F2 (Mailbox 2), alle 200 ms
%   Heartbeat-LED GPIO56. Takt wie app_speed (interner Oszillator 10 MHz -> 200 MHz).
%   Am Bus: Klemme 1 = VCC2 (5 V extern), 2 = CAN_H, 3 = CAN_L, 4/5 = GND2;
%   siehe docs/Hardware-Ueberblick_Frequenzumrichter.md.

if nargin < 1, overwrite = false; end
root = fileparts(fileparts(mfilename('fullpath')));
mdl = 'foc_can_probe';
mdlFile = fullfile(root, 'models', 'test', [mdl '.slx']);
if exist(mdlFile, 'file')
    if ~overwrite, error('foc:can', '%s existiert bereits.', mdlFile); end
    delete(mdlFile);
end
if bdIsLoaded(mdl), close_system(mdl, 0); end

new_system(mdl);
set_param(mdl, 'HardwareBoard', 'TI F2838x');
cs = getActiveConfigSet(mdl);
set_param(mdl, 'SolverType', 'Fixed-step', 'Solver', 'FixedStepDiscrete', 'FixedStep', '0.01', ...
    'StopTime', 'inf', 'ExtMode', 'off');
ct = {'Clocking.UseInternalOsc', 1; 'Clocking.OSCCLK', 10; ...
      'eCAN_A.PinAssignment_Tx', 'GPIO4'; 'eCAN_A.PinAssignment_Rx', 'GPIO5'; ...
      'eCAN_A.BaudRatePrescaler', 20; 'eCAN_A.TSEG1', 5; 'eCAN_A.TSEG2', 4; ...
      'eCAN_A.SJW', 2; 'eCAN_A.SAM', 'Sample_one_time'; 'eCAN_A.SelfTestMode', 0};
for i = 1:size(ct, 1), codertarget.data.setParameterValue(cs, ct{i,1}, ct{i,2}); end

% Zaehler -> Tx 0x1F0
add_block('simulink/Sources/Counter Free-Running', [mdl '/Counter'], 'NumBits', '16', ...
    'tsamp', '0.2', 'Position', [40, 60, 100, 100]);
add_block('simulink/Signal Attributes/Data Type Conversion', [mdl '/ToUint16'], ...
    'OutDataTypeStr', 'uint16', 'Position', [140, 60, 200, 100]);
add_block('c2838xlib/eCAN Transmit', [mdl '/Tx_1F0'], 'useModule', 'eCAN_A', 'mailboxNo', '1', ...
    'messageID', 'hex2dec(''1F0'')', 'Position', [260, 55, 360, 105]);
add_line(mdl, 'Counter/1', 'ToUint16/1');
add_line(mdl, 'ToUint16/1', 'Tx_1F0/1');
% Echo: Rx 0x1F1 -> Tx 0x1F2
add_block('c2838xlib/eCAN Receive', [mdl '/Rx_1F1'], 'useModule', 'eCAN_A', 'mailboxNo', '9', ...
    'messageID', 'hex2dec(''1F1'')', 'sampleTime', '0.2', 'dataType', 'uint16', ...
    'Position', [40, 160, 160, 220]);
add_block('c2838xlib/eCAN Transmit', [mdl '/Tx_1F2'], 'useModule', 'eCAN_A', 'mailboxNo', '2', ...
    'messageID', 'hex2dec(''1F2'')', 'Position', [260, 165, 360, 215]);
phR = get_param([mdl '/Rx_1F1'], 'PortHandles');
% Ausgaenge des Empfangsblocks: Port 1 = Status (neue Nachricht), Port 2 = Daten (falls vorhanden)
dataPort = phR.Outport(end);
add_line(mdl, dataPort, get_param([mdl '/Tx_1F2'], 'PortHandles').Inport(1), 'autorouting', 'on');
for k = 1:numel(phR.Outport) - 1
    t = sprintf('%s/T%d', mdl, k);
    add_block('simulink/Sinks/Terminator', t, 'Position', [200, 230 + 30*k, 220, 250 + 30*k]);
    add_line(mdl, phR.Outport(k), get_param(t, 'PortHandles').Inport(1), 'autorouting', 'on');
end
% Heartbeat-LED
add_block('c2838xlib/Digital Output', [mdl '/LED'], 'Position', [260, 320, 340, 360], ...
    'GPIO_Number', 'GPIO56~GPIO63', 'GPIO_Bit0', 'on', 'GPIO_BIT0_Toggle', 'on');
add_block('simulink/Sources/Constant', [mdl '/One'], 'Value', '1', 'SampleTime', '0.25', ...
    'OutDataTypeStr', 'boolean', 'Position', [100, 325, 140, 355]);
add_line(mdl, 'One/1', 'LED/1');

save_system(mdl, mdlFile);
fprintf('Gespeichert: %s\n', mdlFile);
for p = {'eCAN_A.PinAssignment_Tx', 'eCAN_A.PinAssignment_Rx', 'eCAN_A.BaudRate', 'Clocking.ClosestCpuClock', 'Runtime.BuildAction'}
    v = codertarget.data.getParameterValue(cs, p{1}); if isnumeric(v), v = num2str(v); end
    fprintf('  %-24s = %s\n', p{1}, v);
end
fprintf('  Empfangsblock: %d Ausgaenge (letzter = Daten)\n', numel(phR.Outport));
pw = find_system(mdl, 'LookUnderMasks', 'all', 'Regexp', 'on', 'MaskType', 'ePWM');
if ~isempty(pw), error('foc:can', 'Testmodell enthaelt ePWM-Bloecke.'); end
close_system(mdl, 0);
end
