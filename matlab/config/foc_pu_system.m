function pu = foc_pu_system(motor, inverter, board)
%FOC_PU_SYSTEM  Bezugsgroessen (Per-Unit), wie sie im Modell tatsaechlich wirken.
%
%   pu = foc_pu_system(motor, inverter, board)
%
%   Strom und Spannung folgen den Umrechnungen im Modell, nicht mehr der Konvention der MA
%   (Befund B4, matlab/reports/05_P1b_Diagnose_2026-11-03.md):
%     I_base = Strom, bei dem der ADC 2048 Counts vom Offset abweicht
%              (Input Scaling: PU = (ADC - Offset)/2048)
%            = 2048 * ADC_Vref/ADC_MaxCount / |ISenseVoltPerAmp|      (CarPediem + EVM: 56,0 A)
%     V_base = U_dc/sqrt(3): PWM Reference Generator (SVM) setzt PwmRef = 1 in diese
%              Strangspannung um (Simulation gemessen: 13,85 V/PU)
%     N_base = Nenndrehzahl (vom Modell genutzt: Speed calculator, Speed Measurement)
%     T_base = 1,5 * p * Psi * I_base,  P_base = 1,5 * V_base * I_base
%
%   PU_System.I_base und V_base werden im Modell selbst nicht verwendet, nur in
%   foc_design_controllers. Die MA-Konvention (V_base = sqrt(3)/2*U_dc, I_base = Nennstrom)
%   steht unveraendert im Profil legacy_f2838.

pu.V_base = inverter.V_dc / sqrt(3);
pu.I_base = (board.ADC_MaxCount + 1)/2 * board.ADC_Vref/board.ADC_MaxCount / abs(inverter.ISenseVoltPerAmp);
pu.N_base = motor.N_base;
pu.T_base = 1.5 * motor.p * motor.FluxPM * pu.I_base;
pu.P_base = 1.5 * pu.V_base * pu.I_base;
end
