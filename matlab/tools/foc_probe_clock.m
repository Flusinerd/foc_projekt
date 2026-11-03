function foc_probe_clock(cs)
%FOC_PROBE_CLOCK  Takt eines Testmodells exakt wie app_speed setzen (INTOSC2 10 MHz, PLL x40, /2 = 200 MHz).
%
%   foc_probe_clock(cs)    cs = Konfigurationssatz des Testmodells (Board TI F2838x)
%
%   Uebernimmt den kompletten Block CoderTargetData.Clocking aus app_speed. Hintergrund (2026-11-03):
%   - Nur 'Clocking.UseInternalOsc'/'Clocking.OSCCLK' per codertarget.data umstellen rechnet die
%     automatische PLL nicht neu -> IMULT 20 (Default fuer externen 20-MHz-Oszillator) -> 100 MHz,
%     alle Zeiten/Baud-/Bitraten halbiert (CAN 500 kbit/s statt 1 Mbit/s).
%   - 'Clocking.PLLCR' bei AutoSetPllSettings = 0 speist den Multiplikator nicht -> IMULT 0.
%   app_speed erzeugt nachweislich MW_InitSysPll(INT_OSC2,40,0,0,1,...).

root = fileparts(fileparts(mfilename('fullpath')));
src = 'app_speed';
wasLoaded = bdIsLoaded(src);
if ~wasLoaded, load_system(fullfile(root, 'models', [src '.slx'])); end
srcCtd = get_param(src, 'CoderTargetData');
if ~wasLoaded, close_system(src, 0); end

mdl = cs.getModel;
ctd = get_param(mdl, 'CoderTargetData');
ctd.Clocking = srcCtd.Clocking;
set_param(mdl, 'CoderTargetData', ctd);
v = @(p) num2str(codertarget.data.getParameterValue(cs, p));
fprintf('Takt aus %s uebernommen: intern=%s, OSCCLK=%s MHz, CPU=%s MHz\n', src, ...
    v('Clocking.UseInternalOsc'), v('Clocking.OSCCLK'), v('Clocking.ClosestCpuClock'));
end
