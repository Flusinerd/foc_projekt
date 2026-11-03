function b = foc_cfg_board_carpediem(adcVref)
%FOC_CFG_BOARD_CARPEDIEM  Steuerplatine (MA Darici): TMS320F28386D, 176-QFP.
%
%   b = foc_cfg_board_carpediem()        ADC-Referenz = gemessene 3,36 V
%   b = foc_cfg_board_carpediem(3.3)     ADC-Referenz explizit
%
%   Die Struktur heisst im Modell weiterhin target_f2838 (Name ist im Modell fest
%   verdrahtet). VREFHI haengt auf dieser Platine direkt an der 3,3-V-Schaltregler-
%   schiene; die MA Darici hat dort 3,36 V gemessen (Tab. 6.1). Auf jeder Platine
%   nachmessen.

if nargin < 1, adcVref = 3.36; end

f_cpu = 200e6;
f_pwm = 20e3;

b.model              = 'CarPediem F28386D (MA Darici 2023)';
b.sn                 = '001';
b.CPU_frequency      = f_cpu;
b.PWM_frequency      = f_pwm;
b.PWM_Counter_Period = f_cpu / f_pwm / 2;   % Up-Down-Zaehler, EPWMCLKDIV = /1
b.ADC_Vref           = adcVref;
b.ADC_MaxCount       = 4095;
b.SCI_baud_rate      = 12000000;
b.comport            = '<Select a port...>';

% Zusatz (vom Modell nicht genutzt)
b.oscillator_Hz      = 25e6;                % externer Oszillator, in den Hardware Settings pruefen
b.mcu                = 'TMS320F28386D (PTP, 176-QFP)';
end
