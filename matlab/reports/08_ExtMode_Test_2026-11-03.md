# External Mode – Machbarkeit und erster Test (2026-11-03) – ANGEHALTEN (Linker, Prüfmodell)

## Nachtrag: Lauf ohne Sandbox (Freigabe des Menschen) – Ursache des Make-Fehlers gefunden

1. **Ohne Sandbox:** derselbe Fehler (`foc_extmode_probe.bat … nicht gefunden`). Die Sandbox ist also **nicht** die Ursache.
2. **Ursache:** In der Prozessumgebung des Agenten ist **`NoDefaultCurrentDirectoryInExePath = 1`** gesetzt (nicht beim Benutzer, nicht systemweit). Dann sucht `cmd.exe` Programme und `.bat`-Dateien nicht im aktuellen Ordner, und der relative Aufruf `foc_extmode_probe.bat` bzw. `app_speed.bat` aus `gmake` schlägt fehl. MATLAB erbt die Variable. **In der MATLAB-GUI des Menschen tritt der Fehler nicht auf.** Für Builds aus dem Agenten: `Remove-Item Env:NoDefaultCurrentDirectoryInExePath` nur für den jeweiligen MATLAB-Aufruf.
3. **Gegenprobe `app_speed`** (Build action im Speicher „Build“, ExtMode aus, kein Laden; Modelle hashgleich): **Build erfolgreich**, `app_speed.out/.hex/.elf` erzeugt (TI-Compiler aus CCS 21, C2000Ware 4.00), keine Compilerwarnung. **Damit ist der Blocker aus Schritt 7 gelöst.** Die Ausgaben liegen in `matlab/app_speed_ert_rtw/_out/` (nicht versioniert). Nebenwirkungen (getrackte Caches `matlab/impmodel.slxc` und `slprj/sim/varcache/app_speed/*`) wurden auf HEAD zurückgesetzt.
4. **Prüfmodell:** Make läuft jetzt, Kompilieren ok, **Linker scheitert** (auch nach sauberem Neuaufbau):
   ```
   undefined        first referenced
   MW_SetVREF       MW_c28xx_adc.obj
   extmodeEvent     ert_main.obj
   extmodeParseArgs ert_main.obj
   extmodeReset     ert_main.obj
   error: unresolved symbols remain
   ```
   Die XCP-Quellen werden mitkompiliert (`xcp_ext_work.c`, `rtiostream_serial_c28x_ext_xcp.c`, …). `MW_SetVREF` wird in `app_speed` gefunden, im Prüfmodell nicht.
   **Vermutung:** Der Fehler liegt am Prüfmodell. Die ADC-Blöcke sind aus `app_speed` kopiert und von der Bibliothek gelöst; es sind ältere `c2802xlib/ADC`-Blöcke mit F2838x-Maske. Dadurch fehlt vermutlich eine Treiberquelle. Für die External-Mode-Laufzeit fehlt entweder eine Quelle oder ein Define, das das Blockset nur bei vollständig eingerichteter Konfiguration ergänzt. **Nicht weiter probiert** (Regel 9).
5. **Nichts geflasht**, keine Verbindung zum Target.

**Vorschlag:** Das Prüfmodell mit frischen Bibliotheksblöcken (`c2838xlib`) statt Kopien aufbauen und External Mode über die Standard-Konfigurationsseite (*Hardware Implementation → Target Hardware Resources → External mode*) setzen statt per `codertarget.data`. Alternativ den MathWorks-Ablauf „Monitor & Tune“ einmal in der GUI mit einem minimalen Modell (GPIO-LED + ADC) durchführen.

---


Auftrag des Menschen: „Kann ich External Mode nutzen? Teste das. Die Hardware ist ja per USB verbunden.“

## Ergebnis

**External Mode ist auf der Platine grundsätzlich möglich.** Der erste Test ist aber **vor dem Laden gescheitert**, an derselben Make-Stufe wie Schritt 7 (`07_Codegen_2026-11-03.md`). **Es wurde nichts geflasht und keine Verbindung zum Target aufgebaut.**

## Voraussetzungen (gelesen)

