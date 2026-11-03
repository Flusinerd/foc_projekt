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
switch lower(profile)
    case 'legacy_f2838', v.ctrl_limits.Iq_min = -0.5;
    otherwise,           v.ctrl_limits.Iq_min = 0;
end

% Wird im Modell als Variant-Bedingung ~isequal(dataType,'single') genutzt,
% steht aber nicht in der .mat-Datei.
if exist('fixdt', 'file') || exist('fixdt', 'builtin')
    v.dataType = fixdt(1, 32, 17);
else
    v.dataType = 'fixdt(1,32,17)';   % ausserhalb von MATLAB/Simulink (z. B. Octave-Tests)
end

cfg.vars = v;
end
