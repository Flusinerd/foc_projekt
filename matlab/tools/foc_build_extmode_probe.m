function foc_build_extmode_probe(comPort)
%FOC_BUILD_EXTMODE_PROBE  Pruefmodell fuer External Mode auf der CarPediem-Platine (ohne PWM).
%
%   foc_build_extmode_probe()          COM-Port 'COM4' (XDS100 Class USB Serial Port)
%   foc_build_extmode_probe(comPort)
%
%   Legt matlab/models/test/foc_extmode_probe.slx an:
%     - Konfiguration kopiert aus app_speed (Board TI F2838x, Takt, Toolchain, Load-Befehl)
%     - External Mode: XCP on Serial ueber SCI_A (GPIO84/85 = FT2232 Kanal B, USB), 115200 Baud
%       (app_speed nutzt SCI_B auf GPIO9/11 - GPIO11 ist ePWM6B, Low-Side-Gate Phase U!)
%     - KEINE ePWM-Bloecke: die Gate-Eingaenge des DRV8300 bleiben im Reset-Zustand
%     - liest per Software-Trigger (1 ms): I_A (ADCINB0), I_B (ADCINB2), PVDD (ADCINA5),
%       GVDD (ADCINC3) - Bloecke kopiert aus app_speed; Heartbeat-LED GPIO56 (2 Hz)
%   Alle ADC-Ausgaenge werden geloggt (External Mode -> Simulation Data Inspector).
%   Bricht ab, wenn das Modell schon existiert.

if nargin < 1 || isempty(comPort), comPort = 'COM4'; end
root = fileparts(fileparts(mfilename('fullpath')));
outDir = fullfile(root, 'models', 'test');
mdl = 'foc_extmode_probe';
mdlFile = fullfile(outDir, [mdl '.slx']);
if exist(mdlFile, 'file'), error('foc:probe', '%s existiert bereits.', mdlFile); end
if ~exist(outDir, 'dir'), mkdir(outDir); end

src = 'app_speed';
load_system(src);
new_system(mdl);
cs = getActiveConfigSet(src).copy;
cs.Name = 'probe_config';
attachConfigSet(mdl, cs);
setActiveConfigSet(mdl, 'probe_config');

% ---- Solver, Laufzeit, External Mode -----------------------------------------------------
set_param(mdl, 'SolverType', 'Fixed-step', 'Solver', 'FixedStepDiscrete', 'FixedStep', '1e-3', ...
    'StopTime', 'inf', 'ExtMode', 'on');
codertarget.data.setParameterValue(cs, 'ExtMode.Configuration', 'XCP on Serial');
codertarget.data.setParameterValue(cs, 'ExtMode.SCIModule', 'SCI_A');
codertarget.data.setParameterValue(cs, 'ExtModeCOMPort', comPort);
codertarget.data.setParameterValue(cs, 'SCI_A.UserBaudRate', 115200);

% ---- Bloecke ------------------------------------------------------------------------------
sig = { ...
    [src '/ADC Offset Function/Calculate ADC Offset /For Iterator Subsystem/ADC_B_IN0'], 'I_A_counts'; ...
    [src '/ADC Offset Function/Calculate ADC Offset /For Iterator Subsystem/ADC_B_IN2'], 'I_B_counts'; ...
    [src '/Input Controller/BatteryVoltage_CodeGen/ '],                                  'PVDD_counts'; ...
    [src '/Input Controller/GVDD_Calculation/ADC'],                                      'GVDD_counts'};
for i = 1:size(sig, 1)
    dst = sprintf('%s/ADC_%d', mdl, i);
    add_block(sig{i,1}, dst, 'Position', [60, 40 + 80*i, 160, 90 + 80*i]);
    set_param(dst, 'LinkStatus', 'none');
    trySet(dst, 'SampleTime', '1e-3'); trySet(dst, 'sampleTime', '1e-3');
    term = sprintf('%s/T%d', mdl, i);
    add_block('simulink/Sinks/Terminator', term, 'Position', [260, 55 + 80*i, 280, 75 + 80*i]);
    ph = get_param(dst, 'PortHandles');
    add_line(mdl, ph.Outport(1), get_param(term, 'PortHandles').Inport(1));
    set_param(ph.Outport(1), 'DataLogging', 'on', 'DataLoggingNameMode', 'Custom', 'DataLoggingName', sig{i,2});
end
add_block([src '/Heartbeat LED/Digital Output'], [mdl '/LED'], 'Position', [260, 440, 340, 480]);
set_param([mdl '/LED'], 'LinkStatus', 'none');
add_block('simulink/Sources/Constant', [mdl '/One'], 'Value', '1', 'SampleTime', '0.25', ...
    'OutDataTypeStr', 'boolean', 'Position', [100, 445, 140, 475]);
add_line(mdl, 'One/1', 'LED/1');

close_system(src, 0);
save_system(mdl, mdlFile);
fprintf('Gespeichert: %s\n', mdlFile);
fprintf('External Mode: %s, SCI=%s, COM=%s, Baud=%s\n', ...
    codertarget.data.getParameterValue(cs, 'ExtMode.Configuration'), ...
    codertarget.data.getParameterValue(cs, 'ExtMode.SCIModule'), ...
    codertarget.data.getParameterValue(cs, 'ExtModeCOMPort'), ...
    num2str(codertarget.data.getParameterValue(cs, 'SCI_A.UserBaudRate')));
pw = find_system(mdl, 'LookUnderMasks', 'all', 'Regexp', 'on', 'MaskType', 'ePWM');
fprintf('ePWM-Bloecke im Pruefmodell: %d (muss 0 sein)\n', numel(pw));
if ~isempty(pw), error('foc:probe', 'Pruefmodell enthaelt ePWM-Bloecke.'); end
close_system(mdl, 0);
end

function trySet(b, p, v)
try, set_param(b, p, v); catch, end
end
