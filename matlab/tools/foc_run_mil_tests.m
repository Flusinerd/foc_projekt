function R = foc_run_mil_tests()
%FOC_RUN_MIL_TESTS  Schritt 6: focLib_MIL.mldatx ausfuehren und Ergebnis zusammenfassen.
%
%   R = foc_run_mil_tests()
%
%   Schreibt matlab/reports/06_MIL_Ergebnis.csv (je Testfall: Ergebnis, Baseline, verify).

root = fileparts(fileparts(mfilename('fullpath')));
tfFile = fullfile(root, 'tests', 'focLib_MIL.mldatx');
sltest.testmanager.clear; sltest.testmanager.clearResults;
sltest.testmanager.load(tfFile);
rs = sltest.testmanager.run;
tfr = rs.getTestFileResults;
tcr = [];
for s = tfr.getTestSuiteResults
    tcr = [tcr, s.getTestCaseResults]; %#ok<AGROW>
end
name = cell(numel(tcr), 1); outcome = name; baseline = name; verifyRes = name;
for i = 1:numel(tcr)
    name{i} = tcr(i).Name;
    outcome{i} = char(tcr(i).Outcome);
    anyFail = false;
    try
        v = tcr(i).getVerifyRuns;
        s = {};
        for j = 1:numel(v)
            for k = 1:v(j).SignalCount
                sig = v(j).getSignalByIndex(k);
                o = verifyOutcome(sig);
                anyFail = anyFail || strcmp(o, 'fail');
                s{end+1} = sprintf('%s=%s', regexprep(sig.Name, '^.*[/:]', ''), o); %#ok<AGROW>
            end
        end
        verifyRes{i} = strjoin(s, '; ');
    catch err
        verifyRes{i} = ['nicht lesbar: ' err.message];
    end
    % Baseline: abgeleitet (Testfall faellt ohne verify-Fehler -> Baseline-Abweichung)
    if strcmp(outcome{i}, 'Passed'), baseline{i} = 'ok';
    elseif ~anyFail, baseline{i} = 'Abweichung';
    else, baseline{i} = 'nicht eindeutig (verify fail)'; end
    fprintf('%-28s %-8s\n   Baseline: %s\n   verify:   %s\n', name{i}, outcome{i}, baseline{i}, verifyRes{i});
end
R = table(name, outcome, baseline, verifyRes);
writetable(R, fullfile(root, 'reports', '06_MIL_Ergebnis.csv'));
fprintf('Gesamt: %d bestanden, %d fehlgeschlagen, %d unvollstaendig\n', ...
    rs.NumPassed, rs.NumFailed, rs.NumIncomplete);
sltest.testmanager.clear;
end

function o = verifyOutcome(sig)
% Ergebnis eines verify-Signals (Enumeration slTestResult): fail, wenn irgendwann Fail;
% pass, wenn mindestens einmal Pass; sonst untested (Bedingung nie aktiv).
v = sig.Values.Data;
if any(v == slTestResult.Fail), o = 'fail';
elseif any(v == slTestResult.Pass), o = 'pass';
else, o = 'untested'; end
end
