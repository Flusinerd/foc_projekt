# External Mode – Machbarkeit und erster Test (2026-11-03) – ANGEHALTEN (Linker, Prüfmodell)

## Entscheidung (2026-11-03): External Mode/UART zurückgestellt

Der Mensch hat entschieden, die serielle Schnittstelle vorerst nicht weiter zu verfolgen. Für Schritt 8 genügen JTAG (Flashen/Starten), Messgeräte (S1–S3) und der CCS-Debugger (S4/S5). Offen bleibt die UART-Strecke MCU ↔ FT2232 Kanal B (Messvorschlag in Nachtrag 4, `Erratas.xlsx` anfragen). Die Prüfmodelle `foc_extmode_probe` und `foc_uart_probe` bleiben für einen späteren Test im Repo. Im Flash liegt derzeit `foc_uart_probe` (keine PWM).

## Nachtrag 4: UART-Grundtest ohne XCP – kein Byte auf COM4, Firmware nachweislich richtig

**Testmodell:** `foc_build_uart_probe` → `matlab/models/test/foc_uart_probe.slx`. Ein uint8-Zähler alle 10 ms geht über SCI_A als Rahmen `'S' <n> 'E'` hinaus, bei 115207 Bd. Kein External Mode, keine PWM, Heartbeat-LED GPIO56.

| Lauf | SCI_A-Pins | Neustart nach Flash | COM4 |
|---|---|---|---|
| 1 | GPIO84 TX / GPIO85 RX (Hardware-Überblick) | nein | 0 Byte in 1,5 s |
| 2 | **GPIO42 TX / GPIO43 RX** (Schaltplan MA Darici, Abb. 5.8, siehe unten) | nein | 0 Byte |
| 3 | GPIO42/43 | **ja, LED blinkt** | 0 Byte bei 115200, 57600, 230400, 9600 Bd |

**Schaltplan (vom Menschen geliefert, Abb. 5.8 MA Darici, FT2232HL):** BDBUS0 (Pin 38, FT-TXD, Beschriftung „SCI-RX“) → Netz **USB_P**; BDBUS1 (Pin 39, FT-RXD, „SCI-TX“) → Netz **USB_N**. Die Netznamen deuten auf die USB-Pins des F2838x (USB0DP = GPIO43, USB0DM = GPIO42); beide lassen sich als SCIA_RX/TX schalten (wie beim LaunchPad F28379D). Wohin USB_P/USB_N auf der MCU-Seite gehen, zeigt der Ausschnitt **nicht**.

**Generierter Code (`foc_uart_probe_ert_rtw/MW_c28xx_csl.c`, `MW_c28xx_board.c`):** `GPBGMUX1/GPBMUX1.GPIO42 = 3` („Configure GPIO42 as SCITXDA“), `GPBAMSEL.GPIO42/43 = 0` (digital), `SCILBAUD = 216` (200 MHz → 115207 Bd), `SCICTL1.TXENA = 1`. Senden über `SciaRegs.SCITXBUF`. **Die Firmwareseite ist korrekt.**

**Bewertung:** Das Programm läuft (LED), SCIA sendet auf GPIO42 (bzw. vorher 84), auf COM4 kommt nichts an. Die Ursache liegt auf der **Hardwareseite**: Verbindung MCU ↔ FT2232 Kanal B (Netze USB_P/USB_N, eventuell unbestückte 0-Ω-Brücken, Änderungen laut `Erratas.xlsx`) oder Konfiguration des FT2232-Kanals B im EEPROM (XDS100-Firmware).

**Messvorschlag (Mensch), während der UART-Test läuft (im Flash):**
1. Oszilloskop an **GPIO42** (MCU-Pin): Sieht man 115-kBd-Bursts alle 10 ms? Wenn nein, ist der Pin nicht verbunden bzw. ein anderer Pin aus dem Gehäuse.
2. Oszilloskop an **FT2232 Pin 39 (BDBUS1)**: Kommen die Bursts dort an? Wenn ja und COM4 bleibt leer, liegt es am FT2232-Kanal B (EEPROM/Treiber). Wenn nein, ist die Leiterbahn zwischen MCU und FT2232 unterbrochen oder geht woanders hin.
3. Netz USB_P/USB_N im vollständigen Schaltplan (`CarPediem_Motorcontroller_schematic.pdf`) nachverfolgen; `Erratas.xlsx` beim Institut anfragen.

**Bis dahin:** Beobachtung auf der Hardware über den **CCS-Debugger (JTAG)**, der zuverlässig funktioniert.

## Nachtrag 3: nach Power-Cycle – Target antwortet nicht auf XCP über COM4

