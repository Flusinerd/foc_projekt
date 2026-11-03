# CarpeDiem – Überblick über die bisherige Firmware (Stand: Masterarbeit B. Akgül, Dez 2023)

> Einarbeitung aus `CarpeDiem_SW.zip` und der Masterarbeit „Modellbasierte Entwicklung und Verifikation mit MATLAB/Simulink eines modularen Frequenzumrichters“ (B. Akgül, HS Bochum / Smart Mechatronics GmbH, 18.12.2023, Betreuer Prof. A. Bergmann, C. Peters). Es ist die **3. Generation** des Instituts-Frequenzumrichters; die Hardware entstand parallel in einer zweiten Masterarbeit [4] (nicht im Archiv).
> Es gibt **keinen handgeschriebenen C-Code** – die Firmware ist vollständig modellbasiert (Simulink + Embedded Coder, MATLAB R2023a) und wird aus `impmodel.slx` generiert.

## 1. Inhalt des Archivs

| Ordner | Inhalt |
|---|---|
| `models/impmodel.slx` | **Hauptmodell** Motorcontroller für **TI F2838x** (ControlCard TMDSCNCD28388D, "Custom HW"-Variante), inkl. 4 Test-Harnesses |
| `models/params_f2838.mat` | Parameter (Motor, Inverter, PU-System, PI-Gains, Abtastzeiten) für F2838x |
| `models/F280049C/impmodel_f280049c_origin.slx` + `params_f280049c.mat` | Ältere Variante für **LaunchPad F280049C** (Vorgängerstand, abgeleitet von MathWorks-Beispiel "FOC of PMSM Using Hall Sensor") |
| `models/TCP_Server.slx` | Läuft auf dem **CM-Core (ARM Cortex-M4)** des F28388D: liest IPC-Kanäle, packt Bytes, sendet per TCP (Ethernet-Logging / "External Mode"-Ersatz) |
| `models/TCP_Client.slx` | Host-PC-Modell (Simulink Real-Time TCP Client) zum Empfangen/Plotten |
| `models/Serial_host_model.slx` | Host-Modell für serielle Datenübertragung (ältere Variante über SCI) |
| `reqs/` | Lastenheft v0.2 (ANF_01…ANF_21), Systemmodellanforderungen v0.1 (SW_ANF_01…09), Use Cases, Umfeldmodell |
| `testing/` | Simulink-Test-Datei (`.mldatx`), Requirements-Datei (`.slreqx`), Testreport (4/4 Tests bestanden) |
| `canMessages/`, `ipcMessages/`, `ethernetConfigs/` | Schnittstellentabellen CAN, IPC, Ethernet-Pinmapping |
| `milestone/` | Meilensteinplan KW27–KW52/2023 |
| `Quellen/` | Datenblätter (F2838x TRM + Datasheet, DRV8300, ECI 63.60, TMP23x) und ca. 45 Web-/Literaturquellen |

## 2. Hardware-Setup (aus Modell + Parametern)

