# Schritt 8 – Plan für die Hardware-Erstinbetriebnahme (Entwurf, 2026-11-03)

## Ergebnisse

| Stufe | Datum | Ergebnis |
|---|---|---|
| **S2 Strompolarität** | 2026-11-04 | **bestanden.** Logik über USB, PVDD aus, Netzteil 1 A: Minus an MOTA, Plus an PGND. ISENA (J1.18 gegen J1.4): 1,660 V ohne Strom → 1,630 V mit 1 A, also **−30 mV/A** (erwartet −29,9 mV/A). Polarität `invertingAmp = −1` bestätigt, CSA-Verstärkung 10 V/V bestätigt (1 PU = 56 A). Offset +10 mV (≈ 2023 Counts) liegt im Kalibrierfenster [1500, 2500]. Gemessen vom Menschen. |

Stand der Firmware für S1/S3 (seit 2026-11-04): `app_speed` mit externem 25-MHz-Oszillator (6fb3c62), DeviceID F28386D (3ace0f0), CAN-Mailboxen getrennt (d338ab0). CAN auf der Hardware mit Testfirmware fehlerfrei, UART nicht nutzbar (A8).

**Status: Vorbereitung. Nichts davon ist ausgeführt.** Jede Stufe mit Hardwarebezug braucht die ausdrückliche Freigabe des Menschen in derselben Sitzung (AGENTS.md, Regel 4). Messungen am Aufbau macht der Mensch. Der Agent bereitet Modelle und Code vor, wertet aus und flasht erst nach Freigabe.

Quellen: AGENTS.md §5 Schritt 8, `05_P1b_Diagnose_2026-11-03.md` (Polarität, ePWM, DRV8300DI), `05b_P1b1_2026-11-03.md`, `docs/Hardware-Ueberblick_Frequenzumrichter.md`, `docs/Firmware-Ueberblick_CarPediem.md`, SLVUBV6.

## 0. Voraussetzungen (vor jeder Stufe mit PVDD)

| Nr. | Punkt | Stand |
|---|---|---|
| V1 | Schritt 7: Codegenerierung ohne Fehler; Board, 25-MHz-Oszillator, 200 MHz bestätigt; im generierten Code `OUT_MODE`, Aktionsqualifizierer und ADC-SOC wie erwartet | läuft (parallele Sitzung) |
| V2 | A2: Untergrenze des Drehzahlreglers als Parameter, **0 für den Betrieb am Labornetzteil** (keine Rückspeisung) | vorbereitet (`foc_param_brake_limit.m`), Ausführung nach V1 |
| V3 | Sicherheitsrahmen bestätigt: Labornetzteil mit Strombegrenzung, Motor ohne Last, Welle frei, SafeState-Taster griffbereit, PVDD beobachten | Mensch |
| V4 | EVM: AREF-Jumper **J8 auf 1:2** (bidirektional, 1,65 V). Die Strom-PU-Basis 56 A setzt das voraus | Mensch prüft |
| V5 | Bekannt: Die eFuse (1,1 A) liegt **nur im Logikpfad**. Im Leistungspfad gibt es nur die 30-A-Sicherung des EVM, und der DRV8300 hat keine Überstromabschaltung. Der Schutz ist allein die **Strombegrenzung des Netzteils** | – |
| V6 | Beobachtungsweg festlegen (siehe Abschnitt 2) | Entscheidung Mensch |

## 1. Stufen

Für jede Stufe gilt: erst die Prüfungen ohne PVDD, dann PVDD langsam hochfahren. Bei jedem Abbruchkriterium sofort SafeState auslösen bzw. PVDD aus, protokollieren und nicht weitermachen.

