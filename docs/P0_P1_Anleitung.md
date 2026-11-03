# P0/P1 – Fundament und Bibliothek (modellbasiert, MATLAB/Simulink)

**Ziel:** Die Firmware der MA Akgül (`impmodel.slx`) wird zur Basis des FOC-Frameworks:
- **P0:** Die Parameter kommen aus nachvollziehbaren Skripten statt aus `params_f2838.mat`. Das Modell bleibt unverändert und verhält sich identisch.
- **P1a:** Die Komponenten wandern in eine Simulink-Bibliothek `focLib.slx`, die Anwendung `app_speed.slx` verweist darauf. Der Simulationsvergleich muss bitgleiche Ergebnisse liefern.
- **P1b:** Bekannte Fehler in den Reglern werden einzeln und abschaltbar korrigiert (Befunde B1/B2).

Alles liegt im Ordner `matlab/` des Repos. Wo ich nicht in MATLAB testen konnte, steht das ausdrücklich dabei.

## Ordner

```
matlab/
  setup_project.m          legt einmalig das MATLAB-Projekt an
  foc_startup.m            Pfade + Konfiguration (Startdatei des Projekts)
  config/
    foc_config.m           Einstieg: Profile 'legacy_f2838' und 'datasheet'
    foc_apply_config.m     legt die Variablen im Base Workspace an
    foc_legacy_f2838.m     exakt der Inhalt von params_f2838.mat
    foc_cfg_motor_eci6360.m      Motor aus Datenblatt, Ableitungen berechnet
    foc_cfg_inverter_drv8300.m   Leistungsstufe
    foc_cfg_board_carpediem.m    Steuerplatine F28386D
    foc_pu_system.m        Bezugsgrößen (Konvention der MA)
  design/
    foc_design_controllers.m     Reglerauslegung (ersetzt mcb.internal.SetControllerParameters)
  tools/
    foc_build_library.m    P1a: focLib.slx + app_speed.slx erzeugen
    foc_check_equivalence.m      P1a: Simulationsvergleich Original ↔ app_speed
    foc_apply_fixes.m      P1b: Reglerkorrekturen in focLib
  tests/
    test_config.m          prüft die Parameterskripte (läuft auch in Octave)
  models/legacy/           impmodel.slx und params_f2838.mat aus der MA (nur lesen)
```

| Skript | Getestet |
|---|---|
| `config/*`, `design/*`, `tests/test_config.m`, `foc_startup.m` | in GNU Octave 8.4 ausgeführt, alle Prüfungen bestanden |
| `tools/*`, `setup_project.m` | nur Syntaxprüfung, **nicht in MATLAB ausgeführt** |

## Schritt 0 – MATLAB-Version

Die MA wurde mit **R2023a** gebaut: C2000 Microcontroller Blockset, Embedded Coder, Motor Control Blockset, Simulink Test und Coverage, Powertrain Blockset (Streckenmodell „Interior PMSM“).

```matlab
ver   % Ausgabe bitte zurückmelden
```

Bei einer neueren Version öffnet Simulink das Modell automatisch. Vor dem ersten Speichern einmal den *Upgrade Advisor* laufen lassen (Modeling → Model Advisor → Upgrade Advisor). Die Originaldatei in `models/legacy/` nicht überschreiben.

## Schritt 1 – Projekt anlegen

```matlab
cd <repo>/matlab
setup_project          % einmalig
```

Danach öffnest du immer `FOCFramework.prj`. Pfade und Konfiguration werden automatisch gesetzt.

**Von Hand:** *Home → New → Project → From Folder* → Ordner `matlab`. Dann unter *Project → Startup Shutdown* `foc_startup.m` als Startdatei eintragen und unter *Project Path* die Unterordner hinzufügen.

## Schritt 2 – P0: Parameter aus Skripten

```matlab
test_config                    % muss "ALL PASSED" melden
foc_startup                    % Profil legacy_f2838 (Standard)
open_system('impmodel')        % Originalmodell, ohne .mat zu laden
```

