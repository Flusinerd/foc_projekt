function foc_apply_config(cfg, verbose)
%FOC_APPLY_CONFIG  Legt die Konfiguration im Base Workspace an.
%
%   foc_apply_config(foc_config('legacy_f2838'))
%
%   Danach laesst sich impmodel.slx bzw. app_speed.slx simulieren und bauen,
%   ohne params_f2838.mat zu laden.

if nargin < 2, verbose = true; end
names = fieldnames(cfg.vars);
for k = 1:numel(names)
    assignin('base', names{k}, cfg.vars.(names{k}));
end
assignin('base', 'foc_active_profile', cfg.profile);
if verbose
    fprintf('FOC-Konfiguration "%s" geladen (%d Variablen).\n', cfg.profile, numel(names));
end
end
