function T = foc_run_p1b(stopTime)
%FOC_RUN_P1B  Schritt 5: drei Simulationslaeufe A/B/C und Kennzahlen.
%
%   T = foc_run_p1b()            Simulationsdauer = StopTime des Modells
%   T = foc_run_p1b(stopTime)
%
%   Voraussetzung: focLib.slx ist im Stand P1a (noch keine Korrekturen). Das Skript
%   prueft das und bricht sonst ab. Danach ist focLib im Stand "alle Korrekturen".
%
%     Lauf A  Profil datasheet, Reglerbloecke unveraendert
%     Lauf B  + foc_apply_fixes('current_pi')
%     Lauf C  + foc_apply_fixes('speed_pi')
%
%   Ergebnisse in matlab/reports/:
%     P1b_Vergleich.mldatx   alle Laeufe fuer den Simulation Data Inspector
%     P1b_Kennzahlen.csv     Kennzahlen je Lauf
%     img/P1b_<Lauf>.png     Drehzahl, iq, Vd je Lauf
%
%   Kennzahlen (alle in PU, Zeitbereich: letzte 20 % der Simulation):
%     speed_err_mean   mittlere Abweichung N_ref - Speed_PU  (bleibende Regelabweichung)
%     iq_err_rms       RMS(Iq_ref - Iq_meas)                 (Stromregelguete)
%     id_err_rms       RMS(Id_ref - Id_meas)
%     vd_min           kleinste d-Spannung im ganzen Lauf    (< 0 erst nach current_pi moeglich)

root = fileparts(fileparts(mfilename('fullpath')));
repDir = fullfile(root, 'reports');
if ~exist(repDir, 'dir'), mkdir(repDir); end
app = 'app_speed'; lib = 'focLib';
load_system(app); load_system(lib);
if nargin < 1 || isempty(stopTime), stopTime = str2double(get_param(app, 'StopTime')); end

% ---- Ausgangszustand pruefen ---------------------------------------------------------------
spd = [lib '/Speed Control/PI_Controller_Speed/PID Controller'];
cur = [lib '/Torque Controller/FOC/Current_Controllers/PI_Controller_Iq/PID Controller'];
if ~strcmp(get_param(spd, 'UseKiTs'), 'on') || ~strcmp(get_param(cur, 'UseKiTs'), 'on')
    error('foc:p1b', ['focLib enthaelt bereits Korrekturen. Erst auf Stand P1a zuruecksetzen ' ...
        '(git checkout P1a -- matlab/models/focLib.slx) und MATLAB-Bibliothek schliessen.']);
end

foc_enable_logging(app, lib);
Simulink.sdi.clear;

runs = { ...
    'A_datasheet_unkorrigiert', {}; ...
    'B_plus_current_pi',        {'current_pi'}; ...
    'C_plus_speed_pi',          {'speed_pi'}};

