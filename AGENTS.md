# AGENTS.md – Anweisungen für den lokalen Code-Agenten (Windows + MATLAB)

> Für Claude Code als `CLAUDE.md` ins Repo-Wurzelverzeichnis kopieren (oder `@AGENTS.md` aus einer `CLAUDE.md` referenzieren). Andere Agenten lesen `AGENTS.md` direkt.

## 1. Worum es geht

Ausbau der Motorregler-Firmware eines modularen Frequenzumrichters (Hochschule Bochum, Projekt CarpeDiem) zu einem **FOC-Framework**. Die Vorgabe ist **modellbasierte Entwicklung mit MATLAB/Simulink**: Die Firmware wird aus Simulink-Modellen generiert (Embedded Coder, C2000 Microcontroller Blockset). Handgeschriebener Firmware-C-Code ist **nicht** freigegeben, bis der Betreuer das klärt.

- Zielhardware: Steuerplatine mit **TMS320F28386D** (176-QFP), Leistungsstufe **DRV8300DIPW-EVM**, Motor **ebm-papst ECI 63.60** (24 V, 4 Polpaare, Hall-Sensoren)
- Ausgangsstand: Simulink-Modell der Masterarbeit Akgül 2023 (`matlab/models/legacy/impmodel.slx`)
- Hintergrund und Begründungen: `docs/P0_P1_Anleitung.md` (Befunde B1–B6), `docs/FOC-Framework_Konzept.md`, `docs/Hardware-Ueberblick_Frequenzumrichter.md`, `docs/Firmware-Ueberblick_CarPediem.md`

## 2. Verbindliche Regeln

1. **`matlab/models/legacy/` nie verändern.** Das ist die Referenz für alle Vergleiche.
2. **Modelländerungen per Skript** (`add_block`, `set_param`, `replace_block` …) in `matlab/tools/`. Ein Skript, das ein Modell ändert, gibt den Zustand vorher und nachher aus. Wenn eine Änderung nur in der GUI geht: Schritte im Bericht genau notieren.
3. **Eine Änderung pro Schritt, jede einzeln nachweisbar.** Nach jedem abgeschlossenen Schritt Commit und, wo unten angegeben, Git-Tag.
4. **Nie Hardware ansteuern ohne ausdrückliche Freigabe des Menschen in derselben Sitzung.** Das gilt für Flashen, *Build, Deploy & Start*, External Mode, COM-Ports und CAN. Code generieren (`slbuild` ohne Download) ist erlaubt.
5. **Keine Toolbox-internen Dateien ändern** (MATLAB-Installationsordner, C2000-Support-Package).
6. Die Skripte in `matlab/config/` und `matlab/design/` müssen **ohne Toolboxen** laufen (auch in GNU Octave). Keine `arguments`-Blöcke, keine `string`-Klasse, kein `fixdt` außerhalb des vorhandenen Guards.
7. `test_config` muss nach jeder Änderung an `config/` oder `design/` „ALL PASSED“ melden.
8. Generierte Dateien nicht committen: `*_ert_rtw/`, `slprj/`, `*.slxc`, `*.autosave`, `*.asv`, `reports/*.mldatx` über 20 MB. Bei Bedarf `.gitignore` ergänzen.
9. Bei Fehlern **anhalten und berichten**, statt Modelle per Trial-and-Error umzubauen. Den Bericht ablegen unter `matlab/reports/<Schritt>_<JJJJ-MM-TT>.md` (Befehl, vollständige Fehlermeldung, Vermutung, Vorschlag).

## 3. MATLAB vom Agenten aus aufrufen

Jeder Aufruf startet eine neue MATLAB-Instanz. **Der Workspace geht dazwischen verloren**, deshalb beginnt jeder Aufruf mit `foc_startup(<Profil>)`.

```powershell
# im Repo-Wurzelverzeichnis, PowerShell
matlab -batch "cd('matlab'); foc_startup('legacy_f2838'); test_config" -logfile matlab\reports\last.log
```

- `-batch` (ab R2019a): kein GUI, Exitcode ≠ 0 bei unbehandeltem Fehler, Ausgabe auf stdout.
- Start dauert 20–60 s, Simulationen dauern länger: Timeouts großzügig setzen (Simulation 10 min).
- In PowerShell äußere Anführungszeichen `"`, MATLAB-Strings mit `'`.
- Ist `matlab` nicht im PATH: `& "C:\Program Files\MATLAB\R20xxx\bin\matlab.exe" -batch "..."`. Mit `where.exe matlab` bzw. im Installationsordner suchen.
- Ist das MATLAB-Projekt angelegt (`matlab/FOCFramework.prj`), geht alternativ `openProject('matlab/FOCFramework.prj')`. Das Projekt lädt das Profil `legacy_f2838`; danach bei Bedarf `foc_startup('datasheet')`.

