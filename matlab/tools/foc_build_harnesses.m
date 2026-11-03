function foc_build_harnesses()
%FOC_BUILD_HARNESSES  Schritt 6: MIL-Harnesses der MA auf die Bibliotheksbloecke in focLib uebertragen.
%
%   foc_build_harnesses()
%
%   Fuer jeden der vier Harnesses aus impmodel.slx (Masterarbeit):
%     1. alten Harness laden (impmodel wird NICHT gespeichert), Test Sequence und
%        Test Assessment in ein temporaeres Modell kopieren, alten Harness schliessen
%     2. auf dem gleichnamigen Block in focLib einen neuen Harness anlegen
%        (Quelle Test Sequence, Senke Test Assessment, intern in focLib.slx)
%     3. die generierten Sequenz-/Assessment-Bloecke durch die Kopien ersetzen und
%        an dieselben Ports anschliessen; Ausgaenge des Prueflings fuer Baselines loggen
%   Gibt vorher/nachher aus und speichert focLib. Vorhandene gleichnamige Harnesses in
%   focLib fuehren zum Abbruch (nicht ueberschreiben).
%
%   Siehe matlab/reports/06_Vorbereitung_2026-11-03.md.

root = fileparts(fileparts(mfilename('fullpath')));
legacyFile = fullfile(root, 'models', 'legacy', 'impmodel.slx');
lib = 'focLib';
map = { ...
    'Freemode_Check_TestHarness',  'Speed Control/Digital_Freemode_Check'; ...
    'MaxVel_TestHarness',          'Speed Control/Speed calculator'; ...
    'SpeedController_TestHarness', 'Speed Control/PI_Controller_Speed'; ...
    'SafeState_TestHarness',       'Speed Control/Mechanical_Freemode_Check'};

load_system(legacyFile);
cleanLegacy = onCleanup(@() close_system('impmodel', 0));
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');

fprintf('Vorher: Harnesses in %s:\n', lib); listHarnesses(lib);
for k = 1:size(map, 1)
    if ~isempty(sltest.harness.find([lib '/' map{k,2}], 'Name', map{k,1}))
        error('foc:harness', 'Harness %s existiert bereits in %s. Nicht ueberschrieben.', map{k,1}, lib);
    end
end

tmp = 'foc_harness_tmp';
for k = 1:size(map, 1)
    hname = map{k,1};
    oldOwner = ['impmodel/' map{k,2}];
    newOwner = [lib '/' map{k,2}];
    fprintf('\n=== %s\n  alt: %s\n  neu: %s\n', hname, oldOwner, newOwner);

    % ---- 1. Sequenz und Assessment aus dem alten Harness sichern ---------------------------
    if bdIsLoaded(tmp), close_system(tmp, 0); end
    new_system(tmp);
    sltest.harness.load(oldOwner, hname);
    % Testfaelle der MA nutzen StopTime und Solver des Harness (Library hat keinen Solver)
    solverPar = {'SolverType', 'Solver', 'FixedStep', 'StopTime'};
    solverVal = cellfun(@(p) get_param(hname, p), solverPar, 'UniformOutput', false);
    [oldSrc, oldSnk] = seqBlocks(hname);
    % Effektiver Takt des alten Assessments (dort geerbt ueber From-Bloecke im Takt des
    % Prueflings). Ohne das prueft das neue Assessment im Sequenztakt zwischen zwei
    % Regler-Abtastungen und meldet direkt nach jedem Sollwertsprung einen Fehler.
    feval(hname, [], [], [], 'compile');
    tsA = get_param(oldSnk, 'CompiledSampleTime');
    feval(hname, [], [], [], 'term');
    if iscell(tsA), tsA = tsA{1}; end
    asmtTs = tsName(tsA(1));
    add_block(oldSrc, [tmp '/SRC']);
    add_block(oldSnk, [tmp '/SNK']);
    close_system(hname, 0);

    % ---- 2. neuen Harness anlegen ---------------------------------------------------------
    sltest.harness.create(newOwner, 'Name', hname, 'Source', 'Test Sequence', ...
        'Sink', 'Test Assessment', 'SaveExternally', false, ...
        'Description', sprintf('Uebertragen aus %s (MA Akguel) durch foc_build_harnesses', oldOwner));
    sltest.harness.load(newOwner, hname);

    % ---- 3. generierten Block durch Original-Sequenz und -Assessment ersetzen -------------
    % R2023a erzeugt einen kombinierten Test-Sequence-Block (Eingaenge = Ausgaenge des
    % Prueflings, Ausgaenge = Eingaenge des Prueflings). Verbunden wird ueber Signalnamen.
    replaceCombined(hname, [tmp '/SRC'], [tmp '/SNK']);
    ch = sfroot().find('-isa', 'Stateflow.ReactiveTestingTableChart');
    ch = ch(strcmp(get(ch, 'Path'), [hname '/Test Assessment']));
    if numel(ch) ~= 1
        error('foc:harness', 'Chart %s/Test Assessment nicht eindeutig gefunden (%d).', hname, numel(ch));
    end
    ch.ChartUpdate = 'DISCRETE';   % SampleTime wirkt nur bei diskreter Update-Methode
    ch.SampleTime = asmtTs;
    if ~strcmp(ch.SampleTime, asmtTs)
        error('foc:harness', 'Assessment-Takt in %s nicht gesetzt (ist %s).', hname, ch.SampleTime);
    end
    fprintf('  Assessment-Takt = %s (wie alter Harness)\n', ch.SampleTime);
    logOwnerOutputs(hname, map{k,2});
    for p = 1:numel(solverPar), set_param(hname, solverPar{p}, solverVal{p}); end
    fprintf('  Solver wie alter Harness: %s\n', strjoin(strcat(solverPar, '=', solverVal), ', '));
    save_system(hname);
    close_system(hname, 0);
    close_system(tmp, 0);
    fprintf('  angelegt.\n');
