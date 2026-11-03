function foc_build_mil_tests(profile)
%FOC_BUILD_MIL_TESTS  Schritt 6: Testdatei focLib_MIL.mldatx mit neuen Baselines anlegen.
%
%   foc_build_mil_tests()              Profil 'datasheet' (Stand P1b.1)
%   foc_build_mil_tests(profile)
%
%   Vier Testfaelle vom Typ baseline auf den Harnesses aus foc_build_harnesses.
%   Bewertung: verify() im Test Assessment + Vergleich mit Baseline (geloggte Ausgaenge des
%   Prueflings). Die Baselines werden hier NEU aufgenommen (Entscheidung 2026-11-03: keine
%   Baselines der MA uebernehmen) und unter matlab/tests/baselines/ abgelegt.
%   Bricht ab, wenn die Testdatei schon existiert.

if nargin < 1 || isempty(profile), profile = 'datasheet'; end
root = fileparts(fileparts(mfilename('fullpath')));
tfFile = fullfile(root, 'tests', 'focLib_MIL.mldatx');
blDir  = fullfile(root, 'tests', 'baselines');
if exist(tfFile, 'file')
    error('foc:mil', '%s existiert bereits. Nicht ueberschrieben.', tfFile);
end
if ~exist(blDir, 'dir'), mkdir(blDir); end

cases = { ...
    'Freemode_SW_ANF_01_07_08',  'Freemode_Check_TestHarness',  'focLib/Speed Control/Digital_Freemode_Check'; ...
    'MaxVelocity_SW_ANF_04',     'MaxVel_TestHarness',          'focLib/Speed Control/Speed calculator'; ...
    'SpeedController_SW_ANF_05', 'SpeedController_TestHarness', 'focLib/Speed Control/PI_Controller_Speed'; ...
    'SafeState_SW_ANF_03',       'SafeState_TestHarness',       'focLib/Speed Control/Mechanical_Freemode_Check'};

foc_startup(profile);
load_system(fullfile(root, 'models', 'focLib.slx'));
sltest.testmanager.clear;
tf = sltest.testmanager.TestFile(tfFile);
ts = tf.getTestSuites;
ts = ts(1);
ts.Name = 'focLib_MIL';
old = ts.getTestCases;
for i = 1:numel(old), old(i).remove; end

pre = sprintf('foc_startup(''%s'');', profile);
for i = 1:size(cases, 1)
    tc = ts.createTestCase('baseline', cases{i,1});
    tc.setProperty('Model', 'focLib', 'HarnessName', cases{i,2}, 'HarnessOwner', cases{i,3});
    % StopTime kommt aus dem Harness (wie in der MA: OverrideStopTime = 0)
    tc.setProperty('PreloadCallback', pre);
    tc.Description = sprintf(['MIL-Test aus der MA (impmodel/%s), uebertragen auf focLib.\n' ...
        'Baseline neu aufgenommen am %s mit Profil %s.'], ...
        strrep(cases{i,3}, 'focLib/', ''), datestr(now, 'yyyy-mm-dd'), profile);
    blFile = fullfile(blDir, [cases{i,1} '.mat']);
    bl = tc.captureBaselineCriteria(blFile, true);
    fprintf('%-28s Harness %-28s Baseline %s (%d Signale)\n', cases{i,1}, cases{i,2}, ...
        strrep(blFile, [root filesep], ''), numel(bl));
end
tf.saveToFile;
fprintf('Testdatei gespeichert: %s\n', tfFile);
sltest.testmanager.clear;
end
