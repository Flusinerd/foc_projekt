function foc_fix_sim_sensor(libFile)
%FOC_FIX_SIM_SENSOR  Polaritaet der Strommessung im Streckenmodell (nur Simulationspfad).
%
%   foc_fix_sim_sensor()
%
%   Das Sensormodell in "Inverter and Motor - Plant Model/Simulation/Sensor_Measurements"
%   rechnet Strangstrom -> CSA-Spannung mit +ISenseVoltPerAmp. Das EVM liest Strom in den
%   Motor aber negativ (invertingAmp = -1, Schaltplan SLVC817). Ohne diese Umkehr ist der
%   Stromregelkreis in der Simulation mitgekoppelt (Grenzzyklus in P1b, siehe
%   matlab/reports/05_P1b_Diagnose_2026-11-03.md).
%
%   Neu: Gain = inverter_DRV8300.invertingAmp * inverter_DRV8300.ISenseVoltPerAmp
%   Profil legacy_f2838 hat invertingAmp = 1 -> dort bitgleich zum bisherigen Verhalten.
%   Codegenerierungspfad unveraendert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(libFile), libFile = fullfile(root, 'models', 'focLib.slx'); end

lib = 'focLib';
if ~bdIsLoaded(lib), load_system(libFile); end
set_param(lib, 'Lock', 'off');

blk = [lib '/Inverter and Motor - Plant Model/Simulation/Sensor_Measurements/Get Current Sense Voltage'];
if getSimulinkBlockHandle(blk) == -1
    error('foc:sensor', 'Block nicht gefunden: %s', blk);
end
newGain = 'inverter_DRV8300.invertingAmp*inverter_DRV8300.ISenseVoltPerAmp';
before = get_param(blk, 'Gain');
if strcmp(before, newGain)
    fprintf('%s\n  bereits korrigiert: Gain=%s\n', blk, before);
    return;
end
set_param(blk, 'Gain', newGain);
save_system(lib);
fprintf('%s\n  vorher:  Gain=%s\n  nachher: Gain=%s\n', blk, before, get_param(blk, 'Gain'));
fprintf('focLib gespeichert.\n');
end
