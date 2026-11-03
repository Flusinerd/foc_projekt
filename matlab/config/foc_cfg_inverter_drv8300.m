function inv = foc_cfg_inverter_drv8300()
%FOC_CFG_INVERTER_DRV8300  Leistungsstufe DRV8300DIPW-EVM an der CarPediem-Platine.
%
%   Feldnamen wie inverter_DRV8300 im Altmodell. Offene Punkte:
%   * ISenseVoltPerAmp: Der Altwert 0,003 V/A entspricht nur dem Shunt (3 mOhm) ohne
%     Verstaerker. Das EVM hat aber Strommessverstaerker (CSA). Die tatsaechliche
%     Verstaerkung aus dem EVM-Handbuch (SLVUBV6) eintragen bzw. messen.
%   * Im Codegenerierungspfad des Modells gilt: PU-Strom = (ADC - Offset)/2048,
%     d.h. 1 PU = Strom, bei dem der CSA 1,65 V Hub liefert. Das muss zu
%     PU_System.I_base passen (siehe docs/P0_P1_Anleitung.md, Befund B4).

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
inv.invertingAmp         = 1;
inv.ISenseVref           = 3.3;
inv.ISenseVoltPerAmp     = 0.003;    % TODO: CSA-Verstaerkung des EVM einrechnen
inv.ISenseMax            = 25;
inv.R_board              = 0;
inv.CtSensOffsetMax      = 2500;
inv.CtSensOffsetMin      = 1500;
inv.ADCOffsetCalibEnable = 1;

% Zusatz (vom Modell nicht genutzt)
inv.I_max_hw             = 30;       % Datenblatt EVM
inv.U_max                = 100;
end
