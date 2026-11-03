function pu = foc_pu_system(motor, inverter)
%FOC_PU_SYSTEM  Bezugsgroessen (Per-Unit) nach der Konvention der Altfirmware.
%
%   Konvention der MA (beibehalten, damit Skalierungen im Modell gueltig bleiben):
%     V_base = sqrt(3)/2 * U_dc     I_base = Nennstrom     N_base = Nenndrehzahl
%     T_base = 1,5 * p * Psi * I_base   P_base = 1,5 * V_base * I_base
%
%   Achtung: Die Spannungsbasis, mit der der PWM-Modulator (SVM-Block) tatsaechlich
%   rechnet, und die Strombasis des ADC-Pfads (1,65 V CSA-Hub) sind davon unabhaengig.
%   Siehe Befund B4 in docs/P0_P1_Anleitung.md.

pu.V_base = sqrt(3)/2 * inverter.V_dc;
pu.I_base = motor.I_rated;
pu.N_base = motor.N_base;
pu.T_base = 1.5 * motor.p * motor.FluxPM * pu.I_base;
pu.P_base = 1.5 * pu.V_base * pu.I_base;
end