### S1 – Logik allein (USB-C, kein PVDD), Firmware P1a/`legacy_f2838`
- **Aktion:** `app_speed` im Git-Stand `P1a` (365f7d0) mit `foc_startup('legacy_f2838')` bauen und flashen. Erst nach Freigabe.
- **Erwartet:** Heartbeat-LED (GPIO56) blinkt mit 0,5 s; nach dem Start Offset-Kalibrierung, Offsets in [1500, 2500] (`CtSensOffsetMin/Max`), Nennwert ≈ 2047.
- **Abbruch:** keine LED, Offsets außerhalb des Fensters (Hinweis auf J8, fehlende 3,3 V am CSA oder falschen ADC-Kanal).

### S2 – Strompolarität ohne Software (empfohlen vor jedem PWM-Betrieb)
Prüft die Polarität aus dem Schaltplan, ohne dass ein Gate schaltet.
- **Aufbau:** Logik über USB-C, Hauptversorgung (PVDD) **abgeklemmt**, Motor abgeklemmt. Ein zweites Labornetzteil auf **1 A Strombegrenzung, ≤ 2 V**: **Minus an MOTA (J5.3), Plus an PGND** des EVM.
- **Stromweg:** PGND → Shunt R49 (von unten nach oben) → Body-Diode Low-Side-FET Q2 → MOTA → Netzteil. Das entspricht einem **positiven Strangstrom** (aus dem Wechselrichter in Richtung Motor).
- **Erwartet:** ISENA (J1.18) gegen GND = 1,65 V − 0,0299 V/A · I. Bei 1 A also **≈ 1,620 V** (−30 mV). Ohne Strom 1,650 V. Für Phase B (MOTB, ISENB J1.16) und C (MOTC, ISENC J1.14) gleich.
- **Bewertung:** Spannung sinkt → `invertingAmp = −1` bestätigt, Simulation und Code passen (Diagnosebericht). Spannung steigt → **anhalten**: Dann ist der Code-Pfad mitgekoppelt, und P1a/P1b dürfen nicht geregelt laufen.
- **Abbruch:** Strom erreicht 1 A nicht bei ≤ 2 V (Stromweg anders als erwartet), Erwärmung.

### S3 – Gate-Muster, PVDD klein, Motor abgeklemmt
- **Aufbau:** PVDD = 12 V, Strombegrenzung **0,3 A** (Empfehlung SLVUBV6 für das erste Einschalten). Motor abgeklemmt. SafeState zunächst aktiv (Freemode).
- **Erwartet in Freemode:** GHx = GLx = 0 (WA = 0 → INH low, WB = 1 → INL high → GL aus, DRV8300DI). Stromaufnahme nur die Ruhestromaufnahme.
- **SafeState lösen, Sollwert (Poti) 0, speedMode 1:** Oszilloskop GHA und GLA (bzw. INHA/INLA an J2): **komplementär**, Totzeit ≥ 200 ns, 20 kHz, Duty ≈ 50 % bei Sollwert 0.
- **Abbruch:** GHA und GLA gleichzeitig high, Strom > 0,1 A ohne Motor, PVDD-Messung (CAN 0x110) weicht > 5 % ab.

### S4 – Erstlauf P1a (`legacy_f2838`), Motor angeschlossen, Welle frei
- **Aufbau:** PVDD 24 V, Strombegrenzung **2 A** (Leerlaufstrom laut Datenblatt 0,9 A), speedMode 1 (max. 1333 min⁻¹), Poti langsam hochdrehen.
- **Erwartet:** Der Motor dreht in Sollrichtung, Stromaufnahme < 1,5 A im Leerlauf. Die Hall-Drehzahl folgt grob dem Sollwert (P-Verhalten, bleibende Abweichung).
- **Abbruch:** Ruckeln/Brummen ohne Drehung (Hall-Offset, Phasenfolge), Netzteil geht in die Strombegrenzung, PVDD steigt über 26 V (Rückspeisung).
- **Hinweis:** Der Hall-Offset (0,38 im Block, 0,411 in der Konfiguration) wurde in der MA nur experimentell bestimmt. Bei schlechtem Lauf zuerst die Hall-Offset-Kalibrierung (MathWorks-Beispiel) einplanen, statt Regler zu verstellen.