## 4. Konfigurationsprofile

| Profil | Inhalt | Verwendung |
|---|---|---|
| `legacy_f2838` | bitgenau `params_f2838.mat` der MA | Vergleiche mit dem Altmodell (P0, P1a), Hardware-Erstinbetriebnahme |
| `datasheet` | Motor aus Datenblatt, Platine F28386D, Regler mit `foc_design_controllers` ausgelegt | ab P1b (Reglerkorrekturen) |

`foc_active_profile` im Base Workspace zeigt das geladene Profil.

## 5. Ablauf

Stand 2026-11-04: **Schritte 0–7 und A2 erledigt.** Tags `P1a` (365f7d0), `P1b` (bd15464, Grenzzyklus, siehe Diagnose), `P1b.1` (b451115, gültiger Stand). Schritt 6 in 6f5006d, Schritt 7 in 24d5f90, A2 in 0340c75. `app_speed` nutzt seit 6fb3c62 den **externen 25-MHz-Oszillator** (INTOSC2 zu ungenau für CAN 1 Mbit/s, `08_Takt_ext25_2026-11-03.md`). CAN auf der Hardware getestet (`08_CAN_Test_2026-11-03.md`), UART/External Mode nicht nutzbar (A8). Nächster Schritt: 8 nach `matlab/reports/08_Inbetriebnahme_Plan_2026-11-03.md`.

Testfirmware ohne PWM liegt unter `matlab/models/test/` (Aufbau: `matlab/tools/foc_build_*probe.m`). Den Takt in Testmodellen immer mit `foc_probe_clock` setzen. Nur `UseInternalOsc`/`OSCCLK` zu ändern ergibt 100 MHz. CAN mitlesen: `host/tools/gsusb_can_check.ps1`. CAN-Register über JTAG: `host/tools/ccs_read_can_status.js`. Nach dem Flashen die Platine neu starten (CCS 21 stellt den Takt nicht wieder her).

### Schritt 0 – Umgebung prüfen (nur lesen)
```matlab
ver
licenses = {'Simulink','MATLAB_Coder','Real-Time_Workshop','RTW_Embedded_Coder', ...
            'Simulink_Test','Simulink_Coverage','Motor_Control_Blockset','Powertrain_Blockset'};
for k = 1:numel(licenses), fprintf('%-24s %d\n', licenses{k}, license('test', licenses{k})); end
```
Ergebnis in `matlab/reports/00_umgebung_<Datum>.md`. Fehlt Simulink Coverage, ist das kein Fehler: Coverage-Warnungen ignorieren (`foc_build_library` schaltet Coverage im Anwendungsmodell dann ab). Prüfen, ob das C2000-Support-Package installiert ist (`matlab.addons.installedAddons`).

### Schritte 1–4 – P0/P1a (erledigt, nur bei Bedarf erneut)
```matlab
foc_startup('legacy_f2838'); test_config                    % "ALL PASSED"
foc_startup('legacy_f2838'); foc_build_library              % erzeugt models/focLib.slx, models/app_speed.slx
foc_startup('legacy_f2838'); r = foc_check_equivalence      % alle Abweichungen exakt 0
```
Danach den Stand taggen, falls noch nicht geschehen: `git tag P1a`.
**Wichtig:** `foc_build_library` überschreibt `focLib.slx` und `app_speed.slx`. Nicht ausführen, nachdem Schritt 5 Korrekturen angewendet hat, es sei denn, das ist ausdrücklich gewollt.

### Schritt 5 – P1b: Reglerkorrekturen, nur Simulation
Voraussetzung: `focLib.slx` mit unkorrigierten Reglern (`UseKiTs = on` an allen PID-Blöcken). Seit `ca35d44` enthält der Stand zusätzlich die Polarität im Sensormodell (`foc_fix_sim_sensor`, für `legacy_f2838` bitgleich). Zurücksetzen daher mit `git checkout ca35d44 -- matlab/models/focLib.slx matlab/models/app_speed.slx`, nicht mehr mit P1a.

```matlab
T = foc_run_p1b       % Läufe A, B, C; ggf. foc_run_p1b(2) für einen kurzen Probelauf
```
`foc_run_p1b` markiert die Logging-Signale (`foc_enable_logging`), simuliert drei Läufe, wendet zwischen den Läufen `foc_apply_fixes('current_pi')` und `foc_apply_fixes('speed_pi')` an und schreibt `matlab/reports/P1b_Kennzahlen.csv` sowie `P1b_Vergleich.mldatx`.

