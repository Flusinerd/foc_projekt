function foc_clock_ext25(model)
%FOC_CLOCK_EXT25  Anwendungsmodell auf den externen 25-MHz-Oszillator der CarPediem-Platine umstellen.
%
%   foc_clock_ext25()            matlab/models/app_speed.slx
%   foc_clock_ext25(model)
%
%   Bisher (Altmodell der MA): interner Oszillator INTOSC2 10 MHz, PLL x40, /2 = 200 MHz.
%   INTOSC2 ist fuer CAN mit 1 Mbit/s zu ungenau (Bit-/Formfehler); mit dem externen Oszillator
%   laeuft CAN fehlerfrei (reports/08_CAN_Test_2026-11-03.md). Neu: XTAL 25 MHz, PLL x16, /2 = 200 MHz.
%   Die CPU-Frequenz bleibt 200 MHz; Simulation und Reglerparameter sind nicht betroffen
%   (CoderTargetData wirkt nur auf die Codegenerierung).
%   Werte im Clocking-Block sind Zeichenketten. Vorher/nachher werden ausgegeben; Modell wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(model), model = 'app_speed'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
ctd = get_param(model, 'CoderTargetData');
c = ctd.Clocking;
show = @(c) sprintf('UseInternalOsc=%s, OSCCLK=%s MHz, AutoSetPll=%s, PLLCR=%s, PLLDIV=%s, DIVSEL=%s, CPU=%s MHz', ...
    num2str(c.UseInternalOsc), c.OSCCLK, num2str(c.AutoSetPllSettings), c.PLLCR, c.PLLDIV, c.DIVSEL, c.ClosestCpuClock);
fprintf('%s Clocking\n  vorher:  %s\n', model, show(c));
if isequal(c.UseInternalOsc, 0) && strcmp(c.OSCCLK, '25') && strcmp(c.PLLCR, '16')
    fprintf('  bereits auf externem 25-MHz-Oszillator, keine Aenderung.\n');
    return;
end
c.UseInternalOsc = 0;
c.OSCCLK = '25';
c.PLLCR = '16';
c.PLLDIV = '1';
c.DIVSEL = '2';
ctd.Clocking = c;
set_param(model, 'CoderTargetData', ctd);
c2 = get_param(model, 'CoderTargetData').Clocking;
fprintf('  nachher: %s\n', show(c2));
if ~strcmp(c2.ClosestCpuClock, '200')
    error('foc:clock', 'CPU-Takt nach Umstellung %s MHz statt 200 MHz - nicht gespeichert.', c2.ClosestCpuClock);
end
save_system(model);
fprintf('%s gespeichert.\n', model);
end
