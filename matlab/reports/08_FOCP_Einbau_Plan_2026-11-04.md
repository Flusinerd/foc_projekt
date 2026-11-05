# FOCP in app_speed – Stand und Plan (2026-11-04, unterbrochen)

**Entscheidungen des Menschen:**
- Vorhandenen C-Code (`protocol/src`, `firmware/focp_target.c`) einbinden. Das weicht **bewusst von AGENTS.md ab** (handgeschriebener Firmware-C-Code, Betreuer-Freigabe fehlt).
- Poti ↔ FOC Studio: Ohne FOCP-Verkehr wie bisher (Poti). Der erste FOCP-Befehl schaltet um: Sollwert aus FOC Studio, gesperrt bis ENABLE, Heartbeat-Ausfall → Freemode.
- Tests zunächst minimal.

**Ermittelt (noch nichts umgesetzt):**
- Einfügestelle: Wurzel von `app_speed` (focLib bleibt unverändert), Takt Ts_speed (2 kHz, step0).
  - Speed_Ref: `Rate Transition5` → Speed Control, sfix32_En17.
  - Freemode_Sig: `Rate Transition2`, boolean.
  - Undervoltage `RT1`, hot_temp `RT3`, speedMode `RT4` (uint16), SafeState `RT13` (boolean, 0 = Freemode).
  - Drehzahl: `From2` (PU, sfix32_En17).
- Sollwert: N_ref = Speed_Ref · floor(N_base/3) · speedMode → Speed_Ref = rpm / (floor(N_base/3)·speedMode), auf [0, 1] begrenzt. N_base = 4000.
- CAN (driverlib C2000Ware 4.00, `can.h`):
  - Belegte Message-Objekte: 2, 3 (TX 0x110/0x111), 10, 16 (RX 0x101/0x102).
  - Plan: RX-FIFO in den Objekten 20–23, ID 0x201, Maske 0x7EF (CMD + PARAM_REQ, Knoten 1).
  - Lesen mit `CAN_readMessageWithID`, DLC aus `CAN_O_IF2MCTL`.
  - TX über die Objekte 24–31 im Wechsel, belegte Objekte per `CAN_getTxRequests` erkennen. Vor jedem Senden `CAN_setupMessageObject`, weil sich die ID ändert.
- Custom Code bisher leer.

**Geplante Umsetzung:**
1. `firmware/focp_c28x.c/.h`:
   - `focp_target_irq_save/restore` (`__disable_interrupts`).
   - `focp_c28x_step(uv, fm_can, hot, safe, mode, speed_pu, n_base, *active, *freemode, *ref)` mit verzögerter Initialisierung beim ersten Aufruf.
   - Im Schritt: RX auslesen, Befehle verarbeiten (ENABLE/DISABLE/ACK_FAULT), Heartbeat-Tick 1 ms, STATUS alle 100 ms, TELEM SPEED/SPEED_REF, TX-Queue leeren.
2. Skript `matlab/tools/foc_add_focp.m`:
   - MATLAB-Function `FOCP` mit `coder.ceval` nur für `coder.target('Rtw')`. In der Simulation ist `active = 0`, die Äquivalenz bleibt bitgleich.
   - Switch vor Speed Control in1 und OR-Block für Freemode.
   - CustomSource/CustomInclude relativ zu `matlab/`.
3. `foc_equivalence_p1a`, `foc_codegen`, dann Flashen nur mit Freigabe und Test mit FOC Studio.

**Hardwarestand:** Auf der Platine läuft die **LED-Testfirmware** (datasheet, Iq_max 0,1, Heartbeat-LED schaltet nicht um). Für den Normalbetrieb `foc_flash('app_speed','FREIGABE')` ohne `prep`.

**Offen außerdem:** Sollwert-Totzone/I-Reset gegen das Kriechen bei Poti 0 (S5-Befund), Baseline SW_ANF_05 neu aufnehmen.

## Umsetzung (2026-11-05)

