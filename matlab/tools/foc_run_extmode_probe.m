function foc_run_extmode_probe(tRun, doBuild)
%FOC_RUN_EXTMODE_PROBE  External-Mode-Test mit foc_extmode_probe auf der Hardware (HARDWAREZUGRIFF).
%
%   foc_run_extmode_probe()               bauen, flashen, 5 s aufzeichnen
%   foc_run_extmode_probe(tRun)
%   foc_run_extmode_probe(tRun, false)    NICHT flashen, nur verbinden (z. B. nach Aus-/Einschalten
%                                         der Platine; CCS 21: nach Flash-Boot Power-Cycle noetig)
%
%   Nur mit Freigabe des Menschen ausfuehren (AGENTS.md Regel 4). Baut das Pruefmodell,
%   laedt es ueber den XDS100 (Build, load and run), verbindet External Mode (XCP/SCI_A),
%   zeichnet tRun Sekunden auf und trennt. Das Pruefmodell enthaelt keine PWM.
%   Ausgabe: Mittelwert/Min/Max der ADC-Werte und Umrechnung (Strom, PVDD, GVDD).

if nargin < 1 || isempty(tRun), tRun = 5; end
if nargin < 2 || isempty(doBuild), doBuild = true; end
root = fileparts(fileparts(mfilename('fullpath')));
mdl = 'foc_extmode_probe';
load_system(fullfile(root, 'models', 'test', [mdl '.slx']));
cleanup = onCleanup(@() close_system(mdl, 0));
ph = find_system(mdl, 'FindAll', 'on', 'Type', 'port', 'DataLogging', 'on');
for i = 1:numel(ph)
    set_param(ph(i), 'DataLoggingDecimateData', 'on', 'DataLoggingDecimation', '10');
end
set_param(mdl, 'SimulationMode', 'external');
Simulink.sdi.clear;

if doBuild
    fprintf('Build, Load and Run (XDS100) ...\n');
    slbuild(mdl);
else
    fprintf('Kein Build/Flash, nur Verbindung.\n');
end
fprintf('Verbinde External Mode ...\n');
try
    set_param(mdl, 'SimulationCommand', 'connect');
catch err
    fprintf('connect FEHLER: %s\n', err.message);
    rethrow(err);
end
pause(2);
fprintf('Status nach connect: %s\n', get_param(mdl, 'SimulationStatus'));
set_param(mdl, 'SimulationCommand', 'start');
pause(tRun);
set_param(mdl, 'SimulationCommand', 'stop');
pause(1);
try, set_param(mdl, 'SimulationCommand', 'disconnect'); catch, end
fprintf('Status: %s\n', get_param(mdl, 'SimulationStatus'));

ids = Simulink.sdi.getAllRunIDs;
if isempty(ids), error('foc:probe', 'Keine Daten im Simulation Data Inspector.'); end
r = Simulink.sdi.getRun(ids(end));
vref = 3.36; full = 4095;
for k = 1:r.SignalCount
    s = r.getSignalByIndex(k); v = double(s.Values.Data(:));
    fprintf('%-12s N=%5d  Mittel=%7.1f  Min=%5d  Max=%5d', s.Name, numel(v), mean(v), min(v), max(v));
    u = mean(v) * vref / full;
    switch s.Name
        case {'I_A_counts', 'I_B_counts'}
            fprintf('  -> %.3f V, Offset-Abweichung %+.1f Counts = %+.2f A\n', u, mean(v) - 2047, (mean(v) - 2047)/2048*56.0);
        case 'PVDD_counts'
            fprintf('  -> %.3f V am Pin, PVDD = %.2f V (Teiler 100,61k/3,01k)\n', u, u*(100.61 + 3.01)/3.01);
        otherwise
            fprintf('  -> %.3f V am Pin\n', u);
    end
end
end
