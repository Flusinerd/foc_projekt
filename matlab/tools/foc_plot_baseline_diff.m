function foc_plot_baseline_diff(caseName, profile)
%FOC_PLOT_BASELINE_DIFF  Einen MIL-Testfall ausfuehren und Ausgabe gegen Baseline plotten.
%
%   foc_plot_baseline_diff('SpeedController_SW_ANF_05')          Profil 'datasheet'
%   foc_plot_baseline_diff(caseName, profile)
%
%   Aendert weder Testdatei noch Baseline. Bild: reports/img/MIL_<caseName>_Baseline.png.

if nargin < 2 || isempty(profile), profile = 'datasheet'; end
root = fileparts(fileparts(mfilename('fullpath')));
foc_startup(profile);
sltest.testmanager.clear; sltest.testmanager.clearResults;
tf = sltest.testmanager.load(fullfile(root, 'tests', 'focLib_MIL.mldatx'));
tc = [];
for s = tf.getTestSuites
    c = s.getTestCases;
    tc = [tc, c(strcmp({c.Name}, caseName))]; %#ok<AGROW>
end
if numel(tc) ~= 1, error('foc:bl', 'Testfall %s nicht eindeutig gefunden.', caseName); end
res = tc.run;
tcr = res.getTestCaseResults;
fprintf('%s: %s\n', caseName, char(tcr.Outcome));
out = tcr.getOutputRuns;
bl = load(fullfile(root, 'tests', 'baselines', [caseName '.mat']));
fn = fieldnames(bl); ds = bl.(fn{1});

n = ds.numElements;
figure('Visible', 'off', 'Position', [100 100 900 280*n]);
for k = 1:n
    e = ds.getElement(k); b = e.Values;
    nm = e.Name; if isempty(nm), nm = b.Name; end
    sig = [];
    for j = 1:out(1).SignalCount
        s = out(1).getSignalByIndex(j);
        if n == 1 || strcmp(s.Name, nm), sig = s; break; end
    end
    subplot(n, 1, k); hold on; grid on;
    stairs(b.Time, double(b.Data(:)), 'LineWidth', 1.5, 'DisplayName', 'Baseline (alt)');
    if ~isempty(sig)
        v = sig.Values;
        stairs(v.Time, double(v.Data(:)), '--', 'LineWidth', 1.5, 'DisplayName', ['Ist (' profile ')']);
        fprintf('%-30s Baseline [%.4f, %.4f]  Ist [%.4f, %.4f]\n', [nm ' / ' sig.Name], ...
            min(double(b.Data(:))), max(double(b.Data(:))), min(double(v.Data(:))), max(double(v.Data(:))));
        tq = b.Time; vi = interp1(v.Time, double(v.Data(:)), tq, 'previous', 'extrap');
        d = abs(vi - double(b.Data(:)));
        idx = find(d > 1e-6);
        if isempty(idx), fprintf('   keine Abweichung\n');
        else, fprintf('   Abweichung in %.3f ... %.3f s, max %.4f\n', tq(idx(1)), tq(idx(end)), max(d)); end
    end
    title(strrep([caseName ': ' nm], '_', '\_')); xlabel('t [s]'); legend('Location', 'best');
end
img = fullfile(root, 'reports', 'img', ['MIL_' caseName '_Baseline.png']);
exportgraphics(gcf, img);
fprintf('Bild: %s\n', img);
sltest.testmanager.clear;
end
