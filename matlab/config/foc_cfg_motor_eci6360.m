function m = foc_cfg_motor_eci6360()
%FOC_CFG_MOTOR_ECI6360  Motordaten ebm-papst ECI 63.60 (24-V-Variante) aus dem Datenblatt.
%
%   Alle abgeleiteten Groessen werden hier berechnet, nicht eingetragen.
%   Ersetzt die Werte erst nach der Motoridentifikation (Phase P4) durch Messwerte.
%
%   Annahmen (pruefen!):
%   * "Anschlusswiderstand" und "Anschlussinduktivitaet" sind Klemmenwerte
%     (zwischen zwei Anschluessen). Bei Sternschaltung ist der Strangwert die Haelfte.
%   * "Induzierte Spannung Uimax = 4,26 V/1000 min^-1" ist der Scheitelwert der
%     verketteten Spannung. Plausibilitaet: Mit Psi = 5,9 mVs liegt die Leerlauf-
%     drehzahl bei 24 V ~ 6000 min^-1 wie im Datenblatt; mit groesserem Psi waere
%     sie nicht erreichbar.

% --- Datenblatt (Rohwerte) ------------------------------------------------------------
ds.U_nom_V          = 24;
ds.n_nom_rpm        = 4000;
ds.T_nom_Nm         = 0.88;
ds.I_nom_A          = 17.6;      % Nennstrom (Zuleitung)
ds.n_noload_rpm     = 6000;
ds.I_noload_A       = 0.9;
ds.T_peak_Nm        = 4.4;
ds.I_peak_A         = 115;
ds.Ke_LLpk_per_krpm = 4.26;      % V / 1000 min^-1
ds.R_terminal_Ohm   = 0.045;
ds.L_terminal_H     = 0.0765e-3;
ds.J_kgm2           = 57e-6;
ds.Rth_KperW        = 2.5;
ds.pole_pairs       = 4;         % 8 Pole (Hall-Sequenz, MA Akguel)

% --- Ableitungen ------------------------------------------------------------------------
terminal_to_phase = 0.5;         % Sternschaltung; bei Dreieck 1.5 (siehe Kommentar oben)
p    = ds.pole_pairs;
Rs   = ds.R_terminal_Ohm * terminal_to_phase;
Ls   = ds.L_terminal_H   * terminal_to_phase;
w_mech_per_krpm = 1000 * 2*pi/60;                       % rad/s bei 1000 min^-1
psi  = ds.Ke_LLpk_per_krpm / sqrt(3) / (w_mech_per_krpm * p);   % Vs (Strang, Scheitel)
Kt   = 1.5 * p * psi;                                  % Nm/A (Scheitelwert Strangstrom)
% Leerlaufreibmoment aus Leerlaufstrom (grobe Naeherung, fuer Simulation)
T_fric = Kt * ds.I_noload_A;
w_noload = ds.n_noload_rpm * 2*pi/60;
B = T_fric / w_noload;                                 % als viskose Reibung bei n0

% --- Struktur mit den Feldnamen, die impmodel.slx erwartet ------------------------------
m.model          = 'ECI.63-60';
m.sn             = '003';
m.p              = p;
m.Rs             = Rs;
m.Ld             = Ls;
m.Lq             = Ls;                 % Oberflaechenmagnete: Ld = Lq
m.J              = ds.J_kgm2;
m.B              = B;
m.Ke             = ds.Ke_LLpk_per_krpm;% Vpk_LL / krpm (Einheit des Simscape-Parameters)
m.Kt             = Kt;
m.I_rated        = ds.I_nom_A;
m.N_max          = ds.n_noload_rpm;
m.PositionOffset = 0.411;              % Hall-Offset [PU el.] - wird in P4 kalibriert
m.QEPSlits       = 1000;
m.FluxPM         = psi;
m.T_rated        = ds.T_nom_Nm;
m.N_base         = ds.n_nom_rpm;

% Zusatzinformationen (vom Modell nicht genutzt)
m.datasheet         = ds;
m.terminal_to_phase = terminal_to_phase;
m.T_fric            = T_fric;
m.source            = 'Datenblatt ECI 63.60 (24 V), abgeleitet in foc_cfg_motor_eci6360.m';
end