| Datei | Inhalt |
|---|---|
| `firmware/focp_c28x.c/.h` (neu) | Anbindung an eCAN-A. RX-FIFO in den Objekten 20–23 (0x201/0x211, Maske 0x7EF), TX im Wechsel über 24–31. Interrupt-Sperre für die TX-Queue. Umschaltung Poti ↔ FOCP, ENABLE/DISABLE/ACK_FAULT, Heartbeat-Ausfall → Freemode + `FOCP_FAULT_COMM`, STATUS alle 100 ms (READY/RUN/FAULT; Unterspannung, Übertemperatur, SafeState, COMM), Telemetrie `speed`/`speed_ref`. Udc und Temperatur im STATUS vorerst 0. |
| `firmware/focp_target.c/.h` | Neu: `focp_target_rx_count()` (zählt angenommene FOCP-Nachrichten) |
| `CMakeLists.txt`, `host/tests/test_protocol.cpp` | `focp_c28x.c` wird für den PC mitgebaut. Minimaler Test `testC28xGlue`: Poti-Modus, Umschaltung, Sollwertskalierung (666,5 rpm → 0,5), ENABLE, Heartbeat-Ausfall → Freemode, ACK_FAULT, STATUS 0x231 → **bestanden** |
| `matlab/tools/foc_add_focp.m` (neu) | Subsystem `FOCP` auf der Wurzelebene von `app_speed`. Switch auf Speed_Ref (Speed Control In1), OR auf Freemode (In4), MATLAB Function mit `coder.ceval` nur für `coder.target('Rtw')`. Custom Code relativ zu `matlab/models` (`../../protocol/...`). |

Nachweise:
- `foc_add_focp`: vorher kein FOCP und kein Custom Code, nachher eingebaut (Logs `08_FOCP_add*.log`). Erster Lauf mit Pfaden relativ zu `matlab/` → Build-Fehler „does not exist in … matlab\models“. Korrigiert auf `../../`.
- `foc_equivalence_p1a` (legacy): 12 Signale, Abweichung **0**. Die Simulation bleibt unverändert.
- `foc_codegen` (datasheet): erfolgreich, **kein Laden**. focp.c, focp_dev.c, focp_target.c und focp_c28x.c sind ohne Warnung kompiliert. Aufruf `focp_c28x_step(...)` im 2-kHz-Schritt.

Noch nicht auf der Hardware getestet. CAN-Konfiguration für FOC Studio: CANable, 1 Mbit/s, Knoten 1.

## Hardwaretest (2026-11-05, Freigabe des Menschen)

- `foc_flash` (datasheet). Erster Versuch: Make-Abbruch `focp.d:30: *** missing separator` bei inkrementellem Build, nichts geladen. Das Blockset schreibt die per `#include` eingebundenen `.def`-Tabellen ohne Ziel in die `.d`-Datei. Abhilfe: `tools/foc_clean_custom_deps.m` löscht `focp*.d/.obj` vor jedem Build und wird von `foc_codegen` und `foc_flash` aufgerufen. Zweiter Versuch: geladen, Platine neu gestartet.
- CANable (`gsusb_can_check.ps1`, 1 Mbit/s): GET_INFO → **INFO 0x261** nach 1 ms (`01 00 01 01 2A 00 16`: Protokoll 1, FW 0.1, Platine 1, 42 Parameter, 22 Signale). **STATUS 0x231** alle 100 ms (`02 02 …`: READY, Drehzahlmodus, keine Fehler). 0x110/0x111 unverändert. Ohne empfangenden Teilnehmer staut sich die TX-Queue (keine Bestätigung). Nach dem Anschluss des Adapters kommen die Rahmen gesammelt; danach wieder alle 100 ms.
- FOC Studio (`build/host/studio/focstudio.exe`): Qt-DLLs fehlten im Build-Ordner → `windeployqt`. Verbunden, Zustand **„Bereit“**. **Motorsteuerung aus FOC Studio funktioniert** (Rückmeldung des Menschen).
- Offen:
  - Der Knopf „Verbindung testen“ funktioniert nicht. Vermutung: Er öffnet den Adapter ein zweites Mal, während die Hauptverbindung besteht.
  - Udc und Temperatur werden im STATUS als 0 gemeldet.
  - Telemetrie nur für Drehzahl und Drehzahl-Soll.
  - Parameter aus FOC Studio wirken nicht auf die Regelung.

## Erweiterung: Udc und Telemetrie (2026-11-05)