- **MCU:** TMS320F28388D auf ControlCard, 200 MHz C28x-CPU1 + Cortex-M4 "Connectivity Manager" (Ethernet). Params-Struktur `target_f2838`.
- **Gate-Treiber / Leistungsstufe:** TI **DRV8300** (BoostXL-DRV8300-Parameter in `inverter_DRV8300`), V_dc = 24 V, Shunt 3 mΩ, Stromsensor 3 mV/A, ADC-Offset ≈ 2047 Counts (1,65 V).
- **Motor:** ebm-papst **ECI 63.60, 24 V**, 4 Polpaare, 4000 min⁻¹ Nenn, 17,6 A Nenn, 0,88 Nm, Hall-Sensoren A/B/C (Struktur `pmsm_ECI6360`).
- **Per-Unit-Basis:** V_base = 20,78 V, I_base = 17,6 A, N_base = 4000 min⁻¹, T_base = 4,01 Nm.
- **Peripherie-Belegung (F2838x-Modell):**
  - ePWM6/7/8 → Phasen U/V/W, 20 kHz, Up-Down-Zähler, Periode 5000 Ticks (`PWM_Counter_Period`), CMPA/CMPB vom Modell (A- und B-Kanal getrennt = High-/Low-Side).
  - ADC-B SOC0/SOC1 (ADCIN0/ADCIN2) → Phasenströme Ia/Ib, getriggert durch **ePWM6_ADCSOCA**; ADCINT1 → Hardware-Interrupt, der den Torque-Controller (FOC) aufruft.
  - ADC-A ADCIN5 → Batteriespannung (Spannungsteiler 100,61 kΩ / 3,01 kΩ), ADC-C ADCIN3 → GVDD (Gate-Treiber-Versorgung), ADC-A ADCIN0 → Speed-Sollwert (Poti/Pedal), ADC-C ADCIN2 → MOSFET-Temperatur (TMP23x, 10 mV/K, 500 mV Offset).
  - eCAP1/2/3 → Hall A/B/C (GPIO 24/25/26, beide Flanken, Interrupt je Hall-Wechsel).
  - GPIO 30 → SafeState-Eingang (Taster), GPIO 56 → Heartbeat-LED (0,5 s Toggle).
  - eCAN-A: Rx ID 0x101 Freemode (Mailbox 9), Rx ID 0x102 SpeedMode (Mailbox 15 im Modell / 10 in Tabelle), Tx 0x110 BatteryVoltage, Tx 0x111 GVDD – alle 200 ms, uint16.
  - IPC CPU1 → CM: Kanäle 0 Position, 1 Speed, 2 HallStates, 3–5 Hall A/B/C, 6 SpeedRef, 7 temperatureMOS (fixdt(1,32,17) bzw. uint32).
  - Ethernet MII: Custom-Board-Pinmapping weicht vom ControlCard-Default ab (siehe `config_CC_Custom.xlsx`, z. B. TX_EN GPIO45 statt 118).

## 3. Software-Architektur (`impmodel.slx`)

Alle Subsysteme sind über **Variant Source/Sink `(sim)` / `(codegen)`** doppelt ausgeführt: Simulation nutzt ein Streckenmodell (Surface Mount PMSM + Average-Value Inverter), Codegenerierung nutzt die TI-Treiberblöcke.

```
impmodel
├─ System Initialize
├─ ADC Offset Function        – Stromsensor-Offsetkalibrierung (8 Samples Mittelwert, Plausibilität 1500…2500), sonst Default 2047
├─ Code Generation            – Hardware-Interrupts: ADCB1 (Prio 30) → Torque Controller; eCAP1/2/3 (Prio 25) → Hall-ISRs
├─ Hall Sensor A / B / C      – eCAP-ISR: Hall-Zustand lesen (GPIO24-26), Hall Validity, Datastores GlobalHallState/-Direction/-SpeedCount/-SpeedValidity
├─ IPC Write for eCAP INTs    – schreibt Hall-Daten per IPC an den CM-Core
├─ Input Controller           – Speed_Ref (ADC → 0…1), Undervoltage_Sig, Freemode_Sig (CAN), hot_temp, speedMode (CAN, 1…3), SafeState (GPIO); CAN-Tx Batterie/GVDD
├─ Speed Control  (Ts_speed = 0,5 ms)
│   ├─ Digital_Freemode_Check    – Freemode = CAN-Freemode OR Undervoltage OR hot_temp     (SW_ANF_01/07/08)
│   ├─ Mechanical_Freemode_Check – SafeState-Pin überschreibt → Freemode erzwungen        (SW_ANF_03)
│   ├─ Speed calculator          – N_ref = Speed_Ref · floor(N_base/3) · speedMode → PU  (SW_ANF_04)
│   └─ PI_Controller_Speed       – PI (Kp_speed, Ki_speed_texas·Ts), Sättigung [-0,5; 1], Fehler = 0 bei Freemode  (SW_ANF_05)
│       → IdqRef_PU = [Id_ref = 0, Iq_ref]
└─ Torque Controller (getriggert durch ADC-Interrupt, Ts = 50 µs = PWM-Periode)
    ├─ Iab_Meas_ADC + Input Scaling   – ADC-Counts − Offset → PU-Ströme; Hall → Position/Speed (Hall Speed and Position, 1. Ordnung, Mech→El. Offset 0,38)
    ├─ FOC                            – Clarke → Park → PI_Id / PI_Iq (anti-windup, Reset bei Freemode) → DQ-Limiter (0,95) → Inverse Park → SVM (PWM Reference Generator)
    ├─ Scaling DutyCycles             – 0,5 − 0,5·x (Invertierung/Verschiebung für Up-Down-Zähler)
    ├─ PWM1/2/3                       – bei Freemode: High-Side-Vergleichswert = 0, Low-Side = Konstante (Default 1) → High-Side offen  (SW_ANF_01)
    └─ Output                         – ×PWM_Counter_Period → ePWM6/7/8 CMPA/CMPB
```