### S5 – P1b.1 (`datasheet`, A2 Untergrenze 0)
- Wie S4, Firmware Stand P1b.1 + A2.
- **Erwartet:** Drehzahl erreicht den Sollwert (I-Anteil), etwa 15 % Überschwingen bei Sprung, kein hörbares Pfeifen/Schwingen. Weil die Untergrenze 0 ist, rollt der Motor beim Zurückdrehen des Sollwerts aus, statt zu bremsen.
- **Abbruch:** wie S4, zusätzlich Dauerschwingung im Strom (Pfeifen, Strom in der Begrenzung).

## 2. Beobachtung auf der Hardware (Entscheidung V6)

| Weg | Was | Aufwand | Regel |
|---|---|---|---|
| Multimeter/Oszilloskop an J1/J2 | ISENx, INHx/INLx, PVDD | sofort | Mensch misst |
| CAN 0x110/0x111 | PVDD, GVDD alle 200 ms | CAN-Interface nötig | CAN = Hardwarezugriff, Freigabe |
| CCS-Debugger (JTAG über FT2232 Kanal A) | Variablen im laufenden Code lesen (z. B. `Iab_PU`, Offsets) | CCS installiert? (`Downloads/CCS_21.0.1…zip` liegt vor) | Freigabe |
| Simulink External Mode über SCIA (USB-UART) | Signale live in Simulink | Modellkonfiguration ändern | Freigabe, eigener Schritt |
| Ethernet/TCP über CM-Core (MA) | IPC-Signale | hoch (CM-Modell, Patches) | – |

Empfehlung: S1–S3 nur mit Messgeräten. Für S4/S5 den CCS-Debugger oder später External Mode über SCIA als eigenen Schritt.

## 3. A2 – Bremsstrom-Grenze (vorbereitet, Ausführung nach Schritt 7)

Feste −0,5 an zwei Stellen: PID-Block `Speed Control/PI_Controller_Speed/PID Controller` (`LowerSaturationLimit`) und `foc_apply_fixes('speed_pi')` (setzt `'-0.5'` literal).

Geplante Änderungen (je Commit):
1. **`config/foc_config.m`:** neue Variable `ctrl_limits` in beiden Profilen, gesetzt nach dem `switch`: `legacy_f2838` → `Iq_min = -0.5` (bitgleich), `datasheet` → `Iq_min = 0`. `foc_legacy_f2838.m` bleibt unverändert (Abbild der `.mat`).
   **`tests/test_config.m`:** `ctrl_limits` wie `dataType` als erlaubte Zusatzvariable im Legacy-Vergleich; `ctrl_limits.Iq_min` in die Liste der vom Modell referenzierten Größen; Prüfung `datasheet: Iq_min == 0`. → `test_config` muss ALL PASSED melden.
2. **`tools/foc_param_brake_limit.m`** (liegt bereit): `LowerSaturationLimit = ctrl_limits.Iq_min`, vorher/nachher protokolliert.
   **`tools/foc_apply_fixes.m`:** `speed_pi` setzt die Untergrenze `ctrl_limits.Iq_min` statt `'-0.5'`.
3. **Nachweise:**
   - `foc_check_equivalence` (legacy): alle Abweichungen 0.
   - MIL-Tests: SW_ANF_05 prüft Iq_ref ∈ [−0,5, 0] bzw. ≤ 0 bei Freemode, das bleibt erfüllt. Die **Baseline des SpeedController-Tests ändert sich** (Iq_ref wird nicht mehr negativ) und muss neu aufgenommen werden; das wird dokumentiert.
   - `foc_run_p1b` erneut; Erwartung: C unverändert eingeschwungen, beim Herunterfahren Ausrollen statt Bremsen.