- Der Mensch hat die Steuerplatine aus- und eingeschaltet; die **Heartbeat-LED (GPIO56) blinkt** mit 2 Hz. Das Prüfmodell läuft also aus dem Flash.
- `foc_run_extmode_probe(5, false)`: `connect` ohne Fehler, Status bleibt `stopped`, keine Daten.
- **Direkter Test der seriellen Strecke ohne Simulink:** Ein XCP-CONNECT-Rahmen (Länge, Zähler, `FF 00`, Prüfsumme; Format aus `xcp_frame_serial.c`) wurde in beiden Byte-Reihenfolgen über COM4 gesendet, bei 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1M, 2M, 3M und 5M Baud. **Bei keiner Rate kam ein einziges Byte zurück.**
- **Bewertung:** Ein Taktfehler würde bei einer der Raten wenigstens fehlerhafte Bytes liefern. Daher wahrscheinlicher:
  1. Die UART-Strecke FT2232 Kanal B ↔ SCIA (GPIO84 TX / GPIO85 RX) ist auf dieser Platine nicht durchverbunden oder anders verdrahtet. Laut `docs/Hardware-Ueberblick_Frequenzumrichter.md` enthält `Erratas.xlsx` Änderungen an der FT2232HL-Beschaltung, die Datei liegt nicht vor.
  2. SCIA wird im Prüfmodell nicht initialisiert oder ist auf andere Pins gemultiplext. Zu prüfen im generierten Code: `GPAMUX/GPCMUX` für GPIO84/85, `SciaRegs`.
- **Was funktioniert:** JTAG über den XDS100. Flashen und Starten gehen zuverlässig. Damit wäre ein **CCS-Debugger (Expressions-Fenster)** sofort als Beobachtungsweg nutzbar, unabhängig vom UART.

**Vorschlag:**
a) Mensch: an GPIO84 (TX) und GPIO85 (RX) bzw. an den FT2232-Pins mit dem Oszilloskop prüfen, ob beim Senden von COM4 aus ein Signal an GPIO85 ankommt. `Erratas.xlsx` beim Institut anfragen.
b) Agent (nach Freigabe): UART-Grundtest mit einem Mini-Modell, das über SCI_A zyklisch ein festes Byte sendet und auf COM4 mitgelesen wird. Das trennt die Hardwarestrecke von XCP.
c) Für Schritt 8 als Beobachtungsweg zunächst den CCS-Debugger über JTAG verwenden.

## Nachtrag 2: Prüfmodell v2 gebaut und geflasht, External-Mode-Verbindung liefert noch keine Daten

- **Prüfmodell v2** (`foc_build_extmode_probe`, frisches Modell, Blöcke aus `c2838xlib`, Takt intern 10 MHz → 200 MHz, SCI_A GPIO84/85 115207 Bd, XCP on Serial, COM4, 0 ePWM). `MW_SetVREF` ist damit aufgelöst.
- **Linkerfehler `extmodeEvent/ParseArgs/Reset`:** Diese Funktionen stehen in `toolbox/coder/xcp/src/target/ext_mode/src/xcp_ext_mode.c`. Die Build-Info des C2000-Targets (R2023a) bindet die Datei nicht ein, obwohl `ert_main.c` sie aufruft. Abhilfe **nur im Prüfmodell**: `CustomSource = …/xcp_ext_mode.c`. Danach Build erfolgreich (`foc_extmode_probe.out`). Die Toolbox ist unverändert.
- **Flashen erfolgreich:** CCS-Ausgabe „Loading the program to the target … Program is running … LOAD & RUN DONE“ über den XDS100.
- **Verbindung:** `connect` meldet keinen Fehler, aber der Status bleibt `stopped` und es kommen keine Daten im SDI an. Das gilt auch beim zweiten, reinen Verbindungsversuch ohne Flashen (`foc_run_extmode_probe(5, false)`).
- **Hinweis aus dem Log:** „The clock backup and restore functionality is not supported with this CCS version. When using 'Boot from flash', perform a board power cycle after flashing to ensure that the program executes at the desired clock frequency.“ Nach dem Flashen läuft der Takt daher möglicherweise falsch, und die SCI-Baudrate passt nicht.
- **Nächster Schritt (Mensch):** Steuerplatine einmal aus- und einschalten und prüfen, ob die Heartbeat-LED (GPIO56) mit 2 Hz blinkt. Danach `foc_run_extmode_probe(5, false)`. Falls im Batch-Modus weiterhin nichts ankommt: einmal in der MATLAB-GUI das Prüfmodell öffnen und *Monitor & Tune → Connect* (die GUI zeigt Verbindungsfehler an).
- **Zustand der Hardware:** Im Flash liegt jetzt das Prüfmodell (keine PWM, nur ADC lesen, LED). Die frühere Firmware ist überschrieben.

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