end

save_system(lib);
fprintf('\nNachher: Harnesses in %s:\n', lib); listHarnesses(lib);
fprintf('focLib gespeichert.\n');
end

% -------------------------------------------------------------------------------------------
function listHarnesses(model)
h = sltest.harness.find(model);
if isempty(h), fprintf('  (keine)\n'); end
for i = 1:numel(h)
    fprintf('  %-30s <- %s\n', h(i).name, strrep(h(i).ownerFullPath, sprintf('\n'), ' '));
end
end

function [src, snk] = seqBlocks(h)
% Test-Sequence-Bloecke auf oberster Ebene: Quelle hat Ausgaenge, Senke nur Eingaenge.
b = find_system(h, 'SearchDepth', 1, 'BlockType', 'SubSystem');
src = ''; snk = '';
for i = 1:numel(b)
    try, t = get_param(b{i}, 'SFBlockType'); catch, t = ''; end
    if ~strcmp(t, 'Test Sequence'), continue; end
    ph = get_param(b{i}, 'PortHandles');
    if ~isempty(ph.Outport), src = b{i}; else, snk = b{i}; end
end
if isempty(src) || isempty(snk)
    error('foc:harness', 'Test Sequence/Test Assessment in %s nicht gefunden.', h);
end
end

function replaceCombined(h, srcTpl, snkTpl)
% Ersetzt den kombinierten Test-Sequence-Block des neuen Harness durch die Vorlagen
% (Quelle srcTpl, Senke snkTpl). Zuordnung ueber die Namen der Ein-/Ausgangssymbole.
b = find_system(h, 'SearchDepth', 1, 'BlockType', 'SubSystem');
comb = '';
for i = 1:numel(b)
    try, t = get_param(b{i}, 'SFBlockType'); catch, t = ''; end
    if strcmp(t, 'Test Sequence'), comb = b{i}; end
end
if isempty(comb), error('foc:harness', 'Kein Test-Sequence-Block in %s.', h); end

% Gegenstellen des kombinierten Blocks je Symbolname merken
ph = get_param(comb, 'PortHandles');
outN = portNames(comb, 'Outport'); inN = portNames(comb, 'Inport');
dst = containers.Map(); srcOf = containers.Map();
for i = 1:numel(ph.Outport)
    ln = get_param(ph.Outport(i), 'Line');
    dst(outN{i}) = get_param(ln, 'DstPortHandle'); delete_line(ln);
end
for i = 1:numel(ph.Inport)
    ln = get_param(ph.Inport(i), 'Line');
    srcOf(inN{i}) = get_param(ln, 'SrcPortHandle'); delete_line(ln);