**Regelstruktur:** klassische Kaskade – äußerer Drehzahl-PI (0,5 ms) liefert Iq_ref, innere Strom-PIs (50 µs, Id_ref = 0, keine Feldschwächung aktiv, obwohl `Kp_fwc/Ki_fwc` berechnet sind) liefern Vd/Vq → SVM. Datentypen überwiegend `fixdt(1,32,17)` bzw. `single`; der Regler läuft in Per-Unit.

**Sicherheitslogik:** „Freemode" (Fahren ohne Unterstützung) ist die Rückfallebene für Unterspannung, Übertemperatur, CAN-Befehl und SafeState-Taster. Übertemperatur-Freemode ist laut SW_ANF_08 bis Neustart latchend.

## 4. Verifikation

Testreport 28.11.2023, Simulink Test, 4 Harnesses, alle **bestanden**:

| Test | Subsystem | Anforderung |
|---|---|---|
| Freemode_Undervoltage_Temperature | Digital_Freemode_Check | SW_ANF_01/07/08 |
| MaxVelocity | Speed calculator | SW_ANF_04 |
| SpeedController | PI_Controller_Speed | SW_ANF_05 |
| SafeState | Mechanical_Freemode_Check | SW_ANF_03 |

Nicht modellbasiert getestet: SW_ANF_02 (DriveModes), SW_ANF_06 (TorqueController/FOC), SW_ANF_09 (Hall). Coverage: Decision 92–100 % in den getesteten Blöcken.

## 5. Erkenntnisse aus der Masterarbeit (Kap. 5–9)

**Toolchain:** MATLAB R2023a, C2000 Microcontroller Blockset 1.0, Embedded Coder 7.10, Motor Control Blockset 2.0, SoC Blockset 1.8, Simulink Test 3.8, Coverage 5.6, Requirements Toolbox 2.2, Simulink Real-Time 8.2 (für den TCP-Client), Variant Manager.

**Design-Entscheidungen, die im Modell nicht offensichtlich sind:**
- PI-Koeffizienten wurden nicht händisch getunt, sondern mit `PI_params = mcb.internal.SetControllerParameters(pmsm_ECI6360, inverter_DRV8300, PU_System, T_pwm, Ts, Ts_speed)` berechnet. Die Motorparameter (Fluss, Ld/Lq) waren **nicht gemessen**, sondern aus Datenblatt und Vorgängerprojekt [2] übernommen – der Autor nennt das selbst als Schwachstelle.
- Hall-Offset (0,38 im Block) wurde **experimentell angenähert**, nicht per Kalibriermodell bestimmt.
- Speed-PI-Untergrenze −0,5 = bewusst „leichtes Bremsen“; 0 = Ausrollen, −1 = aktives Bremsen. Die Fehler-Nullung bei Freemode ist nötig, weil sonst der Ruhestrom im Stillstand stark anstieg (Ursache nicht geklärt).
- Freemode-PWM: Konstanten WA = 0 (High-Side), WB = 1 (Low-Side) – weil der **DRV8300 die INLx-Eingänge invertiert**, ergibt das 0 % an beiden Gates, d. h. alle MOSFETs offen. Die „25 % Low-Side“ aus SW_ANF_01/03 ist damit obsolet. Die Negation in „Scaling DutyCycles“ (0,5 − 0,5·x) kompensiert die unterschiedliche positive Drehrichtung zwischen TI und Simulink.
- PWM center-aligned, ePWMxA und ePWMxB bekommen identische Signale **ohne Totzeit** (Totzeit erzeugt der DRV8300). EPWMCLKDIV muss auf SYSCLKOUT/1 stehen, damit Periode 5000 → 20 kHz.
- ADC-SOC wird vom ePWM6 so getriggert, dass die Low-Side leitet (Low-Side-Shunts); Acquisition Window = 15 Takte = 75 ns (Minimum laut TI-Forum).
- Interrupts: ADCB1 Prio 30 preemptable, eCAP1–3 Prio 25 non-preemptable (sonst falsche Drehzahl). ISR-Laufzeiten gemessen: Torque-Controller (ADC-ISR) ≈ 13,2 µs von 50 µs, eCAP-ISR ≈ 3 µs.
- Unterspannungsschwelle 17 V (real ≈ 16,5 V).
- GPIO 56–61 und GPIO 30 mit internem Pull-Up im `System Initialize`-Block (direkte Registerzugriffe `GpioCtrlRegs.GPAPUD`).