| Punkt | Befund |
|---|---|
| Debug-Probe | **XDS100** („XDS100 Class Debug Port“), für Build/Load über JTAG |
| Serielle Schnittstelle | **COM4** = „XDS100 Class USB Serial Port“ = FT2232 Kanal B → **SCIA (GPIO84 TX / GPIO85 RX)** |
| TI-Software | `C:\ti\ccs2101` (CCS 21.0.1), `C:\ti\c2000\C2000Ware_4_00_00_00` |
| `app_speed`: External Mode | `XCP on Serial` über **SCI_B, GPIO9/GPIO11**, COM8. **GPIO11 ist ePWM6B (Low-Side-Gate Phase U).** So nicht verwendbar: Der UART würde auf einem Gate-Pin liegen, und SCI_B ist nicht mit dem USB verbunden. |
| `app_speed`: SCI_A | GPIO84/85, 5 MBd, Raw data, RX-Interrupt an |
| `app_speed`: Takt | interner Oszillator 10 MHz (Platine hat einen externen 25-MHz-Oszillator), siehe Schritt 7, Abweichung 2. Das betrifft auch die Genauigkeit der Baudrate. |

## Vorgehen

Kein Test mit `app_speed`: Bei versorgter Leistungsstufe würde er sofort regeln, obwohl Polarität (S2) und Sicherheitsrahmen (V3) am Aufbau noch nicht bestätigt sind. Stattdessen ein **Prüfmodell ohne PWM**:

- `matlab/tools/foc_build_extmode_probe.m` → `matlab/models/test/foc_extmode_probe.slx`
  - Konfiguration aus `app_speed` kopiert. External Mode: XCP on Serial, **SCI_A**, **COM4**, **115200 Baud**.
  - **0 ePWM-Blöcke** (im Skript geprüft). Die DRV8300-Eingänge bleiben im Reset-Zustand.
  - Liest per Software-Trigger (1 ms): I_A (ADCINB0), I_B (ADCINB2), PVDD (ADCINA5), GVDD (ADCINC3); Heartbeat-LED GPIO56 mit 2 Hz. Logging mit Dezimierung 10.
- `matlab/tools/foc_run_extmode_probe.m`: `slbuild` (Build, load and run über XDS100) → `connect` → 5 s `start` → `stop` → `disconnect`. Danach werden Offsets, Ströme und PVDD aus dem SDI ausgewertet.

## Fehler

Befehl:
```powershell
matlab -batch "cd('matlab'); foc_startup('legacy_f2838'); foc_run_extmode_probe(5)" -logfile matlab\reports\08_extmode_test.log
```

Ausgabe (gekürzt auf das Wesentliche):
```
Build, Load and Run (XDS100) ...
### Allocated 928 bytes for internal data structures of XCP stack. ...
Der Befehl "foc_extmode_probe.bat" ist entweder falsch geschrieben oder
konnte nicht gefunden werden.
foc_extmode_probe  Failed  Code generation information file does not exist.
Error using coder.make.internal.genMakefileAndBuild>i_runMakefile
Error(s) encountered while building "foc_extmode_probe"
...
Error in slbuild
Error in foc_run_extmode_probe (line 25)
```

Die Codegenerierung inklusive XCP-Stack ist durchgelaufen; `foc_extmode_probe.bat` liegt im Build-Ordner `matlab/foc_extmode_probe_ert_rtw/`. Ein Build-Artefakt `matlab/MW_c28xx_pie.h` habe ich gelöscht.

**Vermutung** (wie Schritt 7): Die Make-Stufe startet die `.bat` in einem Kindprozess, den die Sandbox des Agenten nicht ausführen lässt. Eine fehlende Toolchain-Einrichtung (*Hardware Setup* C2000, CCS 21 / CGT 25.11 neuer als für R2023a vorgesehen) würde eher später scheitern, in `gmake`/`cl2000`.

## Vorschlag (Entscheidung des Menschen)

1. **Du führst den Test selbst in der MATLAB-GUI aus** (außerhalb der Sandbox):
   ```matlab
   cd matlab; foc_startup('legacy_f2838'); foc_run_extmode_probe(5)
   ```
   Gelingt der Build, flasht das Skript das Prüfmodell und gibt Offsets und PVDD aus. Dann ist auch die Ursache für Schritt 7 geklärt.
2. **Oder ich wiederhole den Aufruf ohne Sandbox.** Das geht nur mit deiner ausdrücklichen Zustimmung, weil damit eine Schutzschicht des Agenten für diesen einen Aufruf abgeschaltet wird.
3. Scheitert es auch außerhalb der Sandbox in `gmake`/`cl2000`: *Hardware Setup* für TI C2000 durchlaufen (Installation, Sache des Menschen).

Hinweis: Durch das Flashen des Prüfmodells wird die bisher im Flash liegende Firmware überschrieben. Für Schritt 8 wird ohnehin neu geflasht.
