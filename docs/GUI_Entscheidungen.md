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

**Entscheidung: Qwt 6.3.0**, Kurven mit `QwtPlotCurve::FilterPointsAggressive`.

### Benchmark

Programm: `host/bench/plotbench/` (eigenständiges CMake-Projekt, beide Bibliotheken per FetchContent auf festen Tags). Last: 8 Signale, je 20 000 Punkte im Fenster (f_s = 10 kHz, 2 s), Timer 30 Bilder/s, je Bild ca. 333 neue Punkte pro Signal. Jedes Bild wird im Timer-Slot synchron gezeichnet, die Bildzeit umfasst Datenaktualisierung, Replot und Paint. Fenster 1600 × 900, Messung 10 s nach 15 Bildern Vorlauf. Optionen: `--plots 1|8` (Signale auf 1 oder 8 Plots verteilt), `--aa` (Antialiasing der Kurven), `--fast` (Qwt: `FilterPointsAggressive`; JKQtPlotter: ohne Gitter, die Version 4.0 hat keine Punktreduktion).

Rechner: Intel Core i7-10700, Intel UHD 630, Qt 6.12.0 MSVC 2022 64-bit, Release, 2026-11-03.

| Bibliothek | Plots | fast | AA | Bilder/s | Bildzeit Mittel | p95 | CPU |
|---|---|---|---|---|---|---|---|
| Qwt | 1 | – | – | 23,1 | 43,2 ms | 49,7 ms | 99 % |
| Qwt | 1 | ✓ | – | **30,3** | **24,4 ms** | 29,3 ms | 44 % |
| Qwt | 1 | – | ✓ | 18,0 | 55,4 ms | 61,6 ms | 98 % |
| Qwt | 1 | ✓ | ✓ | 30,3 | 30,7 ms | 34,1 ms | 61 % |
| Qwt | 8 | – | – | 22,4 | 44,7 ms | 47,8 ms | 97 % |
| Qwt | 8 | ✓ | – | **30,4** | **30,6 ms** | 33,0 ms | 57 % |
| Qwt | 8 | – | ✓ | 19,5 | 51,4 ms | 61,6 ms | 98 % |
| Qwt | 8 | ✓ | ✓ | 29,9 | 33,2 ms | 36,7 ms | 89 % |
| JKQtPlotter | 1 | – | – | 0,5 | 2079 ms | 2082 ms | 64 % |
| JKQtPlotter | 1 | ✓ | – | 0,5 | 2088 ms | 2095 ms | 69 % |
| JKQtPlotter | 1 | – | ✓ | 0,2 | 5708 ms | 5698 ms | 7 % |

Die Messreihe wurde nach diesen Läufen abgebrochen. JKQtPlotter lag in allen Läufen um den Faktor 70–200 über dem Zeitbudget von 33 ms, die übrigen Läufe hätten daran nichts geändert.

### Begründung

1. **Leistung:** Nur Qwt erreicht die geforderten 30 Bilder/s, und das nur mit `FilterPointsAggressive` (Reduktion auf Min/Max je Pixelspalte). Ohne diese Option schafft auch Qwt nur 18–23 Bilder/s bei voller Auslastung eines Kerns. JKQtPlotter 4.0.3 zeichnet jeden Punkt und braucht 2 s pro Bild.
2. **Einbindung in CMake:** JKQtPlotter bringt eigenes CMake mit, baut aber vier Bibliotheken (Common, MathText inkl. XITS-Schriften, FastPlotter, Plotter) und braucht zusätzlich Qt Svg, Xml und OpenGL. Qwt hat upstream nur qmake. Ein eigenes Ziel in CMake sind aber ca. 10 Zeilen (Quellen per Glob, ohne SVG/OpenGL/Polar, `QWT_NO_SVG`, `QWT_NO_OPENGL`), Abhängigkeiten nur Widgets, PrintSupport, Concurrent. Beides ist reproduzierbar per FetchContent auf festem Tag.
3. **Lizenz:** Qwt License 1.0 = LGPL 2.1 mit Ausnahmen; statisches Linken ausdrücklich erlaubt. JKQtPlotter LGPL 2.1. Beide wären zulässig.

### Folgerungen für die GUI

- Kurven immer mit `FilterPointsAggressive` und `setRawSamples()` (kein Kopieren).
- Antialiasing standardmäßig aus; mit 1 Plot kostet es ca. 25 % mehr Bildzeit, bei 8 Plots wird die CPU-Last grenzwertig.
- Bei 8 Plots liegt das p95 knapp über 33 ms. Im Scope wird deshalb nur der sichtbare Tab neu gezeichnet, und die Bildrate darf bei vielen Plots auf 20 Bilder/s fallen.

## 4. CANable-Transport (gs_usb)

*Offen. Prüfung vorhandener Bibliotheken (z. B. candle_api) und ihrer Lizenzen folgt in Schritt 4.*