Erwartung (bei Abweichung **berichten, nicht nachstellen**):
| Kennzahl | A (unkorrigiert) | B (+ Stromregler) | C (+ Drehzahlregler) |
|---|---|---|---|
| `vd_min` | ≥ 0 (Begrenzung) | < 0 möglich | < 0 möglich |
| `iq_err_rms` | groß | deutlich kleiner als A | ähnlich B |
| `speed_err_mean` | ≠ 0 (P-Verhalten) | ≠ 0 | ≈ 0 |
| Stabilität (Plots) | – | V_d glatt, i_q folgt Sollwert, `id_err_rms` klein | wie B |
| Vorzeichen | – | Moment folgt i_q,ref (nicht invertiert) | wie B |

Die Tabelle allein reicht nicht: P1b erfüllte sie formal trotz Dauerschwingung (mitgekoppelter Stromkreis, `05_P1b_Diagnose_2026-11-03.md`). Immer die Plots mitbewerten.

Bericht `matlab/reports/05_P1b_<Datum>.md` mit Kennzahlentabelle, Bewertung und den Plots, die `foc_run_p1b` unter `matlab/reports/img/P1b_<Lauf>.png` ablegt (Drehzahl Soll/Ist, i_q Soll/Ist, V_d). Zum interaktiven Ansehen: `Simulink.sdi.load('matlab/reports/P1b_Vergleich.mldatx'); Simulink.sdi.view`. Dann committen, Tag `P1b`.

### Schritt 6 – Test-Harnesses auf die Bibliothek übertragen
Die vier MIL-Tests der MA hängen an Blöcken in `impmodel` (Harness-Namen: `Freemode_Check_TestHarness`, `MaxVel_TestHarness`, `SpeedController_TestHarness`, `SafeState_TestHarness`; Testdatei `Testfile_F2838.mldatx` aus dem MA-Anhang, liegt eventuell nicht im Repo → beim Menschen erfragen).

1. Vorhandene Harnesses ansehen, **ohne das Altmodell zu speichern**:
   ```matlab
   load_system('matlab/models/legacy/impmodel.slx');
   h = sltest.harness.find('impmodel'); struct2table(h)
   ```
2. Für jeden Owner (z. B. `impmodel/Speed Control/Digital_Freemode_Check`) den gleichen Block in `focLib` als Owner nehmen und mit `sltest.harness.create` einen neuen Harness anlegen. Dabei die Testsequenz bzw. den Test-Assessment-Block des alten Harness übernehmen (Inhalt per `sltest.harness.load` und Stateflow-API lesen).
3. Testdatei `matlab/tests/focLib_MIL.mldatx` per `sltest.testmanager` anlegen, je Harness ein Testfall, Anforderungen SW_ANF_01/03/04/05 im Testnamen.
4. Erwartung: Mit dem Stand P1a bestehen alle vier Tests. Mit P1b muss sie ebenfalls bestehen, weil sie nur Vorzeichen und Logik prüfen. Falls nicht: berichten.

Lässt sich ein Schritt nicht automatisieren, die genaue GUI-Abfolge in den Bericht schreiben und den Menschen bitten.

### Schritt 7 – Code generieren (ohne Hardware)
```matlab
foc_startup('legacy_f2838'); slbuild('app_speed')   % nur Code, kein Download
```
Prüfen: Build ohne Fehler, Warnungen sammeln. In den Hardware-Einstellungen des Modells sicherstellen und berichten: Board/Prozessor, Oszillator (Platine: externer **25-MHz**-Oszillator), CPU 200 MHz. **Nicht flashen.**

### Schritt 8 – Hardware-Erstinbetriebnahme (nur auf Anweisung des Menschen)
Reihenfolge, damit der Aufbau zuerst mit Bekanntem geprüft wird:
1. `app_speed` **ohne** Korrekturen (Git-Stand `P1a`) mit `legacy_f2838` flashen. Dieser Stand lief laut MA.
2. Erst danach P1b.1. B4 ist geklärt (SLVUBV6, SLVC817): 3 mΩ × 9,98 V/V, Offset 1,65 V, Strom in den Motor → ADC unter 1,65 V (`invertingAmp = −1`), 1 PU = 56 A. Im Code kompensiert die Negation in `Scaling DutyCycles` diese Umkehr. Vor dem ersten geregelten Lauf messen: kleiner Gleichstrom in Phase A → `Iab_PU(1)` negativ; GHA/GLA komplementär (DRV8300**DI**: INLx invertiert).

