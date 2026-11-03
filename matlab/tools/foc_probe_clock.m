function foc_probe_clock(cs, source)
%FOC_PROBE_CLOCK  Takt eines Testmodells setzen: wie app_speed (intern) oder externer 25-MHz-Oszillator.
%
%   foc_probe_clock(cs)            wie app_speed: INTOSC2 10 MHz, PLL x40, /2 = 200 MHz
%   foc_probe_clock(cs, 'ext25')   externer Oszillator der Platine 25 MHz, PLL x16, /2 = 200 MHz
%
%   Uebernimmt den Block CoderTargetData.Clocking aus app_speed (Werte sind ZEICHENKETTEN, z. B.
%   PLLCR = '40'; eine Zahl fuehrte zu IMULT 0). Hintergrund (2026-11-03):
%   - Nur 'Clocking.UseInternalOsc'/'Clocking.OSCCLK' per codertarget.data umstellen rechnet die
%     automatische PLL nicht neu -> IMULT 20 -> 100 MHz, alle Zeiten/Bitraten halbiert.
%   - Der interne Oszillator hat eine Toleranz im Prozentbereich; CAN mit 1 Mbit/s vertraegt bei
%     10 Tq und SJW 2 nur ca. 1 % -> Variante 'ext25' zum Vergleich.

if nargin < 2 || isempty(source), source = 'app'; end
root = fileparts(fileparts(mfilename('fullpath')));
src = 'app_speed';
wasLoaded = bdIsLoaded(src);
if ~wasLoaded, load_system(fullfile(root, 'models', [src '.slx'])); end
srcCtd = get_param(src, 'CoderTargetData');
if ~wasLoaded, close_system(src, 0); end

clk = srcCtd.Clocking;
switch source
    case 'app'
    case 'ext25'
        clk.UseInternalOsc = 0;
        clk.OSCCLK = '25';
        clk.PLLCR = '16';
        clk.PLLDIV = '1';
        clk.DIVSEL = '2';
    otherwise
        error('foc:clock', 'Unbekannte Taktquelle "%s" (app | ext25).', source);
end
mdl = cs.getModel;
ctd = get_param(mdl, 'CoderTargetData');
ctd.Clocking = clk;
set_param(mdl, 'CoderTargetData', ctd);
v = @(p) num2str(codertarget.data.getParameterValue(cs, p));
fprintf('Takt (%s): intern=%s, OSCCLK=%s MHz, PLLCR=%s, CPU=%s MHz\n', source, ...
    v('Clocking.UseInternalOsc'), v('Clocking.OSCCLK'), v('Clocking.PLLCR'), v('Clocking.ClosestCpuClock'));
end
