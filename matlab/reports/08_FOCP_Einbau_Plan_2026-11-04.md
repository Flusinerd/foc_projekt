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
