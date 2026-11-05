function foc_remove_ipc_cm()
%FOC_REMOVE_IPC_CM  IPC-Schreibbloecke CPU1 -> CM (Ethernet-Logging der MA) entfernen.
%
%   foc_remove_ipc_cm()
%
%   Die Subsysteme TCP_* enthalten "Interprocess Data Write" (soc.blocks.IPCWrite) fuer das
%   CM-Modell TCP_Server.slx der MA. Auf dem CM laeuft in diesem Aufbau kein Programm; die
%   Ethernet-Patches des Blocksets fehlen. MW_IPC_Write wartet in einer Schleife und blockierte
%   die Strom-ISR (reports/08_Fehler_Firmware_haengt_2026-11-05.md). Telemetrie laeuft ueber FOCP.
%   Jedes TCP_*-Subsystem wird durch einen Terminator am selben Signal ersetzt:
%     focLib:    Input Controller/Subsystem/TCP_temp, Speed Control/TCP_SpeedRef,
%                Torque Controller/TCP_Position, Torque Controller/TCP_Speed
%     app_speed: IPC Write for eCAP INTs/TCP_HallA, _HallB, _HallC, _HallState
%   Vorher/nachher: Anzahl der IPC-Write-Bloecke in app_speed. Modelle werden gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib'; app = 'app_speed';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
if ~bdIsLoaded(app), load_system(fullfile(root, 'models', [app '.slx'])); end
fprintf('vorher:  %d IPC-Write-Bloecke in %s\n', countIpc(app), app);

set_param(lib, 'Lock', 'off');
libBlocks = {'Input Controller/Subsystem/TCP_temp', 'Speed Control/TCP_SpeedRef', ...
             'Torque Controller/TCP_Position', 'Torque Controller/TCP_Speed'};
for k = 1:numel(libBlocks), replaceByTerminator([lib '/' libBlocks{k}]); end
if strcmp(get_param(lib, 'Dirty'), 'on'), save_system(lib); end

% app_speed neu laden, damit die Bibliotheksverknuepfungen den neuen Stand zeigen
close_system(app, 0);
load_system(fullfile(root, 'models', [app '.slx']));
appBlocks = {'TCP_HallA', 'TCP_HallB', 'TCP_HallC', 'TCP_HallState'};
for k = 1:numel(appBlocks), replaceByTerminator([app '/IPC Write for eCAP INTs/' appBlocks{k}]); end
save_system(app);
fprintf('nachher: %d IPC-Write-Bloecke in %s\n', countIpc(app), app);
fprintf('%s und %s gespeichert.\n', lib, app);
end

function replaceByTerminator(blk)
if getSimulinkBlockHandle(blk) == -1
    fprintf('  %s: nicht vorhanden (bereits ersetzt?)\n', blk);
    return;
end
parent = get_param(blk, 'Parent');
name = get_param(blk, 'Name');
ph = get_param(blk, 'PortHandles');
if ~isempty(ph.Outport), error('foc:ipc', '%s hat Ausgaenge - nicht automatisch ersetzbar.', blk); end
src = zeros(1, numel(ph.Inport));
for i = 1:numel(ph.Inport)
    l = get_param(ph.Inport(i), 'Line');
    src(i) = get_param(l, 'SrcPortHandle');
    delete_line(l);
end
pos = get_param(blk, 'Position');
delete_block(blk);
for i = 1:numel(src)
    t = sprintf('%s/%s_Term%d', parent, name, i);
    add_block('simulink/Sinks/Terminator', t, 'Position', [pos(1), pos(2) + 30 * (i - 1), pos(1) + 20, pos(2) + 20 + 30 * (i - 1)]);
    add_line(parent, src(i), get_param(t, 'PortHandles').Inport(1), 'autorouting', 'on');
end
fprintf('  %s -> Terminator\n', strrep(blk, newline, ' '));
end

function n = countIpc(mdl)
b = find_system(mdl, 'LookUnderMasks', 'all', 'FollowLinks', 'on', ...
    'MatchFilter', @Simulink.match.allVariants, 'BlockType', 'MATLABSystem', 'System', 'soc.blocks.IPCWrite');
n = numel(b);
end
