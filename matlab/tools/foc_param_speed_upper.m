function foc_param_speed_upper()
%FOC_PARAM_SPEED_UPPER  Obergrenze des Drehzahlreglers (Iq_ref) als Konfigurationsparameter.
%
%   foc_param_speed_upper()
%
%   Bisher steht im PID-Block "Speed Control/PI_Controller_Speed/PID Controller" fest
%   UpperSaturationLimit = 1 (Iq_ref in PU). Mit I_base = 56 A (datasheet) waeren das 56 A,
%   ueber Motornennstrom und EVM-Grenze (reports/08_S5_Fehler_Flash_2026-11-04.md).
%   Neu: UpperSaturationLimit = ctrl_limits.Iq_max (foc_config):
%     legacy_f2838  Iq_max = 1     (bitgleich zum Altmodell)
%     datasheet     Iq_max = 0.1   (5,6 A, Erstinbetriebnahme)
%   Gegenstueck zu foc_param_brake_limit (A2). Vorher/nachher werden ausgegeben; focLib wird
%   gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');

blk = [lib '/Speed Control/PI_Controller_Speed/PID Controller'];
if getSimulinkBlockHandle(blk) == -1
    error('foc:iqmax', 'Block nicht gefunden: %s', blk);
end
newMax = 'ctrl_limits.Iq_max';
before = get_param(blk, 'UpperSaturationLimit');
if strcmp(before, newMax)
    fprintf('%s\n  bereits parametriert: UpperSaturationLimit=%s\n', blk, before);
    return;
end
set_param(blk, 'UpperSaturationLimit', newMax);
save_system(lib);
fprintf('%s\n  vorher:  UpperSaturationLimit=%s, LowerSaturationLimit=%s\n  nachher: UpperSaturationLimit=%s\n', ...
    blk, before, get_param(blk, 'LowerSaturationLimit'), get_param(blk, 'UpperSaturationLimit'));
fprintf('focLib gespeichert.\n');
end
