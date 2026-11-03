# Lizenzen der verwendeten Bibliotheken (Host-Teil)

Vorgabe: nur LGPL- oder permissiv lizenzierte Module und Bibliotheken. **Nicht verwendet** werden Qt Charts, Qt Graphs und QCustomPlot (GPL bzw. kommerziell).

Stand: 2026-11-03. Abhängigkeiten außer Qt werden per CMake `FetchContent` auf festen Tags geladen, es liegen keine Binärdateien im Repo.

## FOC Studio (`focstudio`, Qt)

| Komponente | Version | Lizenz | Einbindung | Hinweis |
|---|---|---|---|---|
| Qt Core, Gui, Widgets | 6.12.0 | LGPL 3 | dynamisch (DLL) | Qt-DLLs werden mit `windeployqt` ausgeliefert, Austausch durch den Anwender möglich |
| Qt PrintSupport, Concurrent | 6.12.0 | LGPL 3 | dynamisch | von Qwt benötigt |
| Qwt | 6.3.0 | Qwt License 1.0 = LGPL 2.1 mit Ausnahmen | statisch, aus Quellen (`cmake/Qwt.cmake`) | Ausnahme 2 erlaubt statisches Linken ohne Quellcode-Pflicht für die Anwendung; Änderungen an Qwt selbst müssten offengelegt werden (keine vorgenommen) |
| WinUSB, SetupAPI | Windows | Systembibliotheken | dynamisch | Teil von Windows |

## Kern `fochost` und Tests

Keine Fremdbibliotheken. Der gs_usb-Transport ist eine eigene Umsetzung (siehe `docs/GUI_Entscheidungen.md` §4); candle_api (LGPL 3) wurde geprüft, aber nicht verwendet.

## Entwicklerwerkzeug (`focstudio-dev`, ImGui)

| Komponente | Version | Lizenz |
|---|---|---|
| Dear ImGui | v1.92.7-docking | MIT |
| ImPlot | v1.1 | MIT |
| GLFW | 3.4 | zlib |

## Nur Benchmark (`host/bench/plotbench`, nicht ausgeliefert)

| Komponente | Version | Lizenz |
|---|---|---|
| JKQtPlotter | 4.0.3 | LGPL 2.1 |
| Qwt | 6.3.0 | siehe oben |
