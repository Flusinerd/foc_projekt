function foc_clean_custom_deps(model)
%FOC_CLEAN_CUSTOM_DEPS  Abhaengigkeits- und Objektdateien der FOCP-Quellen vor dem Build loeschen.
%
%   foc_clean_custom_deps('app_speed')
%
%   Das Blockset erzeugt fuer focp*.c fehlerhafte .d-Dateien: die per #include eingebundenen
%   Tabellen focp_params.def/focp_signals.def stehen dort als Zeilen ohne Ziel. Beim naechsten
%   inkrementellen Build bricht gmake mit "focp.d:30: *** missing separator" ab
%   (reports/08_FOCP_Einbau_Plan_2026-11-04.md). Ohne .d und .obj werden die Quellen neu uebersetzt.

root = fileparts(fileparts(mfilename('fullpath')));
d = fullfile(root, [model '_ert_rtw']);
if ~exist(d, 'dir'), return; end
f = [dir(fullfile(d, 'focp*.d')); dir(fullfile(d, 'focp*.obj'))];
for k = 1:numel(f), delete(fullfile(d, f(k).name)); end
if ~isempty(f), fprintf('foc_clean_custom_deps: %d Dateien geloescht\n', numel(f)); end
end
