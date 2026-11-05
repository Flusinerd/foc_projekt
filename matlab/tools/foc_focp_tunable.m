function foc_focp_tunable()
%FOC_FOCP_TUNABLE  Regler-Gains und i_q-Grenzen aus FOC Studio wirksam machen (Punkt 8).
%
%   foc_focp_tunable()
%
%   Die PID-Bloecke in focLib nutzen statt PI_params.*/ctrl_limits.* die skalaren Kopien
%   focp_tune_* (foc_config). Bei der Codegenerierung werden diese zu ExportedGlobal-Parametern
%   (foc_tunable_params); firmware/focp_c28x.c schreibt sie, wenn FOC Studio die Werte aendert.
%     Stromregler d/q:   P = focp_tune_cur_Kp, I = focp_tune_cur_Ki
%     Drehzahlregler:    P = focp_tune_spd_Kp, I = focp_tune_spd_Ki,
%                        Grenzen = focp_tune_iq_max / focp_tune_iq_min
%   Gain-Datentypen fest mit Reserve (sonst waehlt "best precision" nur den Startwert und ein
%   groesserer Wert aus FOC Studio wuerde ueberlaufen):
%     Strom   P fixdt(1,32,24) (+-128 PU),  I fixdt(1,32,16) (+-32768 1/s)
%     Drehzahl P fixdt(1,32,24),            I fixdt(1,32,24)
%   Werte unveraendert (Kopien), daher nur Quantisierung der Gains anders.
%   Vorher/nachher werden ausgegeben; focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');
cc = [lib '/Torque Controller/FOC/Current_Controllers'];
cfg = {
    [cc '/PI_Controller_Id/PID Controller'], 'focp_tune_cur_Kp', 'focp_tune_cur_Ki', 'fixdt(1,32,24)', 'fixdt(1,32,16)', '', ''
    [cc '/PI_Controller_Iq/PID Controller'], 'focp_tune_cur_Kp', 'focp_tune_cur_Ki', 'fixdt(1,32,24)', 'fixdt(1,32,16)', '', ''
    [lib '/Speed Control/PI_Controller_Speed/PID Controller'], 'focp_tune_spd_Kp', 'focp_tune_spd_Ki', ...
        'fixdt(1,32,24)', 'fixdt(1,32,24)', 'focp_tune_iq_max', 'focp_tune_iq_min'};
keys = {'P', 'I', 'PParamDataTypeStr', 'IParamDataTypeStr', 'UpperSaturationLimit', 'LowerSaturationLimit'};
for r = 1:size(cfg, 1)
    blk = cfg{r, 1};
    fprintf('%s\n  vorher:  %s\n', blk, show(blk));
    vals = cfg(r, 2:end);
    for k = 1:numel(keys)
        if ~isempty(vals{k}), set_param(blk, keys{k}, vals{k}); end
    end
    fprintf('  nachher: %s\n', show(blk));
end
save_system(lib);
fprintf('focLib gespeichert.\n');

    function s = show(blk)
        s = strjoin(cellfun(@(k) sprintf('%s=%s', k, get_param(blk, k)), keys, 'UniformOutput', false), ', ');
    end
end