`tools/foc_focp_telemetry.m` (Logs `08_FOCP_telemetry*.log`):
- `focLib/Input Controller`: neuer Ausgang 7 **Udc**. Das ist der Ausgang von Variant Source1, **uint16 in V, also 1 V Auflösung**; dieselbe Größe nutzen Unterspannung und CAN 0x110.
- `focLib/Torque Controller`: neuer Ausgang 3 **Idq_PU**, aus der Park-Transformation über den neuen Ausgang 2 von `FOC`.
- `app_speed`:
  - Rate Transitions auf Ts_speed (Kopien von RT1/RT7)
  - Data Store Read `GlobalHallState`
  - FOCP-Eingänge 8–11: udc, idq_ref (aus Speed Control, über einen **Unit Delay**), idq, hall
  - Konstante `PU_System.I_base`
- `firmware/focp_c28x.c`: STATUS mit Udc. Telemetrie: speed, speed_ref, udc, id, iq, id_ref, iq_ref (in A), hall_state.

Verlauf:
1. Erster Lauf brach ab: Die neuen Ausgänge der Bibliothek waren im geladenen `app_speed` noch nicht sichtbar. `focLib` war da schon gespeichert (6→7, 1→2, 2→3 Ausgänge). Das Skript lädt `app_speed` jetzt nach dem Speichern der Bibliothek neu und ist wiederholbar.
2. Danach meldeten Simulation und Codegenerierung eine **algebraische Schleife** über `app_speed/FOCP/ref_fix` bzw. `Freemode_Or`: FOCP liest den Ausgang von Speed Control und speist dessen Eingänge. Behoben mit einem Unit Delay auf idq_ref (`addRefDelay`).

Nachweise:
- PC-Test `testC28xGlue` (STATUS mit Udc = 24 V → `60 09`): **bestanden**.
- `foc_equivalence_p1a` (legacy): Abweichung **0**.
- MIL (datasheet): 3/4; SW_ANF_05 ist weiterhin nur die Baseline aus Iq_max (verify bestanden).
- `foc_codegen`: erfolgreich, keine neuen Warnungen.

## Hardwaretest Telemetrie (2026-11-05)

Geflasht (`08_FOCP_tel_flash.log`). FOC Studio zeigt Drehzahl, i_q/i_q-Soll und Hall-Zustand.

**Zeitachse um Faktor 10 falsch:** FOC Studio rechnet mit `inv.f_pwm` = 20 kHz / Teiler. Die Firmware tastet aber im Drehzahltakt (2 kHz) ab. Teiler 1 bedeutet also real 2 kHz, angezeigt werden 20 kHz. Abhilfe offen: Abtastung in den 20-kHz-Interrupt verlegen (empfohlen) oder den Takt melden.

Auswertung `focstudio_scope_20261105_120812.csv` (Teiler 1, real 2 kHz, 81 s, Sollwert 1000 rpm, PVDD 24 V, Leerlauf):

| Größe | Ergebnis |
|---|---|
| Drehzahl stationär | 1000,0 rpm, σ = 13,9 rpm |
| i_q / i_q-Soll stationär | 0,827 / 0,828 A, σ(i_q) = 0,25 A (Messrauschen) |
| Hall-Folge | 1→5→4→6→2→3→1, je 5549 Übergänge, **keine** ungültigen oder übersprungenen Zustände; 2,5 ms je Sektor = 1000 rpm |
| Anlauf i_q-Soll | springt auf 5,3–5,6 A = Iq_max (0,1 PU) ✔ |
| **Anlauf i_q** | **−27,8 → −30,7 → −22 → −14 → −8 → −4 → −2 A** über 7 Abtastwerte (3,5 ms), dann positiv. Gleichzeitig Hall 3→2→6→4 (**rückwärts**) und gemessene Drehzahl −238 rpm |

**Befund: Windup der Stromregler im Freemode.** Im Freemode ist die PWM aus. Die Stromregler rechnen aber weiter, und ihr I-Anteil integriert den Messoffset bis an die Spannungsgrenze (±1 PU = 13,9 V; vgl. S3: 90/10-Tastgrad ohne Motor). Beim Freigeben liegt diese Spannung sofort an. Ergebnis: ein Stromstoß von ≈ −31 A gegen die Sollrichtung, bis der I-Anteil nach ≈ 3 ms abgebaut ist. Ohne Last unkritisch, mit Last oder höherer Spannung nicht.
**Vorschlag:** I-Anteile der Stromregler (d/q) im Freemode zurücksetzen. Das geht wie beim Drehzahlregler (`foc_speed_pi_reset`) und lässt sich per Parameter schalten: legacy aus (bitgleich), datasheet an.
