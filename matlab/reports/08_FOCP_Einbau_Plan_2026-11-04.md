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