end
pos = get_param(comb, 'Position');
delete_block(comb);

% Quelle: jeder Ausgang muss einen gleichnamigen Pruefling-Eingang haben
src = add_block(srcTpl, [h '/Test Sequence'], 'Position', pos);
sN = portNames(src, 'Outport'); sPh = get_param(src, 'PortHandles');
if ~isequal(sort(sN), sort(outN))
    error('foc:harness', '%s: Sequenz-Ausgaenge {%s} passen nicht zu Pruefling-Eingaengen {%s}.', ...
        h, strjoin(sN, ','), strjoin(outN, ','));
end
for i = 1:numel(sN)
    d = dst(sN{i});
    for j = 1:numel(d), add_line(h, sPh.Outport(i), d(j), 'autorouting', 'on'); end
    srcOf(sN{i}) = sPh.Outport(i);
end
% Wie im alten Harness liest das Assessment die Stimuli NACH dem Input Conversion Subsystem
% (gleiche Signale und gleicher Takt wie der Pruefling), nicht direkt an der Sequenz.
conv = [h '/Input Conversion Subsystem'];
if getSimulinkBlockHandle(conv) ~= -1
    cPh = get_param(conv, 'PortHandles');
    for i = 1:numel(cPh.Outport)
        ln = get_param(cPh.Outport(i), 'Line');
        dstBlk = get_param(ln, 'DstBlockHandle'); dstPort = get_param(ln, 'DstPortHandle');
        ipName = find_system(dstBlk(1), 'SearchDepth', 1, 'BlockType', 'Inport', ...
            'Port', num2str(get_param(dstPort(1), 'PortNumber')));
        nm = get_param(ipName(1), 'Name');
        if isKey(srcOf, nm), srcOf(nm) = cPh.Outport(i); end
    end
end

% Senke: jeder Eingang = Stimulus oder Pruefling-Ausgang gleichen Namens
snk = add_block(snkTpl, [h '/Test Assessment'], 'Position', pos + [0 120 0 120]);
aN = portNames(snk, 'Inport'); aPh = get_param(snk, 'PortHandles');
for i = 1:numel(aN)
    if ~isKey(srcOf, aN{i})
        error('foc:harness', '%s: Assessment-Eingang "%s" hat keine Quelle.', h, aN{i});
    end
    add_line(h, srcOf(aN{i}), aPh.Inport(i), 'autorouting', 'on');
end
fprintf('  Sequenz {%s} -> Pruefling, Assessment liest {%s}\n', strjoin(sN, ', '), strjoin(aN, ', '));
end

function s = tsName(ts)
% Abtastzeit als Ausdruck: Name der passenden Workspace-Variable, sonst Zahl.
s = num2str(ts, 17);
for v = {'Ts_speed', 'Ts', 'Ts_simulink'}
    if evalin('base', sprintf('exist(''%s'', ''var'')', v{1})) && abs(evalin('base', v{1}) - ts) < 1e-12
        s = v{1}; return;
    end
end
end

function n = portNames(blk, type)
% Namen der Port-Bloecke im Inneren, sortiert nach Portnummer.
if ~ischar(blk), blk = getfullname(blk); end
p = find_system(blk,'LookUnderMasks', 'all', 'FollowLinks', 'on', 'SearchDepth', 1, 'BlockType', type);
num = cellfun(@(x) str2double(get_param(x, 'Port')), p);
[~, o] = sort(num);
n = cellfun(@(x) get_param(x, 'Name'), p(o), 'UniformOutput', false);
n = n(:)';
end

function logOwnerOutputs(h, ownerRel)
% Ausgaenge des Prueflings loggen (Grundlage der Baselines).
[~, ownerName] = fileparts(strrep(ownerRel, '/', filesep));
b = [h '/' ownerName];
if getSimulinkBlockHandle(b) == -1
    error('foc:harness', 'Pruefling %s im Harness nicht gefunden.', b);
end
ph = get_param(b, 'PortHandles');
for i = 1:numel(ph.Outport)
    set_param(ph.Outport(i), 'DataLogging', 'on');
end
fprintf('  %d Ausgaenge von %s geloggt\n', numel(ph.Outport), ownerName);
end
