function result = foc_check_equivalence(fileA, fileB, stopTime, profile)
%FOC_CHECK_EQUIVALENCE  Vergleicht zwei Modelle per Simulation (Variante "sim").
%
%   foc_check_equivalence()     impmodel (Original) gegen models/app_speed.slx, 0,5 s
%   r = foc_check_equivalence(fileA, fileB, stopTime, profile)
%
%   Protokolliert die Ausgaenge der Hauptkomponenten in beiden Modellen und meldet die
%   groesste Abweichung je Signal. Nach P1a (reines Umstrukturieren) muessen alle
%   Abweichungen exakt 0 sein.

root = fileparts(fileparts(mfilename('fullpath')));
if nargin < 1 || isempty(fileA), fileA = fullfile(root, 'models', 'legacy', 'impmodel.slx'); end
if nargin < 2 || isempty(fileB), fileB = fullfile(root, 'models', 'app_speed.slx'); end
if nargin < 3 || isempty(stopTime), stopTime = 0.5; end
if nargin < 4 || isempty(profile), profile = 'legacy_f2838'; end

foc_apply_config(foc_config(profile), false);
addpath(fullfile(root, 'models'));

probes = {'Input Controller', 'Speed Control', 'Torque Controller', ...
          'Inverter and Motor - Plant Model'};
files = {fileA, fileB};
logs = cell(1, 2);
for i = 1:2
    [~, name] = fileparts(files{i});
    if bdIsLoaded(name), close_system(name, 0); end
    load_system(files{i});
    for k = 1:numel(probes)
        blk = [name '/' probes{k}];
        ph = get_param(blk, 'PortHandles');
        for o = 1:numel(ph.Outport)
            set_param(ph.Outport(o), 'DataLogging', 'on', ...
                'DataLoggingNameMode', 'Custom', ...
                'DataLoggingName', sprintf('%s_%d', regexprep(probes{k}, '\W', '_'), o));
        end
    end
    fprintf('Simuliere %s (%g s) ...\n', name, stopTime);
    out = sim(name, 'StopTime', num2str(stopTime), 'SignalLogging', 'on', ...
        'SignalLoggingName', 'logsout', 'ReturnWorkspaceOutputs', 'on');
    logs{i} = out.get('logsout');
    close_system(name, 0);       % Logging-Einstellungen nicht speichern
end

names = logs{1}.getElementNames();
result = struct('signal', {}, 'maxAbsDiff', {}, 'samples', {});
fprintf('\n%-45s %12s %8s\n', 'Signal', 'max|A-B|', 'Werte');
allZero = true;
for k = 1:numel(names)
    a = logs{1}.getElement(names{k}).Values;
    b = logs{2}.getElement(names{k}).Values;
    da = double(a.Data(:)); db = double(b.Data(:));
    if numel(da) ~= numel(db)
        d = Inf;
    else
        d = max(abs(da - db));
        if isempty(d), d = 0; end
    end
    allZero = allZero && d == 0;
    result(end+1) = struct('signal', names{k}, 'maxAbsDiff', d, 'samples', numel(da)); %#ok<AGROW>
    fprintf('%-45s %12.3g %8d\n', names{k}, d, numel(da));
end
if allZero
    fprintf('\nERGEBNIS: identisch (alle Abweichungen 0)\n');
else
    fprintf('\nERGEBNIS: Abweichungen vorhanden\n');
end
end
