function foc_codegen(model)
%FOC_CODEGEN  Code fuer das Anwendungsmodell erzeugen und kompilieren, OHNE Laden auf die Hardware.
%
%   foc_startup('datasheet'); foc_codegen()      app_speed
%   foc_codegen(model)
%
%   Bricht vor slbuild ab, wenn die gespeicherte Build action nicht 'Build' ist oder External
%   Mode an ist (AGENTS.md Regel 4). Aendert nichts am Modell. Flashen: foc_flash.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(model), model = 'app_speed'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
cs = getActiveConfigSet(model);
act = codertarget.data.getParameterValue(cs, 'Runtime.BuildAction');
ext = get_param(model, 'ExtMode');
prof = '?';
if evalin('base', 'exist(''foc_active_profile'', ''var'')'), prof = evalin('base', 'foc_active_profile'); end
fprintf('foc_codegen: %s, Profil %s, BuildAction=%s, ExtMode=%s\n', model, prof, act, ext);
if ~strcmp(act, 'Build') || ~strcmp(ext, 'off')
    error('foc:codegen', ['BuildAction muss ''Build'' und ExtMode ''off'' sein ' ...
          '(foc_build_action). Abbruch, es wurde nichts gebaut.']);
end
slbuild(model);
if strcmp(get_param(model, 'Dirty'), 'on')
    error('foc:codegen', 'Modell nach dem Build veraendert (Dirty) - bitte pruefen.');
end
end
