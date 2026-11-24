function foc_prep_trip_test(model, dac_hi, dac_lo, prescale)
%FOC_PREP_TRIP_TEST  Testvariante fuer den Hardware-Ueberstrompfad, nur im Speicher (fuer foc_flash).
%
%   foc_flash('app_speed', 'FREIGABE', @(m) foc_prep_trip_test(m, 2158, 1499, 19))
%
%   dac_hi/dac_lo: CMPSS3-DAC-Schwellen (ctrl_limits.cmpss_dac_hi/lo im Base Workspace), [] = unveraendert.
%   prescale:      CLKPRESCALE der Digitalfilter H und L (Filtertakt SYSCLK/(prescale+1)), [] = unveraendert.
%                  Mit SAMPWIN 20 / THRESH 15 muss ein Ausschlag 16 Filtertakte anliegen:
%                  9 -> 0,8 us, 19 -> 1,6 us, 39 -> 3,2 us.
%   Aendert nichts auf der Platte; foc_flash schliesst das Modell danach ohne Speichern.

cs = getActiveConfigSet(model);
if ~isempty(dac_hi), evalin('base', sprintf('ctrl_limits.cmpss_dac_hi = %d;', dac_hi)); end
if ~isempty(dac_lo), evalin('base', sprintf('ctrl_limits.cmpss_dac_lo = %d;', dac_lo)); end
if ~isempty(prescale)
    codertarget.data.setParameterValue(cs, 'CMPSS.CMPSS3_COMPH_CLKPRESCALE', num2str(prescale));
    codertarget.data.setParameterValue(cs, 'CMPSS.CMPSS3_COMPL_CLKPRESCALE', num2str(prescale));
end
cl = evalin('base', 'ctrl_limits');
p = codertarget.data.getParameterValue(cs, 'CMPSS.CMPSS3_COMPH_CLKPRESCALE');
w = codertarget.data.getParameterValue(cs, 'CMPSS.CMPSS3_COMPH_SAMPWIN');
t = codertarget.data.getParameterValue(cs, 'CMPSS.CMPSS3_COMPH_THRESH');
fprintf('Testvariante: DAC hi %d, lo %d; Filter CLKPRESCALE %s, SAMPWIN %s, THRESH %s -> %.2f us\n', ...
    cl.cmpss_dac_hi, cl.cmpss_dac_lo, p, w, t, (str2double(t) + 1) * (str2double(p) + 1) / 200);
end
