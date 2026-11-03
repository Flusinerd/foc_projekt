function inv = foc_cfg_inverter_drv8300()
%FOC_CFG_INVERTER_DRV8300  Leistungsstufe DRV8300DIPW-EVM an der CarPediem-Platine.
%
%   Feldnamen wie inverter_DRV8300 im Altmodell.
%   Strommessung (SLVUBV6 Abschn. 2.4, Schaltplan SLVC817/MD029A, geprueft 2026-11-03):
%   * Low-Side-Shunt 3 mOhm, Differenzverstaerker TLV9004 mit 49,9k/(4,99k+10,2) = 9,98 V/V,
%     Offset 1,65 V (AREF-Jumper J8 auf 1:2, bidirektional) -> 0,03 V/A, Messbereich +/-55 A.
%   * Polaritaet: IN+ am Shunt-Ende auf FET-Seite. Strom in den Motor fliesst waehrend der
%     Low-Side-Leitphase von GND nach oben -> ADC-Wert sinkt. MCB-Konvention
%     invertingAmp = -1 ("currents entering motor phases are read as negative").
%   * Im Codegenerierungspfad gilt PU-Strom = (ADC - Offset)/2048 (invertingAmp dort
%     ungenutzt; die Negation steckt in "Scaling DutyCycles"). Siehe
%     matlab/reports/05_P1b_Diagnose_2026-11-03.md und foc_pu_system (Befund B4).

inv.model                = 'DRV8300DIPW-EVM';
inv.sn                   = 'INV_XXXX';
inv.V_dc                 = 24;
inv.I_trip               = 25;
inv.Rds_on               = 0.003;
inv.Rshunt               = 0.003;
inv.CtSensAOffset        = 2047;     % Startwert, wird beim Hochlauf kalibriert
inv.CtSensBOffset        = 2047;
inv.CtSensCOffset        = 2047;
inv.ADCGain              = 1;
inv.EnableLogic          = 1;
inv.invertingAmp         = -1;       % EVM liest Strom in den Motor negativ (SLVC817)
inv.ISenseVref           = 3.3;
inv.CSA_gain             = 9.98;     % 49,9k/(4,99k+10,2), SLVC817
inv.ISenseVoltPerAmp     = inv.Rshunt * 10;   % 0,03 V/A (Nennwert SLVUBV6: 10 V/V)
inv.ISenseMax            = inv.ISenseVref/(2*inv.ISenseVoltPerAmp);   % 55 A Spitze
inv.R_board              = 0;
inv.CtSensOffsetMax      = 2500;
inv.CtSensOffsetMin      = 1500;
inv.ADCOffsetCalibEnable = 1;

% Zusatz (vom Modell nicht genutzt)
inv.I_max_hw             = 30;       % Datenblatt EVM
inv.U_max                = 100;
end
