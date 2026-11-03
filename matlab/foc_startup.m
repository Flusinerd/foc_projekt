function foc_startup(profile)
%FOC_STARTUP  Pfade setzen und Konfiguration laden (Startdatei des Simulink-Projekts).
%
%   foc_startup                    Profil 'legacy_f2838' (Verhalten wie MA Akguel)
%   foc_startup('datasheet')       Profil mit Datenblatt-Motor und neuer Reglerauslegung
%
%   Ersetzt das manuelle Laden von params_f2838.mat.

if nargin < 1, profile = 'legacy_f2838'; end
root = fileparts(mfilename('fullpath'));
addpath(root, ...
    fullfile(root, 'config'), fullfile(root, 'design'), fullfile(root, 'tools'), ...
    fullfile(root, 'tests'), fullfile(root, 'models'), fullfile(root, 'models', 'legacy'));
foc_apply_config(foc_config(profile));
end