Sicherheitsrahmen, den der Mensch bestätigt:
- Labornetzteil mit Strombegrenzung
- Motor ohne Last, Welle frei
- SafeState-Taster griffbereit
- PVDD beobachten

**Rückspeisung:** Ein Labornetzteil nimmt keinen Strom auf. Bremsen (negatives i_q) kann die Zwischenkreisspannung hochtreiben. Für die ersten Läufe die Untergrenze des Drehzahlreglers auf 0 setzen (offene Aufgabe A2).

## 6. Offene Aufgaben (nach Schritt 6, je eigener Schritt mit Bericht)

| Nr. | Aufgabe | Hinweise |
|---|---|---|
| A1 | **Überspannungsabschaltung** im Input Controller (analog zur Unterspannung, Schwelle als Konfigurationsparameter, z. B. 30 V) | Freemode bei Überschreitung; MIL-Test dazu |
| A2 | **Bremsstrom-Grenze** als Parameter (`PI_params` bzw. eigener Struct) statt fest −0,5 im PID-Block | Default 0 für Labornetzteil |
| A3 | **Dritten Phasenstrom** (ADCINB3) einlesen, Plausibilität ia + ib + ic ≈ 0 | SOC auf ePWM6-SOCA, wie Ia/Ib |
| A4 | **Überstrom in Hardware**: CMPSS3 auf I_B (ADCINB2), ADC-PPB-Grenzwerte für I_A/I_C → ePWM-Trip-Zone | Begründung: Hardware-Überblick §3.2 |
| A5 | **HAL** aus den Komponenten herauslösen (Varianten `CARPEDIEM_F28386D`, `SIM`) | P1, Teil 2 |
| A6 | **Bus-Objekte** für Mess-, Soll- und Statussignale | Definitionen in `config/`, Octave-kompatibel halten |
| A7 | **Supervisor in Stateflow** (Init → Offsetkalibrierung → Bereit → Betrieb → Fehler) | ersetzt Freemode-Logik schrittweise |
| A8 | **UART/External Mode** über FT2232 Kanal B (COM4), zurückgestellt | Firmware und Takt korrekt (am 2026-11-04 mit 200 MHz/ext. Oszillator wiederholt, SCIA auf GPIO42/43 und 84/85), trotzdem kein Byte auf COM4 → Hardwarestrecke messen, `Erratas.xlsx`. Prüfmodelle `models/test/`, Bericht `08_ExtMode_Test_2026-11-03.md` |

Hinweis für Builds aus dem Agenten: In der Agentenumgebung ist `NoDefaultCurrentDirectoryInExePath=1` gesetzt, dann scheitert die Make-Stufe („*.bat nicht gefunden“). Vor dem MATLAB-Aufruf `Remove-Item Env:NoDefaultCurrentDirectoryInExePath`. In der MATLAB-GUI tritt das nicht auf.

## 7. Repo-Überblick

```
matlab/            modellbasierte Firmware (Arbeitsbereich dieses Agenten)
  config/ design/  Parameter + Reglerauslegung (toolboxfrei)
  tools/           Skripte, die Modelle erzeugen/ändern/vergleichen
  tests/           test_config.m (+ künftige Testdateien)
  models/          focLib.slx, app_speed.slx; legacy/ = Referenz (nur lesen)
  reports/         Berichte, Kennzahlen, Plots
docs/              Konzept, Befunde, Anleitung P0/P1, Installation, GUI-Entscheidungen, Lizenzen
protocol/ firmware/  Firmware-C-Code (Protokoll FOCP, Target-Anbindung) – nicht anfassen,
                   solange die C-Code-Frage offen ist.
host/              PC-Teil (kein Firmware-Code), ändern nur auf Anweisung des Menschen
  core/            fochost: Transporte (Sim, CANable/gs_usb, SocketCAN, PCAN), Device – Qt-frei halten
  studio/          FOC Studio (Qt 6 Widgets, Qwt), Übersetzungen in translations/
  app/             focstudio-dev (ImGui), Entwicklerwerkzeug
  tests/           ctest-Tests;  bench/  Plot-Benchmark (eigenständig)
cmake/             Qwt.cmake (FetchContent), Packaging.cmake (CPack/NSIS)
```

PC-Teil bauen (Windows, Developer PowerShell; Qt 6 MSVC 2022 64-bit, Details in `docs/Installation.md`):
```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build build; ctest --test-dir build --output-on-failure
```
Auch für FOC Studio gilt Regel 4: Verbindungen zu echter Hardware (CANable, PCAN, „Verbindung testen“) und Freigaben nur mit Zustimmung des Menschen; Tests mit dem virtuellen Antrieb (`--demo`).
