function test_config()
%TEST_CONFIG  Prueft die Parameterskripte. Laeuft in MATLAB und GNU Octave.
%
%   1. Profil legacy_f2838 reproduziert params_f2838.mat exakt (jedes Feld).
%   2. Profil datasheet: abgeleitete Groessen sind physikalisch konsistent.
%   3. Beide Profile liefern alle Variablen, die impmodel.slx referenziert.

here = fileparts(mfilename('fullpath'));
root = fileparts(here);
addpath(fullfile(root, 'config'), fullfile(root, 'design'));
nfail = 0;

% ---- 1. Legacy == .mat --------------------------------------------------------------------
matFile = fullfile(root, 'models', 'legacy', 'params_f2838.mat');
ref = load(matFile);
cfg = foc_config('legacy_f2838');
names = fieldnames(ref);
for k = 1:numel(names)
    n = names{k};
    if ~isfield(cfg.vars, n)
        fprintf('FAIL  legacy: Variable %s fehlt\n', n); nfail = nfail + 1; continue;
    end
    nfail = nfail + compare(n, ref.(n), cfg.vars.(n));
end
tune = {'focp_tune_cur_Kp'; 'focp_tune_cur_Ki'; 'focp_tune_spd_Kp'; 'focp_tune_spd_Ki'; 'focp_tune_iq_max'; 'focp_tune_iq_min'};
extra = setdiff(fieldnames(cfg.vars), [names; {'dataType'; 'ctrl_limits'}; tune]);
nfail = nfail + check('legacy: ctrl_limits.Iq_min', cfg.vars.ctrl_limits.Iq_min, -0.5, 0);
nfail = nfail + check('legacy: ctrl_limits.reset_I_freemode', cfg.vars.ctrl_limits.reset_I_freemode, 0, 0);
nfail = nfail + check('legacy: ctrl_limits.reset_I_cur_freemode', cfg.vars.ctrl_limits.reset_I_cur_freemode, 0, 0);
nfail = nfail + check('legacy: ctrl_limits.N_ref_min', cfg.vars.ctrl_limits.N_ref_min, 0, 0);
nfail = nfail + check('legacy: ctrl_limits.Iq_max', cfg.vars.ctrl_limits.Iq_max, 1, 0);
if ~isempty(extra)
    fprintf('FAIL  legacy: zusaetzliche Variablen %s\n', strjoin(extra', ', ')); nfail = nfail + 1;
end
fprintf('legacy_f2838: %d Variablen mit .mat verglichen\n', numel(names));

% ---- 2. Datenblatt-Profil ------------------------------------------------------------------
c = foc_config('datasheet');
m = c.vars.pmsm_ECI6360; p = c.vars.PI_params; b = c.vars.target_f2838; pu = c.vars.PU_System;
nfail = nfail + check('Psi aus Ke', m.FluxPM, 0.00587, 0.0001);
% Leerlaufdrehzahl bei 24 V, lineare SVM-Grenze Udc/sqrt(3): muss nahe 6000 min^-1 liegen
n0 = (24/sqrt(3)) / (m.FluxPM * m.p) * 60/(2*pi);
nfail = nfail + check('Leerlaufdrehzahl aus Psi [rpm]', n0, 5600, 400);
nfail = nfail + check('Rs Strang', m.Rs, 0.0225, 1e-9);
nfail = nfail + check('PWM-Periode [Ticks]', b.PWM_Counter_Period, 5000, 0);
% PU-Basis wie im Modell (B4): 2048 Counts * 3,36 V/4095 / 0,03 V/A, SVM-Grenze 24 V/sqrt(3)
nfail = nfail + check('I_base aus Strommesskette [A]', pu.I_base, 56.01, 0.01);
nfail = nfail + check('V_base SVM [V]', pu.V_base, 13.856, 0.001);
nfail = nfail + check('datasheet: ctrl_limits.Iq_min (Labornetzteil)', c.vars.ctrl_limits.Iq_min, 0, 0);
nfail = nfail + check('datasheet: ctrl_limits.reset_I_freemode', c.vars.ctrl_limits.reset_I_freemode, 1, 0);
nfail = nfail + check('datasheet: ctrl_limits.reset_I_cur_freemode', c.vars.ctrl_limits.reset_I_cur_freemode, 1, 0);
nfail = nfail + check('datasheet: ctrl_limits.N_ref_min [rpm]', c.vars.ctrl_limits.N_ref_min * pu.N_base, 20, 1e-9);
% Laufzeit-Kopien fuer FOCP stimmen mit den Auslegungswerten ueberein
nfail = nfail + check('datasheet: focp_tune_cur_Kp', c.vars.focp_tune_cur_Kp, p.Kp_i, 0);
nfail = nfail + check('datasheet: focp_tune_spd_Ki', c.vars.focp_tune_spd_Ki, p.Ki_speed, 0);
nfail = nfail + check('datasheet: focp_tune_iq_max', c.vars.focp_tune_iq_max, c.vars.ctrl_limits.Iq_max, 0);
% FOCP-Tabellenwerte (SI) = Auslegung (PU): cur.Kp in V/A, spd.Kp in A/rpm
nfail = nfail + check('cur.Kp SI [V/A]', p.Kp_i * pu.V_base / pu.I_base, 0.255, 0.001);
nfail = nfail + check('spd.Kp SI [A/rpm]', p.Kp_speed * pu.I_base / pu.N_base, 0.0053, 0.0001);
nfail = nfail + check('datasheet: ctrl_limits.Iq_max (Erstinbetriebnahme)', c.vars.ctrl_limits.Iq_max, 0.1, 0);
nfail = nfail + check('datasheet: Iq_max * I_base <= I_max_hw [A]', min(c.vars.ctrl_limits.Iq_max * pu.I_base, c.vars.inverter_DRV8300.I_max_hw), c.vars.ctrl_limits.Iq_max * pu.I_base, 0);
nfail = nfail + check('Strom-Tn = L/R', p.Ti_i, m.Lq/m.Rs, 1e-12);
nfail = nfail + check('Strom-Ki = Kp/Tn', p.Ki_i, p.Kp_i/p.Ti_i, 1e-9);
% Betragsoptimum in SI zurueckrechnen
Kp_SI = p.Kp_i * pu.V_base / pu.I_base;
nfail = nfail + check('Strom-Kp SI = L/(2 sigma)', Kp_SI, m.Lq/(2*1.5*c.vars.Ts), 1e-9);
% Drehzahlregler: offener Kreis bei fc muss Betrag ~1 haben (P-Anteil dominiert ab fc)
wc = 2*pi*5; w_base = pu.N_base*2*pi/60; Kt = 1.5*m.p*m.FluxPM;
L = abs((p.Kp_speed + p.Ki_speed/(1i*wc)) * pu.I_base/w_base * Kt/(m.J*1i*wc));
nfail = nfail + check('|L(j wc)| Drehzahlkreis', L, sqrt(1+(1/4)^2), 1e-6);
fprintf('datasheet: Kp_i = %.4f PU (%.3f V/A), Ki_i = %.1f 1/s, Kp_speed = %.3f PU, Ki_speed = %.2f 1/s\n', ...
    p.Kp_i, Kp_SI, p.Ki_i, p.Kp_speed, p.Ki_speed);

% ---- 3. Vom Modell referenzierte Groessen --------------------------------------------------
need = {'pmsm_ECI6360.p','pmsm_ECI6360.Rs','pmsm_ECI6360.Ld','pmsm_ECI6360.Lq','pmsm_ECI6360.J', ...
    'pmsm_ECI6360.B','pmsm_ECI6360.Ke','pmsm_ECI6360.FluxPM','pmsm_ECI6360.N_base','pmsm_ECI6360.T_rated', ...
    'inverter_DRV8300.ADCOffsetCalibEnable','inverter_DRV8300.CtSensAOffset','inverter_DRV8300.CtSensBOffset', ...
    'inverter_DRV8300.CtSensOffsetMax','inverter_DRV8300.CtSensOffsetMin','inverter_DRV8300.ISenseVoltPerAmp', ...
    'inverter_DRV8300.ISenseVref','inverter_DRV8300.V_dc','target_f2838.ADC_MaxCount','target_f2838.ADC_Vref', ...
    'target_f2838.CPU_frequency','target_f2838.PWM_Counter_Period','PU_System.I_base','PU_System.N_base', ...
    'PI_params.Kp_i','PI_params.Ki_i','PI_params.Kp_id','PI_params.Ki_id','PI_params.Ki_texas', ...
    'PI_params.Ki_d_texas','PI_params.Kp_speed','PI_params.Ki_speed_texas','PI_params.Ki_speed', ...
    'T_pwm','Ts','Ts_inverter','Ts_motor','Ts_simulink','Ts_speed','availableSpeedModes','dataType', ...
    'ctrl_limits.Iq_min', 'ctrl_limits.Iq_max', 'ctrl_limits.reset_I_freemode', 'ctrl_limits.reset_I_cur_freemode', 'ctrl_limits.N_ref_min', ...
    'focp_tune_cur_Kp', 'focp_tune_cur_Ki', 'focp_tune_spd_Kp', 'focp_tune_spd_Ki', 'focp_tune_iq_max', 'focp_tune_iq_min'};
for prof = {'legacy_f2838', 'datasheet'}
    v = foc_config(prof{1}).vars;
    for k = 1:numel(need)
        parts = strsplit(need{k}, '.');
        ok = isfield(v, parts{1});
        if ok && numel(parts) == 2, ok = isfield(v.(parts{1}), parts{2}); end
        if ~ok
            fprintf('FAIL  %s: %s fehlt\n', prof{1}, need{k}); nfail = nfail + 1;
        end
    end
end
fprintf('%d vom Modell referenzierte Groessen je Profil geprueft\n', numel(need));

if nfail == 0
    fprintf('ALL PASSED\n');
else
    error('test_config: %d Fehler', nfail);
end
end

% -------------------------------------------------------------------------------------------
function n = compare(name, a, b)
n = 0;
if isstruct(a)
    fa = fieldnames(a); fb = fieldnames(b);
    if ~isequal(sort(fa), sort(fb))
        fprintf('FAIL  %s: Felder unterschiedlich\n', name); n = 1; return;
    end
    for k = 1:numel(fa)
        n = n + compare([name '.' fa{k}], a.(fa{k}), b.(fa{k}));
    end
elseif ischar(a)
    if ~strcmp(a, b), fprintf('FAIL  %s: "%s" ~= "%s"\n', name, a, b); n = 1; end
else
    if ~isequal(double(a), double(b))
        fprintf('FAIL  %s: %.17g ~= %.17g\n', name, a, b); n = 1;
    end
end
end

function n = check(what, val, expect, tol)
n = abs(val - expect) > tol;
if n, fprintf('FAIL  %s = %.6g, erwartet %.6g +/- %.3g\n', what, val, expect, tol); end
end
