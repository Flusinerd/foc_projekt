function foc_param_brake_limit()
%FOC_PARAM_BRAKE_LIMIT  A2: Untergrenze des Drehzahlreglers (Bremsstrom) als Konfigurationsparameter.
%
%   foc_param_brake_limit()
%
%   Bisher steht im PID-Block "Speed Control/PI_Controller_Speed/PID Controller" fest
%   LowerSaturationLimit = -0.5 (Iq_ref in PU, "leichtes Bremsen"). Ein Labornetzteil nimmt
%   keinen Strom auf; Bremsen kann PVDD hochtreiben (AGENTS.md, Schritt 8).
%   Neu: LowerSaturationLimit = ctrl_limits.Iq_min (foc_config):
%     legacy_f2838  Iq_min = -0.5   (bitgleich zum Altmodell)
%     datasheet     Iq_min =  0     (Ausrollen statt Bremsen, Default fuer das Labornetzteil)
%   Die Anti-Windup-Begrenzung des PID-Blocks folgt den Saettigungsgrenzen automatisch.
%   Vorher/nachher werden ausgegeben; focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');

blk = [lib '/Speed Control/PI_Controller_Speed/PID Controller'];
if getSimulinkBlockHandle(blk) == -1
    error('foc:a2', 'Block nicht gefunden: %s', blk);
end
newMin = 'ctrl_limits.Iq_min';
before = get_param(blk, 'LowerSaturationLimit');
if strcmp(before, newMin)
    fprintf('%s\n  bereits parametriert: LowerSaturationLimit=%s\n', blk, before);
    return;
end
set_param(blk, 'LowerSaturationLimit', newMin);
save_system(lib);
fprintf('%s\n  vorher:  LowerSaturationLimit=%s, UpperSaturationLimit=%s\n  nachher: LowerSaturationLimit=%s\n', ...
    blk, before, get_param(blk, 'UpperSaturationLimit'), get_param(blk, 'LowerSaturationLimit'));
fprintf('focLib gespeichert.\n');
end