name = cell(3,1); speed_err_mean = nan(3,1); iq_err_rms = nan(3,1);
id_err_rms = nan(3,1); vd_min = nan(3,1);
for i = 1:size(runs, 1)
    if ~isempty(runs{i,2}), foc_apply_fixes(runs{i,2}); end
    foc_startup('datasheet');
    fprintf('\n=== Lauf %s (%g s) ===\n', runs{i,1}, stopTime);
    out = sim(app, 'StopTime', num2str(stopTime), 'ReturnWorkspaceOutputs', 'on', ...
        'SignalLogging', 'on', 'SignalLoggingName', 'logsout');
    ids = Simulink.sdi.getAllRunIDs();
    r = Simulink.sdi.getRun(ids(end));
    r.Name = runs{i,1};

    L = out.get('logsout');
    name{i} = runs{i,1};
    t0 = 0.8 * stopTime;
    try
        [t, nref] = sig(L, 'N_ref', 1);
        [~, n]    = sigAt(L, 'Speed_PU', 1, t);
        m = t >= t0;
        speed_err_mean(i) = mean(nref(m) - n(m));
    catch err, warning('foc:p1b', 'Drehzahl-Kennzahl: %s', err.message); end
    try
        [t, idref] = sig(L, 'IdqRef_PU', 1);
        [~, iqref] = sig(L, 'IdqRef_PU', 2);
        [idm, iqm] = idqMeas(L, t);
        m = t >= t0;
        id_err_rms(i) = sqrt(mean((idref(m) - idm(m)).^2));
        iq_err_rms(i) = sqrt(mean((iqref(m) - iqm(m)).^2));
    catch err, warning('foc:p1b', 'Strom-Kennzahl: %s', err.message); end
    try
        [~, vd] = sig(L, 'Vdq_ref_1', 1);
        vd_min(i) = min(vd);
    catch err, warning('foc:p1b', 'Spannungs-Kennzahl: %s', err.message); end
    try
        savePlot(L, runs{i,1}, fullfile(repDir, 'img'));
    catch err, warning('foc:p1b', 'Plot: %s', err.message); end
end

T = table(name, speed_err_mean, iq_err_rms, id_err_rms, vd_min);
disp(T);
writetable(T, fullfile(repDir, 'P1b_Kennzahlen.csv'));
Simulink.sdi.save(fullfile(repDir, 'P1b_Vergleich.mldatx'));
fprintf('Gespeichert in %s\n', repDir);
end

% -------------------------------------------------------------------------------------------
function [t, y] = sig(L, name, col)
e = L.getElement(name);
if isa(e, 'Simulink.SimulationData.Dataset'), e = e.getElement(1); end
v = e.Values;
t = v.Time(:);
d = double(v.Data);
if size(d, 1) ~= numel(t), d = reshape(d, [], numel(t)).'; end   % [n x 1 x T] -> [T x n]
y = d(:, min(col, size(d, 2)));
end

function [t, y] = sigAt(L, name, col, tq)
[t0, y0] = sig(L, name, col);
[t0, iu] = unique(t0);
y = interp1(t0, y0(iu), tq, 'previous', 'extrap');
t = tq;
end

function [idm, iqm] = idqMeas(L, t)
% Park Transform hat je nach Version zwei Ausgaenge (Id, Iq) oder einen Vektor
names = L.getElementNames();
if any(strcmp(names, 'Idq_meas_1'))
    [~, idm] = sigAt(L, 'Idq_meas_1', 1, t);
    [~, iqm] = sigAt(L, 'Idq_meas_2', 1, t);
else
    [~, idm] = sigAt(L, 'Idq_meas', 1, t);
    [~, iqm] = sigAt(L, 'Idq_meas', 2, t);
end
end

function savePlot(L, runName, imgDir)
if ~exist(imgDir, 'dir'), mkdir(imgDir); end
f = figure('Visible', 'off', 'Position', [100 100 900 800]);
subplot(3,1,1);
[t, y] = sig(L, 'N_ref', 1); stairs(t, y); hold on;
[t, y] = sig(L, 'Speed_PU', 1); plot(t, y); grid on;
legend('N\_ref', 'Speed\_PU', 'Location', 'best'); ylabel('Drehzahl [PU]'); title(strrep(runName, '_', ' '));
subplot(3,1,2);
[t, y] = sig(L, 'IdqRef_PU', 2); stairs(t, y); hold on;
[idm, iqm] = idqMeas(L, t); plot(t, iqm); grid on;  %#ok<ASGLU>
legend('Iq\_ref', 'Iq\_meas', 'Location', 'best'); ylabel('i_q [PU]');
subplot(3,1,3);
[t, y] = sig(L, 'Vdq_ref_1', 1); plot(t, y); grid on;
ylabel('V_d [PU]'); xlabel('t [s]');
saveas(f, fullfile(imgDir, ['P1b_' runName '.png']));
close(f);
end
