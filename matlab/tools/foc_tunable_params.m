function foc_tunable_params()
%FOC_TUNABLE_PARAMS  focp_tune_* im Base Workspace zu ExportedGlobal-Parametern machen.
%
%   foc_startup(<Profil>); foc_tunable_params()
%
%   foc_config legt focp_tune_* als einfache Zahlen an (toolboxfrei, Octave). Fuer die
%   Codegenerierung werden sie zu Simulink.Parameter (single, ExportedGlobal): Im erzeugten Code
%   entstehen globale Variablen gleichen Namens, die firmware/focp_c28x.c zur Laufzeit schreibt
%   (FOC Studio aendert cur.Kp/Ki, spd.Kp/Ki, spd.Iq_max/Iq_min). Aufgerufen von foc_codegen und
%   foc_flash.

names = {'focp_tune_cur_Kp', 'focp_tune_cur_Ki', 'focp_tune_spd_Kp', 'focp_tune_spd_Ki', ...
         'focp_tune_iq_max', 'focp_tune_iq_min'};
for k = 1:numel(names)
    v = evalin('base', names{k});
    if isa(v, 'Simulink.Parameter'), continue; end
    p = Simulink.Parameter(v);
    p.DataType = 'single';
    p.CoderInfo.StorageClass = 'ExportedGlobal';
    assignin('base', names{k}, p);
end
end
