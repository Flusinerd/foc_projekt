function setup_project()
%SETUP_PROJECT  Legt einmalig ein Simulink-Projekt (MATLAB Project) fuer den Ordner an.
%
%   Danach: FOCFramework.prj oeffnen -> Pfade und Konfiguration werden automatisch
%   gesetzt (Startdatei foc_startup.m).
%
%   Falls das Anlegen per Skript in deiner MATLAB-Version scheitert:
%   Home > New > Project > From Folder, Ordner "matlab" waehlen, dann
%   foc_startup.m unter Project > Startup Shutdown als Startdatei eintragen.

root = fileparts(mfilename('fullpath'));
if ~isempty(dir(fullfile(root, '*.prj')))
    fprintf('Im Ordner existiert bereits ein Projekt. Einfach die .prj-Datei oeffnen.\n');
    return;
end

proj = matlab.project.createProject(root);
proj.Name = 'FOC-Framework';
folders = {'config', 'design', 'tools', 'tests', 'models', fullfile('models', 'legacy')};
for k = 1:numel(folders)
    f = fullfile(root, folders{k});
    if exist(f, 'dir')
        addFolderIncludingChildFiles(proj, f);
        addPath(proj, f);
    end
end
addFile(proj, fullfile(root, 'foc_startup.m'));
addFile(proj, fullfile(root, 'setup_project.m'));
addStartupFile(proj, fullfile(root, 'foc_startup.m'));
fprintf('Projekt "%s" angelegt in %s\n', proj.Name, root);
end