Fertig ist P0, wenn:
1. `test_config` „ALL PASSED“ meldet. Das Profil `legacy_f2838` gibt **jedes Feld** der `.mat` bitgenau wieder, und beide Profile liefern alle 41 Größen, die das Modell referenziert.
2. `impmodel` simuliert (*Run*) und baut (*Build, Deploy & Start*) wie vorher.

Neu gegenüber der `.mat`: Die Variable `dataType` wird jetzt mit angelegt. Das Modell braucht sie für seine Festkomma-Varianten, in der `.mat` fehlt sie aber.

## Schritt 3 – P1a: Bibliothek erzeugen

```matlab
foc_build_library
```

Das Skript
1. liest `models/legacy/impmodel.slx`, ohne es zu verändern,
2. kopiert die acht Hauptkomponenten in die neue Bibliothek `models/focLib.slx`: Input Controller, Speed Control, Torque Controller, Hall Sensor A/B/C, ADC Offset Function, Streckenmodell,
3. legt `models/app_speed.slx` als Kopie an und ersetzt dort diese Komponenten per `replace_block` durch Bibliotheksverweise. Die Verbindungen bleiben dabei erhalten,
4. prüft jeden Verweis (`ReferenceBlock`) und führt einmal *Update Diagram* aus.

Die Test-Harnesses der MA hängen an Blöcken innerhalb dieser Komponenten. In der Kopie werden sie entfernt, im Original bleiben sie (→ Schritt 6).

**Von Hand (z. B. für die Dokumentation):**
1. *New → Library*, als `focLib.slx` speichern.
2. Die acht Subsysteme aus `impmodel` per Strg+C/Strg+V in die Bibliothek kopieren.
3. `impmodel.slx` als `app_speed.slx` speichern.
4. In `app_speed` jedes Subsystem löschen, den gleichnamigen Block aus dem Library Browser einfügen und die Leitungen neu ziehen. Genau das erledigt `replace_block` automatisch.

## Schritt 4 – P1a: Nachweis „gleiches Verhalten“

```matlab
r = foc_check_equivalence            % 0,5 s, Variante "sim"
r = foc_check_equivalence([], [], 2) % längerer Lauf
```

Das Skript schaltet in beiden Modellen das Signal-Logging an allen Ausgängen der Hauptkomponenten ein, simuliert und vergleicht. **Erwartet: Jede Abweichung ist exakt 0.** Die Logging-Einstellungen werden nicht gespeichert.

Das ist der modellbasierte Nachweis für die Doku: Die Umstrukturierung hat das Verhalten nicht verändert.

## Schritt 5 – P1b: Reglerkorrekturen

```matlab
foc_apply_fixes('current_pi')   % oder 'speed_pi' oder 'all'
foc_startup('datasheet')        % Korrekturen setzen Reglerwerte mit Bedeutung [1/s] voraus
sim('app_speed')
```

Jede Korrektur wird einzeln eingeschaltet und protokolliert den Zustand vorher und nachher. So lässt sich der Einfluss jeder Änderung getrennt in Simulation und am Prüfstand zeigen. Erst wenn die Simulation plausibel ist, auf die Hardware gehen:
- Motor ohne Last, Welle frei
- Stromgrenze klein (`Saturation`/DQ-Limiter bzw. Iq-Grenze des Drehzahlreglers)
- Netzteil mit Strombegrenzung, Finger am STOPP-Taster (SafeState)

## Schritt 6 – Tests auf die Bibliothek übertragen

Die vier MIL-Tests der MA (`Testfile_F2838.mldatx`) beziehen sich auf `impmodel/...`. Neu anlegen auf den Bibliotheksblöcken:
1. Rechtsklick auf den Block in `focLib` → *Test Harness → Create for …*
2. Testsequenz und Prüfungen aus dem alten Harness übernehmen. Ausgangspunkt ist die Bedingung im Assessment-Block des alten Tests.
3. Im Test Manager das Systemmodell der Testfälle auf den neuen Harness umstellen.

Harnesses an Bibliotheksblöcken prüfen die Komponente unabhängig von der Anwendung. Genau das ist der Zweck der Bibliothek.

## Danach (P1, Teil 2)

