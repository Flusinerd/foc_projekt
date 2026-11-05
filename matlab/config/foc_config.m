function cfg = foc_config(profile, opts)
%FOC_CONFIG  Gesamte Konfiguration des FOC-Frameworks (ersetzt params_f2838.mat).
%
%   cfg = foc_config('legacy_f2838')  exakt die Werte der MA Akguel (Regression, P0/P1a)
%   cfg = foc_config('datasheet')     Motor aus Datenblatt, Platine CarPediem,
%                                     Regler nachvollziehbar ausgelegt (ab P1b)
%   cfg = foc_config('datasheet', opts)   opts wird an foc_design_controllers
%                                     weitergereicht (z. B. opts.speed_bw_Hz = 3)
%
%   cfg.vars enthaelt genau die Workspace-Variablen, die das Modell erwartet.
%   Mit foc_apply_config(cfg) werden sie im Base Workspace angelegt.

if nargin < 1 || isempty(profile), profile = 'legacy_f2838'; end
if nargin < 2, opts = struct(); end

cfg.profile = profile;
switch lower(profile)
    case 'legacy_f2838'
        v = foc_legacy_f2838();

    case 'datasheet'
        board = foc_cfg_board_carpediem();
        inv   = foc_cfg_inverter_drv8300();
        motor = foc_cfg_motor_eci6360();

        f_pwm = board.PWM_frequency;
        timing.Ts          = 1/f_pwm;            % Strom-ISR = PWM-Periode
        timing.T_pwm       = 1/f_pwm;
        timing.Ts_speed    = 10/f_pwm;           % Drehzahlregler 2 kHz
        timing.Ts_simulink = 0.5/f_pwm;          % Streckenmodell
        timing.Ts_motor    = 0.5/f_pwm;
        timing.Ts_inverter = 0.5/f_pwm;

        pu = foc_pu_system(motor, inv, board);
        v.pmsm_ECI6360     = motor;
        v.inverter_DRV8300 = inv;
        v.target_f2838     = board;
        v.PU_System        = pu;
        v.PI_params        = foc_design_controllers(motor, pu, timing, opts);
        v.T_pwm            = timing.T_pwm;
        v.Ts               = timing.Ts;
        v.Ts_inverter      = timing.Ts_inverter;
        v.Ts_motor         = timing.Ts_motor;
        v.Ts_simulink      = timing.Ts_simulink;
        v.Ts_speed         = timing.Ts_speed;
        v.availableAccModes   = 3;
        v.availableSpeedModes = 3;

    otherwise
        error('foc:config', 'Unbekanntes Profil "%s" (legacy_f2838 | datasheet)', profile);
end

% Untergrenze des Drehzahlreglers (Iq_ref in PU, A2): legacy bitgleich zum Altmodell (-0,5,
% "leichtes Bremsen"); datasheet 0 = Ausrollen, weil ein Labornetzteil keine Rueckspeisung aufnimmt.
% Iq_max: Obergrenze des Drehzahlreglers (Iq_ref in PU). legacy 1 (bitgleich); datasheet 0,1 PU
% = 5,6 A bei I_base = 56 A, fuer die Erstinbetriebnahme (Motornennstrom 17,6 A, EVM I_max_hw 30 A).
% reset_I_freemode: I-Anteil des Drehzahlreglers im Freemode auf 0 halten (sonst bleibt mit
% Iq_min = 0 ein kleiner positiver Rest stehen, SW_ANF_05). legacy 0 = Verhalten wie Altmodell.
% reset_I_cur_freemode: I-Anteile der Stromregler (d/q) im Freemode auf 0 halten. Sonst integrieren
% sie bei abgeschalteter PWM den Messoffset bis an die Spannungsgrenze; beim Freigeben folgt ein
% Stromstoss gegen die Sollrichtung (gemessen -31 A, reports/08_FOCP_Einbau_Plan_2026-11-04.md).
% N_ref_min: unterhalb dieses Drehzahlsollwerts (PU) wird der I-Anteil des Drehzahlreglers auf 0
% gehalten. Sonst integriert er bei Poti 0 Rauschen/Offset, bis der Motor alle ~20 s ruckt
% (S5-Befund). datasheet 0,005 PU = 20 rpm bei N_base 4000; legacy 0 = aus (bitgleich).
switch lower(profile)
    case 'legacy_f2838'
        v.ctrl_limits.Iq_min = -0.5;
        v.ctrl_limits.reset_I_freemode = 0;
        v.ctrl_limits.reset_I_cur_freemode = 0;
        v.ctrl_limits.N_ref_min = 0;
        v.ctrl_limits.Iq_max = 1;
    otherwise
        v.ctrl_limits.Iq_min = 0;
        v.ctrl_limits.reset_I_freemode = 1;
        v.ctrl_limits.reset_I_cur_freemode = 1;
        v.ctrl_limits.N_ref_min = 0.005;
        v.ctrl_limits.Iq_max = 0.1;
end

% Zur Laufzeit aenderbare Reglerwerte (FOC Studio, FOCP): skalare Kopien, die die PID-Bloecke nutzen.
% Bei der Codegenerierung werden sie zu ExportedGlobal-Parametern (tools/foc_tunable_params.m);
% firmware/focp_c28x.c schreibt sie, wenn FOC Studio cur.Kp/Ki, spd.Kp/Ki, spd.Iq_max/Iq_min aendert.
v.focp_tune_cur_Kp = v.PI_params.Kp_i;
v.focp_tune_cur_Ki = v.PI_params.Ki_i;
v.focp_tune_spd_Kp = v.PI_params.Kp_speed;
v.focp_tune_spd_Ki = v.PI_params.Ki_speed;
v.focp_tune_iq_max = v.ctrl_limits.Iq_max;
v.focp_tune_iq_min = v.ctrl_limits.Iq_min;

% Wird im Modell als Variant-Bedingung ~isequal(dataType,'single') genutzt,
% steht aber nicht in der .mat-Datei.
if exist('fixdt', 'file') || exist('fixdt', 'builtin')
    v.dataType = fixdt(1, 32, 17);
else
    v.dataType = 'fixdt(1,32,17)';   % ausserhalb von MATLAB/Simulink (z. B. Octave-Tests)
end

cfg.vars = v;
end
