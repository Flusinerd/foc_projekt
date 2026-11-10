function foc_hall_qualification(model)
%FOC_HALL_QUALIFICATION  Eingangsqualifikation fuer die Hall-Eingaenge GPIO24..26 setzen.
%
%   foc_hall_qualification()          app_speed
%
%   Hardware-Review HW10: Auf der Steuerplatine gibt es weder Filter noch Schmitt-Trigger an den
%   Hall-Leitungen; Stoerimpulse erreichen ungefiltert das eCAP (gemessen: Drehzahlsprung auf den
%   doppelten Wert etwa einmal pro Minute). Neu in den Hardware-Einstellungen:
%     GPIO24..26  "Qualification using <n> samples"   (n = target_f2838.hall_qual_samples)
%     GPIO24_31.QualPRD = target_f2838.hall_qual_prd  (gilt fuer GPIO24..31, auch SafeState GPIO30)
%   Werte aus config/foc_cfg_board_carpediem.m. Nur Codegenerierung; Vorher/nachher werden
%   ausgegeben; das Modell wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(model), model = 'app_speed'; end
if ~bdIsLoaded(model), load_system(fullfile(root, 'models', [model '.slx'])); end
b = foc_cfg_board_carpediem();
cs = getActiveConfigSet(model);
keys = {'GPIO24_31.GPIOQualSel24', 'GPIO24_31.GPIOQualSel25', 'GPIO24_31.GPIOQualSel26', 'GPIO24_31.QualPRD'};
vals = [repmat({sprintf('Qualification using %d samples', b.hall_qual_samples)}, 1, 3), {num2str(b.hall_qual_prd)}];
fprintf('%s\n  vorher:  %s\n', model, show());
for k = 1:numel(keys), codertarget.data.setParameterValue(cs, keys{k}, vals{k}); end
fprintf('  nachher: %s\n', show());
fprintf('  Fensterbreite: %d x 2 x %d / %g MHz = %g ns\n', b.hall_qual_samples - 1, b.hall_qual_prd, ...
    b.CPU_frequency / 1e6, (b.hall_qual_samples - 1) * 2 * b.hall_qual_prd / b.CPU_frequency * 1e9);
save_system(model);
fprintf('%s gespeichert.\n', model);

    function s = show()
        v = cellfun(@(k) codertarget.data.getParameterValue(cs, k), keys, 'UniformOutput', false);
        v = cellfun(@(x) num2str(x), v, 'UniformOutput', false);
        s = strjoin(strcat(strrep(keys, 'GPIO24_31.', ''), '=', v), ', ');
    end
end
