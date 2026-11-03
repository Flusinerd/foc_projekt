# FOC Studio – Entscheidungen zur Qt-Oberfläche

Umstellung von FOC Studio vom Entwicklerwerkzeug (Dear ImGui) auf ein Endanwender-Programm mit Qt 6 Widgets. Die ImGui-Oberfläche bleibt als `focstudio-dev` baubar (CMake-Option `FOC_BUILD_DEV_GUI`). Der Kern `host/core` (Bibliothek `fochost`) bleibt Qt-frei.

## 1. Vorgaben aus der Befragung (2026-11-03)

| Frage | Entscheidung |
|---|---|
| Anwender | Labor-Studierende, Werkstatt/Service, Entwickler/Betreuer. Pedelec-Besitzer sind ausdrücklich **keine** Zielgruppe. |
| Normaler Modus | Dashboard, Fehler und Ereignisse (immer); **Bedienung** (Freigabe, STOPP, Quittieren, Regelmodus, Sollwert); **Parameter lesen** inkl. JSON-Export; **einfaches Scope** (feste Plots Drehzahl und Strom, keine Signalauswahl) |
| Expertenmodus | Parameter ändern und JSON-Import, „Im Antrieb speichern“, volles Scope (Signalauswahl, Zeitfenster, Pause, CSV-Export), Kalibrierungen, Rohparameter, virtueller Antrieb |
| Schutz Expertenmodus | Umschalten im Menü mit **Bestätigungsdialog** und Warnhinweis. Kein Passwort: Der Dialog soll versehentliches Umschalten verhindern, nicht absichtliches. |
| Sprachen | **Deutsch und Englisch**. Voreinstellung ist die Systemsprache, umschaltbar in den Einstellungen (wirkt nach Neustart oder sofort per `QEvent::LanguageChange`). Quelltexte englisch in `tr()`, `.ts`-Dateien `focstudio_de.ts`, `focstudio_en.ts`. |
| Qt | Noch nicht installiert. Der Anwender installiert **Qt 6.8 LTS, MSVC 2022 64-bit** über den Qt Online Installer und meldet den Pfad zu `Qt6Config.cmake`. |
| CANable | Firmware **candleLight (gs_usb)**: geprüft im Geräte-Manager, Gerät „canable gs_usb“, USB VID 1D50 / PID 606F, Interface 0, Treiber **WinUSB** (Microsoft, `winusb.inf`) bereits gebunden. Daraus folgt: `GsUsbTransport` (WinUSB unter Windows, libusb unter Linux) statt `SlcanTransport`. |

## 2. Umgebung (Stand 2026-11-03)

- Visual Studio 2022 Build Tools 17.14 mit „Desktopentwicklung mit C++“: MSVC 19.44 (Toolset 14.44.35207), CMake und Ninja aus den Build Tools. Bauen aus einer Umgebung mit `vcvars64.bat` und `-G Ninja`. Bestandsbau und `ctest` (3/3) bestanden.
- Qt: Zunächst nur Qt 6.12.0 **MinGW** installiert (`C:\Qt\6.12.0\mingw_64`). Diese Variante ist nicht mit MSVC linkbar. Entscheidung: Die Komponente **Qt 6.12.0 MSVC 2022 64-bit** wird nachinstalliert, damit gilt Qt 6.12 statt 6.8 LTS.

## 3. Plot-Bibliothek: Qwt oder JKQtPlotter

*Offen. Der Benchmark (8 Signale, je 20 000 Punkte im Fenster, 30 Bilder/s, laufend nachgeschoben) folgt, sobald Qt und MSVC installiert sind.*

## 4. CANable-Transport (gs_usb)

*Offen. Prüfung vorhandener Bibliotheken (z. B. candle_api) und ihrer Lizenzen folgt in Schritt 4.*
