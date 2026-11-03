function foc_speed_assessment_tol(tol)
%FOC_SPEED_ASSESSMENT_TOL  SW_ANF_05: Toleranz fuer "Iq_ref <= 0" bei negativer Regelabweichung.
%
%   foc_speed_assessment_tol()       Toleranz 0,03 PU (Entscheidung 2026-11-03; gemessener
%                                    Rest bis +0,024 PU, 0,01 PU reichte nicht)
%   foc_speed_assessment_tol(tol)
%
%   Mit A2 (Untergrenze ctrl_limits.Iq_min = 0) friert die Anti-Windup-Begrenzung den
%   I-Anteil an der Grenze mit einem kleinen Rest ein; laeuft die Regelabweichung gegen 0,
%   erscheint er als Iq_ref ≈ +0,001 PU. Die Pruefung DesiredTorqueNegative erlaubt deshalb
%   Iq_ref <= tol statt <= 0. Untergrenze -0,5 bleibt geprueft. Vorher/nachher werden
%   ausgegeben; focLib (Harness intern) wird gespeichert.

if nargin < 1 || isempty(tol), tol = 0.03; end
root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');
own = [lib '/Speed Control/PI_Controller_Speed'];
h = 'SpeedController_TestHarness';
sltest.harness.load(own, h);
blk = [h '/Test Assessment'];
step = 'GlobalAssessment.DesiredTorqueNegative';
before = sltest.testsequence.readStep(blk, step).Action;
if isempty(regexp(before, 'Iq_ref <= [0-9.]+\s*[,)]', 'once'))
    fprintf('%s\n  unerwartete Aktion, nichts geaendert:\n  %s\n', step, before);
    close_system(h, 0); return;
end
after = regexprep(before, 'Iq_ref <= [0-9.]+(\s*[,)])', sprintf('Iq_ref <= %g$1', tol), 'once');
after = regexprep(after, 'zwischen -0.5 und (Null|[0-9.]+)', sprintf('zwischen -0.5 und %g', tol));
sltest.testsequence.editStep(blk, step, 'Action', after);
fprintf('%s\n  vorher:  %s\n  nachher: %s\n', step, before, sltest.testsequence.readStep(blk, step).Action);
save_system(h);
close_system(h, 0);
save_system(lib);
fprintf('focLib gespeichert.\n');
end
