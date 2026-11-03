function foc_build_library(srcFile, outDir)
%FOC_BUILD_LIBRARY  P1a: Altmodell in Bibliothek + Anwendungsmodell ueberfuehren.
%
%   foc_build_library()                    Quelle models/legacy/impmodel.slx
%   foc_build_library(srcFile, outDir)
%
%   Ergebnis (in outDir, Standard: models/):
%     focLib.slx     Bibliothek mit den Komponenten des Altmodells (unveraendert kopiert)
%     app_speed.slx  Kopie von impmodel, in der diese Komponenten durch
%                    Bibliotheksverweise (Library Links) ersetzt sind
%
%   Das Verhalten aendert sich dabei NICHT. Nachweis: foc_check_equivalence.
%   Das Originalmodell wird nur gelesen, nie gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(srcFile), srcFile = fullfile(root, 'models', 'legacy', 'impmodel.slx'); end
if nargin < 2 || isempty(outDir),  outDir  = fullfile(root, 'models'); end

lib = 'focLib';
app = 'app_speed';
% Komponenten auf oberster Ebene, die in die Bibliothek wandern
components = { ...
    'Input Controller', ...
    'Speed Control', ...
    'Torque Controller', ...
    'Hall Sensor A', 'Hall Sensor B', 'Hall Sensor C', ...
    'ADC Offset Function', ...
    'Inverter and Motor - Plant Model'};

[~, srcName] = fileparts(srcFile);
closeIfLoaded(lib); closeIfLoaded(app); closeIfLoaded(srcName);
if ~exist(srcFile, 'file'), error('foc:build', 'Quellmodell nicht gefunden: %s', srcFile); end
if ~exist(outDir, 'dir'), mkdir(outDir); end
addpath(outDir);

% Konfiguration wird zum Laden nicht gebraucht, aber fuer die Pruefung am Ende
if ~evalin('base', 'exist(''PU_System'', ''var'')')
    foc_apply_config(foc_config('legacy_f2838'), false);
end

% ---- 1. Quelle pruefen --------------------------------------------------------------------
load_system(srcFile);
for k = 1:numel(components)
    hit = find_system(srcName, 'SearchDepth', 1, 'Name', components{k});
    if numel(hit) ~= 1
        error('foc:build', 'Block "%s" auf oberster Ebene von %s nicht eindeutig gefunden.', ...
            components{k}, srcName);
    end
end

% ---- 2. Bibliothek anlegen ----------------------------------------------------------------
new_system(lib, 'Library');
for k = 1:numel(components)
    dst = [lib '/' components{k}];
    add_block([srcName '/' components{k}], dst);
    col = mod(k-1, 4); row = floor((k-1)/4);
    set_param(dst, 'Position', [40 + 260*col, 40 + 200*row, 220 + 260*col, 160 + 200*row]);
end
set_param(lib, 'Description', ['FOC-Framework Komponentenbibliothek. Aus ' srcName ...
    ' erzeugt durch foc_build_library am ' datestr(now, 'yyyy-mm-dd HH:MM')]);
libFile = fullfile(outDir, [lib '.slx']);
save_system(lib, libFile);
close_system(srcName, 0);
fprintf('Bibliothek gespeichert: %s\n', libFile);

% ---- 3. Anwendungsmodell: Kopie des Originals, Bloecke durch Links ersetzen ---------------
appFile = fullfile(outDir, [app '.slx']);
copyfile(srcFile, appFile, 'f');
fileattrib(appFile, '+w');
load_system(appFile);
% Die Test-Harnesses der MA haengen an Bloecken, die gleich ersetzt werden. In der Kopie
% entfernen; im Originalmodell bleiben sie erhalten (Uebertragung auf focLib: P1, Schritt 6).
try
    hs = sltest.harness.find(app);
    for h = 1:numel(hs)
        sltest.harness.delete(hs(h).ownerFullPath, hs(h).name);
        fprintf('  Harness in Kopie entfernt: %s (%s)\n', hs(h).name, hs(h).ownerFullPath);
    end
catch err
    fprintf('  Hinweis: Harnesses nicht entfernt (%s)\n', err.message);
end
for k = 1:numel(components)
    name = components{k};
    replaced = replace_block(app, 'Name', name, [lib '/' name], 'noprompt');
    if numel(replaced) ~= 1
        error('foc:build', 'replace_block hat fuer "%s" %d Bloecke ersetzt (erwartet 1).', name, numel(replaced));
    end
    blk = replaced{1};
    ref = get_param(blk, 'ReferenceBlock');
    if ~strcmp(ref, [lib '/' name])
        error('foc:build', '"%s" ist danach kein Link auf %s (ReferenceBlock = "%s").', blk, lib, ref);
    end
    fprintf('  verlinkt: %s -> %s\n', blk, ref);
end
set_param(app, 'Description', ['Anwendung Drehzahlregelung (CarPediem). Komponenten aus ' lib '.']);
% Coverage war im Altmodell aktiv; ohne Simulink-Coverage-Lizenz nur Warnungen -> abschalten
if ~license('test', 'Simulink_Coverage')
    try
        set_param(app, 'CovEnable', 'off');
        fprintf('  Coverage abgeschaltet (keine Simulink-Coverage-Lizenz).\n');
    catch err
        fprintf('  Hinweis: Coverage nicht abschaltbar (%s)\n', err.message);
    end
end
save_system(app);

% ---- 4. Kurzpruefung: Modell laesst sich uebersetzen --------------------------------------
try
    set_param(app, 'SimulationCommand', 'update');
    fprintf('app_speed: Update Diagram erfolgreich.\n');
catch err
    warning('foc:build', 'Update Diagram fehlgeschlagen:\n%s', err.message);
end
fprintf('Anwendungsmodell gespeichert: %s\n', appFile);
fprintf('Naechster Schritt: foc_check_equivalence\n');
end

function closeIfLoaded(name)
if bdIsLoaded(name), close_system(name, 0); end
end