- Hardwarezugriffe (ePWM, ADC, eCAP, GPIO, CAN) aus den Reglerkomponenten herauslösen und in eine **HAL** pro Platine legen (Variante `CARPEDIEM_F28386D` / `SIM`).
- **Bus-Objekte** als Schnittstellen zwischen den Komponenten (Messwerte, Sollwerte, Status) statt Einzelsignalen.
- **Supervisor in Stateflow** (Init → Offsetkalibrierung → Bereit → Betrieb → Fehler).

---

## Befunde aus der Analyse des Altmodells

| Nr. | Befund | Wirkung | Behandlung |
|---|---|---|---|
| **B1** | **I-Anteil des Drehzahlreglers um Größenordnungen zu klein.** Am PID-Block steht `I = Ki_speed_texas*Ts_speed` mit `UseKiTs = on`. `Ki_speed_texas` ist schon Ts/Tn ohne Kp und wird dann noch einmal mit Ts multipliziert: ≈ 6,6·10⁻⁶ statt ≈ 0,095 pro Schritt. | Regler arbeitet praktisch als reiner P-Regler (Kp = 7,2 PU) → bleibende Drehzahlabweichung unter Last | `foc_apply_fixes('speed_pi')` |
| **B2** | **Stromregler:** `P = Kp − 0,0002` (Handkorrektur), `I = Ki_texas*Ts` (Ts/Tn ohne Kp, wie B1), Ausgang auf **[0, 1]** begrenzt | I-Anteil viel zu klein. Negative d/q-Spannung unmöglich, obwohl im Betrieb u_d ≈ −ωL·i_q nötig ist | `foc_apply_fixes('current_pi')` |
| **B3** | **Motordaten:** Die alten Werte sind um Faktoren verschoben (Rs/5, L/500, Ψ/55). Aus dem Datenblatt folgt Ψ = 5,9 mVs: Nur damit passt die Leerlaufdrehzahl von 6000 min⁻¹ bei 24 V. R und L im Datenblatt sind sehr wahrscheinlich Klemmenwerte, pro Strang also die Hälfte (22,5 mΩ, 38 µH). | Reglerauslegung und Streckenmodell stimmen nicht mit dem Motor überein | Profil `datasheet`; endgültig erst nach Motoridentifikation (P4) |
| **B4** | **Strom-Skalierung:** Im Codegenerierungspfad ist der PU-Strom = (ADC − Offset)/2048. 1 PU ist also der Strom, bei dem der Strommessverstärker 1,65 V Hub liefert, nicht `PU_System.I_base` = 17,6 A. `ISenseVoltPerAmp` = 0,003 V/A berücksichtigt nur den Shunt, nicht die Verstärkung des EVM. | Verstärkungen in PU passen eventuell nicht zur realen Strecke | Verstärkung aus dem EVM-Handbuch holen oder messen (Konstantstrom einprägen, Iab_PU beobachten), dann I_base oder Skalierung angleichen |
| **B5** | `dataType` fehlt in der `.mat` | Modell lässt sich ohne Zusatzskript nicht übersetzen | in `foc_config` behoben |
| **B6** | **ADC-Referenz** 3,36 V statt 3,3 V (Hardware-MA, Tab. 6.1) | 1,8 % Messfehler bei Spannung, Temperatur und Sollwert | Profil `datasheet` nutzt 3,36 V; auf jeder Platine nachmessen |

Hinweis zu B1/B2: Ob `UseKiTs = on` bedeutet, dass der Parameter bereits I·Ts enthält oder dass der Block intern mit Ts multipliziert, ändert nur die Größe des Fehlers. In beiden Lesarten ist der I-Anteil mindestens drei Größenordnungen zu klein. Die Korrektur stellt deshalb auf `UseKiTs = off` mit I in 1/s um. Das ist eindeutig.

## Offen / bitte zurückmelden

1. Ausgabe von `ver` (MATLAB-Version, Toolboxen)
2. Konsolenausgabe von `test_config`, `foc_build_library` und `foc_check_equivalence`, vor allem bei Fehlern
3. Klärung mit dem Betreuer: Darf handgeschriebener C-Code (z. B. das Kommunikationsprotokoll über C-Caller-Blöcke) ins Modell, oder muss alles aus Simulink-Blöcken kommen?
