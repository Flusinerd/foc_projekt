function foc_recapture_baseline(caseName, profile)
%FOC_RECAPTURE_BASELINE  Baseline eines Testfalls in tests/focLib_MIL.mldatx neu aufnehmen.
%
%   foc_recapture_baseline('SpeedController_SW_ANF_05')            Profil 'datasheet'
%   foc_recapture_baseline(caseName, profile)
%
%   Entfernt die vorhandenen Baseline-Kriterien des Testfalls, nimmt sie im aktuellen
%   Modellstand neu auf (gleiche Datei tests/baselines/<caseName>.mat) und speichert die
%   Testdatei. Nur nach einer bewussten, dokumentierten Verhaltensaenderung verwenden.

if nargin < 2 || isempty(profile), profile = 'datasheet'; end
root = fileparts(fileparts(mfilename('fullpath')));
tfFile = fullfile(root, 'tests', 'focLib_MIL.mldatx');
blFile = fullfile(root, 'tests', 'baselines', [caseName '.mat']);
foc_startup(profile);
sltest.testmanager.clear;
tf = sltest.testmanager.load(tfFile);
tc = [];
for s = tf.getTestSuites
    c = s.getTestCases;
    tc = [tc, c(strcmp({c.Name}, caseName))]; %#ok<AGROW>
end
if numel(tc) ~= 1, error('foc:baseline', 'Testfall %s nicht eindeutig gefunden.', caseName); end
old = tc.getBaselineCriteria;
fprintf('%s: %d alte Baseline-Kriterien entfernt\n', caseName, numel(old));
for i = 1:numel(old), old(i).remove; end
if exist(blFile, 'file'), delete(blFile); end
tc.captureBaselineCriteria(blFile, true);
tf.saveToFile;
fprintf('Baseline neu: %s (Profil %s)\n', strrep(blFile, [root filesep], ''), profile);
sltest.testmanager.clear;
end
