function foc_enable_logging(model, lib)
%FOC_ENABLE_LOGGING  Markiert die Signale fuer Schritt 5 zum Loggen (Simulation Data Inspector).
%
%   foc_enable_logging()                  app_speed + focLib
%   foc_enable_logging(model, lib)
%
%   Oberste Ebene (im Anwendungsmodell):
%     Speed_PU      Ausgang 1 von Torque Controller (gemessene Drehzahl, PU)
%     IdqRef_PU     Ausgang 1 von Speed Control ([Id_ref Iq_ref], PU)
%   Innerhalb der Bibliothek (gilt fuer alle Modelle, die focLib nutzen):
%     N_ref         Ausgang von Speed Control/Speed calculator (Solldrehzahl, PU)
%     Idq_meas_k    Ausgaenge von Torque Controller/FOC/Park Transform (gemessene id/iq)
%     Vdq_ref_k     Ausgaenge von Torque Controller/FOC/Current_Controllers (Vd_ref, Vq_ref)
%
%   Speichert app_speed und focLib.

if nargin < 1 || isempty(model), model = 'app_speed'; end
if nargin < 2 || isempty(lib), lib = 'focLib'; end
load_system(model);
load_system(lib);
set_param(lib, 'Lock', 'off');

top = { ...
    'Torque Controller', 1, 'Speed_PU'; ...
    'Speed Control',     1, 'IdqRef_PU'};
for k = 1:size(top, 1)
    logPort([model '/' top{k,1}], top{k,2}, top{k,3});
end

inner = { ...
    'Speed Control/Speed calculator',          'N_ref'; ...
    'Torque Controller/FOC/Park Transform',    'Idq_meas'; ...
    'Torque Controller/FOC/Current_Controllers', 'Vdq_ref'};
for k = 1:size(inner, 1)
    blk = [lib '/' inner{k,1}];
    if getSimulinkBlockHandle(blk) == -1
        error('foc:log', 'Block nicht gefunden: %s', blk);
    end
    ph = get_param(blk, 'PortHandles');
    n = numel(ph.Outport);
    for o = 1:n
        if n == 1, name = inner{k,2}; else, name = sprintf('%s_%d', inner{k,2}, o); end
        logPort(blk, o, name);
    end
end

save_system(lib);
save_system(model);
fprintf('Logging gesetzt und gespeichert (%s, %s).\n', model, lib);
end

function logPort(blk, port, name)
if getSimulinkBlockHandle(blk) == -1
    error('foc:log', 'Block nicht gefunden: %s', blk);
end
ph = get_param(blk, 'PortHandles');
set_param(ph.Outport(port), 'DataLogging', 'on', ...
    'DataLoggingNameMode', 'Custom', 'DataLoggingName', name);
fprintf('  loggt: %-12s <- %s/Out%d\n', name, blk, port);
end
