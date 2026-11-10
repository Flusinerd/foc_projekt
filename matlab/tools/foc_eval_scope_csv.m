function r = foc_eval_scope_csv(file, varargin)
%FOC_EVAL_SCOPE_CSV  FOC-Studio-Scope-CSV auswerten (Hall-Spruenge, Rauschen, FFT).
%
%   r = foc_eval_scope_csv('C:\...\focstudio_scope_*.csv')
%   r = foc_eval_scope_csv(file, 'png', 'reports/img/xyz.png')   zusaetzlich FFT-Plot speichern
%
%   Erwartet das Format des CSV-Exports (Trennzeichen ';', erste Spalte t_s, weitere Spalten
%   <signal>_<einheit>). Ausgewertet wird, was vorhanden ist:
%     speed_rpm   Mittelwert, Standardabweichung, Spruenge > 1,3 x gleitender Median
%     hall_state  Zustandswechsel, ungueltige Zustaende (0/7) und unzulaessige Uebergaenge
%     ia/ib/ic/iq/id_A  Mittelwert, Standardabweichung, staerkste FFT-Linien (Stillstand: Rauschen)
%   Hinweis: Die Zeitspalte nutzt den von FOC Studio gemeldeten Basistakt (comm.telem_f).

p = inputParser; addParameter(p, 'png', ''); parse(p, varargin{:});
T = readtable(file, 'Delimiter', ';', 'VariableNamingRule', 'preserve');
t = T{:, 1}; dt = median(diff(t)); fs = 1 / dt;
r = struct('file', file, 'samples', height(T), 'duration_s', t(end) - t(1), 'fs_Hz', fs);
fprintf('%s: %d Abtastwerte, %.1f s, %.0f Hz\n', file, height(T), r.duration_s, fs);
names = T.Properties.VariableNames;

if any(strcmp(names, 'speed_rpm'))
    n = T.speed_rpm; m = movmedian(n, 41);
    jump = abs(n) > 1.3 * abs(m) & abs(m) > 100;
    r.speed_mean = mean(n); r.speed_std = std(n); r.speed_jumps = sum(diff([0; jump]) == 1);
    fprintf('  Drehzahl: Mittel %.1f rpm, Std %.1f rpm, Spruenge >1,3x Median: %d (%.2f/min)\n', ...
        r.speed_mean, r.speed_std, r.speed_jumps, r.speed_jumps / r.duration_s * 60);
end
hcol = names(startsWith(names, 'hall_state'));
if ~isempty(hcol)
    h = round(T.(hcol{1}));
    fwd = containers.Map({1, 5, 4, 6, 2, 3}, {5, 4, 6, 2, 3, 1});
    ch = find(diff(h) ~= 0) + 1; bad = 0; inval = sum(h == 0 | h == 7);
    for k = ch'
        a = h(k - 1); b = h(k);
        if isKey(fwd, a) && isKey(fwd, b) && fwd(a) ~= b && fwd(b) ~= a, bad = bad + 1; end
    end
    r.hall_changes = numel(ch); r.hall_invalid = inval; r.hall_bad_transitions = bad;
    fprintf('  Hall: %d Wechsel, %d ungueltige Werte (0/7), %d unzulaessige Uebergaenge\n', numel(ch), inval, bad);
end
cur = names(endsWith(names, '_A'));
if ~isempty(p.Results.png), figure('Visible', 'off', 'Position', [100 100 900 300 * numel(cur)]); end
for k = 1:numel(cur)
    x = T.(cur{k}); x = x - mean(x); N = numel(x);
    X = abs(fft(x .* (0.5 - 0.5 * cos(2 * pi * (0:N - 1)' / N)))) / N * 4;
    f = (0:N - 1)' * fs / N; half = f > 5 & f < fs / 2;
    [~, idx] = sort(X(half), 'descend'); fh = f(half); xh = X(half);
    s = struct('mean', mean(T.(cur{k})), 'std', std(T.(cur{k})), 'peaks_Hz', fh(idx(1:min(3, end)))', ...
               'peaks_A', xh(idx(1:min(3, end)))');
    r.(matlab.lang.makeValidName(cur{k})) = s;
    fprintf('  %s: Mittel %.3f A, Std %.3f A, Linien %s Hz (%s A)\n', cur{k}, s.mean, s.std, ...
        mat2str(round(s.peaks_Hz)), mat2str(s.peaks_A, 2));
    if ~isempty(p.Results.png)
        subplot(numel(cur), 1, k); semilogy(fh, xh); grid on; xlabel('f [Hz]'); ylabel('A');
        title(strrep(cur{k}, '_', '\_'));
    end
end
if ~isempty(p.Results.png), exportgraphics(gcf, p.Results.png); fprintf('  Bild: %s\n', p.Results.png); end
end
