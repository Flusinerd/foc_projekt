function foc_adjust_speed_assessment(tSettle)
%FOC_ADJUST_SPEED_ASSESSMENT  SW_ANF_05: Vorzeichenpruefung des Drehzahlreglers erst nach Einschwingzeit.
%
%   foc_adjust_speed_assessment()        Einschwingzeit 0,5 s
%   foc_adjust_speed_assessment(tSettle)
%
%   Die MA-Pruefung "Regelabweichung <= 0 => Iq_ref <= 0" (und umgekehrt) gilt nur fuer einen
%   reinen P-Regler. Mit wirksamem I-Anteil (P1b, speed_pi) bleibt Iq_ref nach einem
%   Sollwertsprung kurz im alten Vorzeichen, bis der Integrator abgebaut ist (≈ 0,3 s).
%   Die When-Bedingungen DesiredTorquePositive/Negative (ohne Freemode) gelten deshalb erst,
%   wenn die Bedingung laenger als tSettle ansteht. 0,5 s ≈ 4·Ti_speed (datasheet: 0,127 s).
%   Begrenzungen und Freemode-Pruefungen bleiben unveraendert.
%   Siehe matlab/reports/06_Harnesses_2026-11-03.md.

if nargin < 1 || isempty(tSettle), tSettle = 0.5; end
root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');
own = [lib '/Speed Control/PI_Controller_Speed'];
h = 'SpeedController_TestHarness';
sltest.harness.load(own, h);
blk = [h '/Test Assessment'];

steps = {'GlobalAssessment.DesiredTorquePositive', 'GlobalAssessment.DesiredTorqueNegative'};
for i = 1:numel(steps)
    s = sltest.testsequence.readStep(blk, steps{i});
    before = s.WhenCondition;
    if ~isempty(strfind(before, 'duration('))
        fprintf('%s\n  bereits angepasst: %s\n', steps{i}, before);
        continue;
    end
    after = sprintf('%s && duration(%s, sec) > %g', before, before, tSettle);
    sltest.testsequence.editStep(blk, steps{i}, 'WhenCondition', after);
    fprintf('%s\n  vorher:  %s\n  nachher: %s\n', steps{i}, before, ...
        sltest.testsequence.readStep(blk, steps{i}).WhenCondition);
end
save_system(h);
close_system(h, 0);
save_system(lib);
fprintf('focLib gespeichert.\n');
end
