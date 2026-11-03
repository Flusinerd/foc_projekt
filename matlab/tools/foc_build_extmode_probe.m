function foc_build_extmode_probe(comPort, overwrite)
%FOC_BUILD_EXTMODE_PROBE  Pruefmodell fuer External Mode auf der CarPediem-Platine (ohne PWM).
%
%   foc_build_extmode_probe()                COM-Port 'COM4' (XDS100 Class USB Serial Port)
%   foc_build_extmode_probe(comPort, true)   vorhandenes Pruefmodell ersetzen
%
%   Legt matlab/models/test/foc_extmode_probe.slx als FRISCHES Modell an (Board TI F2838x,
%   Bloecke direkt aus c2838xlib; v1 mit aus app_speed kopierten Bloecken scheiterte beim
%   Linken, siehe reports/08_ExtMode_Test_2026-11-03.md):
%     - Takt wie app_speed (interner Oszillator 10 MHz, PLL auto, 200 MHz)
%     - External Mode: XCP on Serial ueber SCI_A auf GPIO84/85 (FT2232 Kanal B, USB), 115200 Baud
%       (app_speed nutzt SCI_B auf GPIO9/11 - GPIO11 ist ePWM6B, Low-Side-Gate Phase U!)
%     - KEINE ePWM-Bloecke: die Gate-Eingaenge des DRV8300 bleiben im Reset-Zustand
%     - ADC per Software-Trigger, 1 ms: I_A = ADCINB0, I_B = ADCINB2, PVDD = ADCINA5, GVDD = ADCINC3
%     - Heartbeat-LED GPIO56 (Toggle alle 0,25 s)
%   Alle ADC-Ausgaenge werden geloggt (External Mode -> Simulation Data Inspector).

if nargin < 1 || isempty(comPort), comPort = 'COM4'; end
if nargin < 2, overwrite = false; end
root = fileparts(fileparts(mfilename('fullpath')));
outDir = fullfile(root, 'models', 'test');
mdl = 'foc_extmode_probe';
mdlFile = fullfile(outDir, [mdl '.slx']);
if exist(mdlFile, 'file')
    if ~overwrite, error('foc:probe', '%s existiert bereits (overwrite = true zum Ersetzen).', mdlFile); end
    delete(mdlFile);
end
if ~exist(outDir, 'dir'), mkdir(outDir); end
if bdIsLoaded(mdl), close_system(mdl, 0); end

new_system(mdl);
set_param(mdl, 'HardwareBoard', 'TI F2838x');
cs = getActiveConfigSet(mdl);
set_param(mdl, 'SolverType', 'Fixed-step', 'Solver', 'FixedStepDiscrete', 'FixedStep', '1e-3', 'StopTime', 'inf');
foc_probe_clock(cs);   % Takt wie app_speed (200 MHz)
setCT(cs, 'SCI_A.PinAssignment_Tx', 'GPIO84');
setCT(cs, 'SCI_A.PinAssignment_Rx', 'GPIO85');
setCT(cs, 'SCI_A.UserBaudRate', 115200);
set_param(mdl, 'ExtMode', 'on');
setCT(cs, 'ExtMode.Configuration', 'XCP on Serial');
setCT(cs, 'ExtMode.SCIModule', 'SCI_A');
setCT(cs, 'ExtModeCOMPort', comPort);
% R2023a/C2000: ert_main.c ruft extmodeEvent/ParseArgs/Reset auf, die Build-Info des Targets
% bindet die definierende Datei xcp_ext_mode.c aber nicht ein (Linkerfehler "unresolved symbols").
% Gezielt als Custom-Code-Quelle ergaenzen (nur dieses Pruefmodell, Toolbox unveraendert).
xcpSrc = fullfile(matlabroot, 'toolbox', 'coder', 'xcp', 'src', 'target', 'ext_mode', 'src', 'xcp_ext_mode.c');
set_param(mdl, 'CustomSource', ['"' xcpSrc '"']);

adc = { ... % Modul, SOC, Kanal, Signalname
    'B', 'SOC0', 'ADCIN0', 'I_A_counts'; ...
    'B', 'SOC1', 'ADCIN2', 'I_B_counts'; ...
    'A', 'SOC0', 'ADCIN5', 'PVDD_counts'; ...
    'C', 'SOC0', 'ADCIN3', 'GVDD_counts'};
for i = 1:size(adc, 1)
    b = sprintf('%s/ADC_%s_%s', mdl, adc{i,1}, adc{i,3});
    add_block('c2838xlib/ADC', b, 'Position', [60, 40 + 80*i, 170, 90 + 80*i]);
    set_param(b, 'ADCModule', adc{i,1}, 'useSOC1', adc{i,2}, 'conv1_Type4', adc{i,3}, ...
        'SOCTriggerSource', 'Software', 'Ts', '1e-3');
    t = sprintf('%s/T%d', mdl, i);
    add_block('simulink/Sinks/Terminator', t, 'Position', [260, 55 + 80*i, 280, 75 + 80*i]);
    ph = get_param(b, 'PortHandles');
    add_line(mdl, ph.Outport(1), get_param(t, 'PortHandles').Inport(1));
    set_param(ph.Outport(1), 'DataLogging', 'on', 'DataLoggingNameMode', 'Custom', 'DataLoggingName', adc{i,4});
end
add_block('c2838xlib/Digital Output', [mdl '/LED'], 'Position', [260, 440, 340, 480], ...
    'GPIO_Number', 'GPIO56~GPIO63', 'GPIO_Bit0', 'on', 'GPIO_BIT0_Toggle', 'on');
add_block('simulink/Sources/Constant', [mdl '/One'], 'Value', '1', 'SampleTime', '0.25', ...
    'OutDataTypeStr', 'boolean', 'Position', [100, 445, 140, 475]);
add_line(mdl, 'One/1', 'LED/1');

save_system(mdl, mdlFile);
fprintf('Gespeichert: %s\n', mdlFile);
for p = {'Clocking.UseInternalOsc', 'Clocking.OSCCLK', 'Clocking.ClosestCpuClock', 'SCI_A.PinAssignment_Tx', ...
         'SCI_A.PinAssignment_Rx', 'SCI_A.BaudRate', 'ExtMode.Configuration', 'ExtMode.SCIModule', 'ExtModeCOMPort', ...
         'Runtime.DeviceID', 'Runtime.BuildAction'}
    v = codertarget.data.getParameterValue(cs, p{1}); if isnumeric(v) || islogical(v), v = num2str(v); end
    fprintf('  %-26s = %s\n', p{1}, v);
end
fprintf('  ExtMode = %s\n', get_param(mdl, 'ExtMode'));
pw = find_system(mdl, 'LookUnderMasks', 'all', 'Regexp', 'on', 'MaskType', 'ePWM');
fprintf('ePWM-Bloecke im Pruefmodell: %d (muss 0 sein)\n', numel(pw));
if ~isempty(pw), error('foc:probe', 'Pruefmodell enthaelt ePWM-Bloecke.'); end
close_system(mdl, 0);
end

function setCT(cs, p, v)
codertarget.data.setParameterValue(cs, p, v);
end
