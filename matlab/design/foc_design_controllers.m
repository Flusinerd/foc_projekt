function par = foc_design_controllers(motor, pu, timing, opts)
%FOC_DESIGN_CONTROLLERS  Nachvollziehbare Auslegung der Strom- und Drehzahlregler.
%
%   pi = foc_design_controllers(motor, pu, timing, opts)
%
%   Ersetzt mcb.internal.SetControllerParameters (internes, undokumentiertes API).
%   Ausgabe: Struktur mit den Feldnamen von PI_params im Altmodell (in PU), plus
%   SI-Werte und Bandbreiten zur Dokumentation.
%
%   Stromregler (d, q) - Betragsoptimum:
%       Strecke    G(s) = 1/R / (1 + s*L/R) * 1/(1 + s*sigma)
%       Tn = L/R (Polkompensation),  Kp = L / (2*sigma)  [V/A]
%       sigma = opts.sigma_factor * Ts  (Rechentotzeit 1*Ts + PWM 0,5*Ts)
%       Durchtrittsfrequenz ~ 1/(2*sigma)
%
%   Drehzahlregler - Bandbreitenauslegung (Strom-Regelkreis als schnell angenommen):
%       Strecke    G(s) = Kt / (J_ges * s)
%       Kp = J_ges * wc / Kt   [A/(rad/s)],  Tn = opts.speed_tn_factor / wc
%       Mit Hall-Sensoren und kleiner Last-Traegheit bleibt fc klein (Messtotzeit
%       = Zeit zwischen zwei Flanken, bei 200 min^-1 und p=4 schon 12,5 ms).
%
%   opts (alle optional):
%       sigma_factor     (1.5)   Summentotzeit des Stromkreises in Vielfachen von Ts
%       speed_bw_Hz      (5)     Bandbreite Drehzahlregler
%       speed_tn_factor  (4)     Tn = factor/wc  (4 -> ca. 50 Grad Phasenreserve)
%       J_load           (0)     zusaetzliche Traegheit an der Welle [kgm^2]

if nargin < 4, opts = struct(); end
opts = setdef(opts, 'sigma_factor', 1.5);
opts = setdef(opts, 'speed_bw_Hz', 5);
opts = setdef(opts, 'speed_tn_factor', 4);
opts = setdef(opts, 'J_load', 0);

Ts  = timing.Ts;
Tsn = timing.Ts_speed;

% ---- Stromregler ------------------------------------------------------------------------
sigma = opts.sigma_factor * Ts;
cur_d = current_bo(motor.Ld, motor.Rs, sigma, pu);
cur_q = current_bo(motor.Lq, motor.Rs, sigma, pu);

% ---- Drehzahlregler ---------------------------------------------------------------------
Kt   = 1.5 * motor.p * motor.FluxPM;          % Nm/A
Jges = motor.J + opts.J_load;
wc   = 2*pi*opts.speed_bw_Hz;
Kp_n_SI = Jges * wc / Kt;                      % A / (rad/s mech)
Tn_n    = opts.speed_tn_factor / wc;
w_base  = pu.N_base * 2*pi/60;                 % rad/s mech je 1 PU Drehzahl
Kp_n    = Kp_n_SI * w_base / pu.I_base;        % PU Strom je PU Drehzahl
Ki_n    = Kp_n / Tn_n;                         % 1/s

% ---- Plausibilitaet ---------------------------------------------------------------------
fc_cur = 1/(2*pi*2*sigma);
if opts.speed_bw_Hz * 10 > fc_cur
    warning('foc:design', 'Drehzahlbandbreite %.1f Hz ist nicht << Strombandbreite %.0f Hz', ...
        opts.speed_bw_Hz, fc_cur);
end
if Tsn * wc > 0.2
    warning('foc:design', 'Drehzahlabtastung Ts_speed = %g s zu langsam fuer %.1f Hz', Tsn, opts.speed_bw_Hz);
end

% ---- Ausgabe mit den Feldnamen des Altmodells -------------------------------------------
par.T1        = Ts;
par.T2        = Ts;
par.delay_IIR = 0.02;                          % nur Kompatibilitaet, im Modell ungenutzt
par.damping   = 1/sqrt(2);
par.x         = 1.2;                           % nur Kompatibilitaet
par.sigma     = sigma;
par.Ti_i      = cur_q.Tn;
par.Ti_id     = cur_d.Tn;
par.Kp_i      = cur_q.Kp;
par.Ki_i      = cur_q.Ki;
par.Kp_id     = cur_d.Kp;
par.Ki_id     = cur_d.Ki;
par.Ki_texas  = Ts / cur_q.Tn;                 % "TI-Form": Ki*Ts/Kp  (vom Altmodell genutzt)
par.Ki_d_texas= Ts / cur_d.Tn;
par.delta     = Tn_n / opts.speed_tn_factor;   % nur Kompatibilitaet
par.Ti_speed  = Tn_n;
par.Kp_speed  = Kp_n;
par.Ki_speed  = Ki_n;
par.Ki_speed_texas = Tsn / Tn_n;
par.gamma     = 0;                             % nur Kompatibilitaet
par.Kp_fwc    = 0;                             % Feldschwaechung nicht aktiv
par.Ki_fwc    = 0;
par.Ti_fwc    = cur_d.Tn;

% Dokumentation
par.design.method      = 'Strom: Betragsoptimum, Drehzahl: Bandbreite';
par.design.opts        = opts;
par.design.Kp_i_SI     = cur_q.Kp_SI;           % V/A
par.design.Kp_speed_SI = Kp_n_SI;               % A/(rad/s)
par.design.Kt          = Kt;
par.design.fc_current_Hz = fc_cur;
par.design.fc_speed_Hz   = opts.speed_bw_Hz;
end

% -----------------------------------------------------------------------------------------
function c = current_bo(L, R, sigma, pu)
c.Tn    = L / R;
c.Kp_SI = L / (2*sigma);
c.Kp    = c.Kp_SI * pu.I_base / pu.V_base;    % PU Spannung je PU Strom
c.Ki    = c.Kp / c.Tn;                        % 1/s
end

function s = setdef(s, name, value)
if ~isfield(s, name) || isempty(s.(name)), s.(name) = value; end
end