**Ethernet / „External Mode über TCP“ (Kap. 5.7):** Ethernet ist nur über den CM-Core erreichbar → CPU1 schreibt Signale per IPC, CM-Modell `TCP_Server.slx` sendet per TCP (Server 192.168.1.8, Host-Client 192.168.1.10, Daten als uint8/uint32 gepackt). GPIO 57-LED blinkt bei funktionierender Übertragung. Der eigentliche Simulink-External-Mode wurde nicht benutzt.

**⚠ Nicht im Archiv enthalten – gepatchte MathWorks-Basissoftware:** Weil die Custom-Hardware ein anderes MII-Pinmapping hat als die ControlCard, wurden Dateien des C2000-Blocksets geändert (`c2838xBoard_Realtime_Support.c` → `initSetGPIOIPC`/`setGPIOForEthernet`, `MW_c28xGPIO.c` → neue Funktion `getCarPediemGPIONumber`, Hook in `GPIO_SetupPinMux`/`GPIO_ReadPin`), eingeschlossen in `#ifdef CARPEDIEM_PIN_CORRECTION`. Das Makro wird als Custom-Code-Define in den Hardware Settings von **impmodel und TCP_Server** gesetzt. Ohne diese Patches läuft das Ethernet auf der Custom-Hardware nicht; die Tabelle `config_CC_Custom.xlsx` ist die Spezifikation dafür. → Patches beim Autor/Institut anfordern oder nach der Tabelle neu erstellen.

**Verifikationsergebnisse auf der Zielhardware (Kap. 7):**
- Hall-Signale waren anfangs stark verrauscht (fehlende Level-Shifter) → Fehl-Interrupts, Position friert ein, Drehzahl springt auf 0. Nach Hardware-Änderungen [4] deutlich besser; in speedMode 3 bleiben vereinzelte Aussetzer (Ursache offen). Das F280049C-LaunchPad (mit Level-Shifter) lief sauber – deshalb existiert die zweite Modellvariante.
- Volle Drehzahl war zunächst nicht erreichbar, weil die ADC-Referenz (3,3 V) gleichzeitig Temperatursensor und Hall-Sensoren versorgt → Poti-Spannung < 3,3 V. Abhilfe: Rundung + Filter im Speed-Ref-Pfad.
- Freemode über Unterspannung, CAN und SafeState-Taster auf dem Oszilloskop verifiziert; Übertemperatur nur über die Temperaturmessung (±2 °C) verifiziert.
- SW_ANF_02 (DriveModes) **nicht implementiert**; SW_ANF_06/09 nicht modellbasiert getestet, nur auf dem Target gezeigt.

**Ausblick des Autors:** Multi-Core (Torque Controller auf CPU2), Motorparameter messen, Parameter per Skript statt .mat, eigenes Streckenmodell, IPC-Übertragung nur bei aktivem Ethernet, ggf. UDP.

## 6. Offene Punkte / Auffälligkeiten (eigene Analyse)

1. **Motorparameter in `params_f2838.mat` passen nicht zum Datenblatt:** Rs = 0,009 Ω, Ld = Lq = 1,53e-7 H, FluxPM = 6,9e-4. Datenblatt ECI 63.60 (24 V): R_V = 0,045 Ω, L_V = 76,5 µH. Die F280049C-Parameter (Rs = 0,045, L = 7,65e-5, FluxPM = 0,038) stimmen mit dem Datenblatt überein. Entsprechend sind die Strom-PI-Gains im F2838-Satz um Faktor 500 kleiner (Kp_i = 0,0013 statt 0,648). Laut Arbeit liefen die berechneten Koeffizienten „verifiziert“ auf dem Target – trotzdem vor Wiederverwendung klären, welcher Satz real benutzt wurde, und die Parameter messen (Ausblick der Arbeit).
2. **Hall-Offset doppelt gepflegt:** `pmsm_ECI6360.PositionOffset = 0,411`, im Block „Mechanical to Electrical Position“ hart 0,38 (experimentell). Kalibrierung per MathWorks-Beispiel „Hall Offset Calibration“ steht aus.
3. **Interrupt-Blöcke:** Sichtbare Maskenparameter zeigen `ADCA1_INT`/F2837x; die reale Zuordnung (ADCB1, eCAP1–3 gemäß Tab. 5.4 der Arbeit) liegt im Hardware-Mapping-Konfigurator / Block-UserData. Beim Portieren neu setzen.
4. **CAN-Mailbox-Inkonsistenz:** SpeedMode laut Tabelle Mailbox 10, im Modell Mailbox 15.
5. Anforderungstexte sind teilweise überholt (SW_ANF_01/03: „25 % Low-Side“; SW_ANF_05: Sättigung ±1; SW_ANF_06: Stellgröße 0…1 statt DQ-Limiter 0,95). Bei einer Weiterführung die Systemmodellanforderungen auf den Ist-Stand bringen.
6. `Saturation`-Block hinter dem Speed-PI hat Upper = Lower = 0 – vermutlich Relikt, prüfen.
7. Hall-Aussetzer in speedMode 3 nicht geklärt; Glitch-Filter-Konstante hängt von CPU-Takt, N_base und Polpaarzahl ab.
8. Kein Bremsen außer −0,5 PU, keine Feldschwächung (Kp_fwc/Ki_fwc berechnet, aber ungenutzt), Id_ref = 0.
9. Umrechnung Motordrehzahl → Fahrzeuggeschwindigkeit (25 km/h, ANF_02) fehlt; speedMode begrenzt nur auf 1333/2666/3999 min⁻¹.
10. IPC-Daten werden permanent in der Dauerschleife gesendet, auch ohne Ethernet-Verbindung.

## 7. Begriffe / Signalnamen (Glossar)

| Signal | Bedeutung |
|---|---|
| `Freemode_Sig` | 1 = Antrieb aus (High-Side offen), Fahren ohne Unterstützung |
| `SafeState` | GPIO-Taster; 0 = sicherer Zustand verlassen → Freemode |
| `speedMode` | 1…3, Unterstützungsstufe, skaliert N_ref (max. 1333/2666/3999 min⁻¹) |
| `Speed_Ref` | Sollwert 0…1 aus ADC (Pedal/Poti) |
| `Undervoltage_Sig`, `hot_temp` | Batterie-Unterspannung bzw. MOSFET-Übertemperatur |
| `pos_PU`, `speed_PU` | Rotorlage (elektrisch, 0…1) und Drehzahl (PU, Basis 4000 min⁻¹) aus Hall-Dekoder |
| `IdqRef_PU` | [Id_ref, Iq_ref] in PU, Ausgang Speed Control |
| `Vabc in PU` / `Duty Cycle` | SVM-Ausgang bzw. skalierte Vergleichswerte für ePWM |

## 8. Nachtrag (P0-Analyse, Nov 2026)

Die genaue Analyse der Reglerblöcke hat weitere Befunde ergeben. Details und Korrekturen stehen in `claude/P0_P1_Anleitung.md`:
- **B1** I-Anteil des Drehzahlreglers um Größenordnungen zu klein (`Ki_speed_texas*Ts_speed`, `UseKiTs = on`) → praktisch P-Regler.
- **B2** Stromregler: `Kp − 0,0002` von Hand nachgestellt, I-Anteil wie B1, Ausgang auf [0, 1] begrenzt (keine negative u_d/u_q).
- **B3** Korrigierte Motordaten: Ψ = 5,9 mVs (aus Ke und Leerlaufdrehzahl), R/L im Datenblatt sehr wahrscheinlich Klemmenwerte → Strang 22,5 mΩ / 38 µH. Die frühere Angabe Ψ ≈ 8,3 mVs in diesem Dokument war falsch.
- **B4** Strom-Skalierung im Codegen-Pfad: 1 PU = Strom bei 1,65 V Hub am Strommessverstärker, nicht I_base.
- **B5** `dataType` fehlt in der `.mat`.
