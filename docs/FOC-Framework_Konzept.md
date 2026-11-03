# FOC-Framework – Konzept & Roadmap (Entwurf v0.1)

Ziel: Die CarpeDiem-Firmware (impmodel.slx, Masterarbeit Akgül 2023) wird zu einem **wiederverwendbaren FOC-Framework** ausgebaut. Damit lassen sich **Drehzahl- und Positionsregelung** als dünne Anwendungsmodelle umsetzen. Drei Bausteine kommen dazu: Simulink-Bibliothek, Inbetriebnahme-Tool (MATLAB) und Host-GUI.
Zielhardware: **F28388D-Platine + DRV8300** (wird bei Bedarf überarbeitet). Lagegeber: **Hall und sensorlos, umschaltbar**.

---

## 1. Schichtenarchitektur (Simulink)

```
 Anwendung (app_speed.slx / app_position.slx / app_motorid.slx)
   └─ wählt Regelmodus, Sollwertquelle, Geber, Parameter-Set
 ────────────────────────────────────────────────────────────
 Supervisor (Stateflow)      INIT → OFFSET_CAL → READY → ALIGN/START → RUN → FAULT
                             Fehler: OC, OV/UV, OT, Hall ungültig, Beobachter verloren, Kommunikation
 ────────────────────────────────────────────────────────────
 Regelung                    Lageregler (P + Vorsteuerung) → Drehzahlregler (PI + Rampe)
                             → Stromregler d/q (PI, Entkopplung, Spannungskreis-Begrenzung) → SVM
                             Modus per Laufzeit-Parameter: TORQUE | SPEED | POSITION | OPENLOOP_VF/IF
 ────────────────────────────────────────────────────────────
 Sensorik                    Strom (Offset-Kal.), Udc, Temperatur
                             Lage/Drehzahl: Variant {HALL | SENSORLESS | HYBRID}
 ────────────────────────────────────────────────────────────
 HAL (board-spezifisch)      ePWM, ADC/SOC, eCAP, GPIO, CMPSS/Trip-Zone, CAN, IPC
                             Variant {CARPEDIEM_F28388D | LAUNCHXL_F280049C | SIM}
 ────────────────────────────────────────────────────────────
 Kommunikation (CPU1 ↔ CM)   Telemetrie + Parameter/Kommandos, transportunabhängig (CAN / UART / TCP)
```

Grundsätze:
- Jeder Block ist ein **Library-Block mit Maske** und hat eine definierte Schnittstelle (Bus-Objekte statt loser Signale, z. B. `FocMeasBus`, `FocRefBus`, `FocStatusBus`).
- **Sim/Codegen-Varianten nur in der HAL** – alles darüber ist hardwareunabhängig und per MIL testbar.
- Regelmodus, Grenzwerte und Reglerparameter liegen in einer **Parametertabelle im RAM** (tunable, per GUI änderbar). Nur Struktur-Entscheidungen (Board, Geber-Typ) sind Kompilier-Varianten.
- Abtastraten: Stromregler 20 kHz (ADC-ISR), Drehzahl 2 kHz, Lage 1 kHz, Supervisor/Kommunikation 100 Hz.

## 2. Lagegeber

| Variante | Einsatz | Bemerkung |
|---|---|---|
| HALL | Anlauf, niedrige Drehzahl, Positionieren | Auflösung 6 Zustände × 4 Polpaare = 24 Schritte/U ⇒ **15° mechanisch**. Zwischen den Flanken wird interpoliert, im Stillstand aber nicht. |
| SENSORLESS | mittlere/hohe Drehzahl | Beobachter (Sliding-Mode oder Flussbeobachter, MCB-Blöcke als Basis) + I/f-Hochlauf. **Im Stillstand nicht möglich**: Der ECI 63.60 hat Ld ≈ Lq, also keine Schenkligkeit, also funktioniert keine HF-Injektion. |
| HYBRID | Standardfall Drehzahlregelung | Unterhalb einer Schwelle Hall, darüber Beobachter, Überblendung mit Hysterese; Hall dient zusätzlich zur Plausibilisierung. |

⚠ **Positions-/Servoregelung nur mit Hall** ergibt ±7,5° mechanisch. Für echte Servo-Anwendungen sollte ein höher auflösender Geber vorgesehen werden (ECI-Variante mit Encoder → eQEP, oder magnetischer Winkelsensor, z. B. über SPI/ABI). Die Geber-Schicht ist so geschnitten, dass das später als weitere Variante (`ENCODER`) dazukommt.

## 3. Inbetriebnahme-Tool (MATLAB-Paket `+foc`)

Ersetzt `params_f2838.mat` und `mcb.internal.SetControllerParameters` (internes, undokumentiertes API).

| Funktion | Inhalt |
|---|---|
| `foc.config.*` | Skripte statt .mat: `board_carpediem.m`, `motor_eci6360.m`, `inverter_drv8300.m`, `ctrl_defaults.m` → `cfg = foc.buildConfig(...)`. Alle abgeleiteten Größen (PU-Basis, PWM-Periode, Abtastzeiten, Glitch-Filter) werden berechnet, nicht eingetragen. |
| `foc.design.*` | Transparente Reglerauslegung: Stromregler (Betragsoptimum, Polkompensation L/R), Drehzahlregler (symmetrisches Optimum), Lageregler; Beobachter-Gains; Bode-/Sprungplots zur Kontrolle. |
| `foc.ident.*` | Motoridentifikation **auf dem Target**: Rs (DC-Einprägung), Ls (Spannungssprung / Stromanstieg), Ψ_PM (Leerlauf-EMK im I/f-Betrieb), J/B (Hochlauf/Auslauf). Dafür gibt es einen eigenen Supervisor-Zustand `IDENT`. |
| `foc.calib.*` | Stromoffset, Hall-Offset und Hall-Sequenz automatisch (Rotor ausrichten, langsam drehen, Flanken gegen Winkel aufnehmen). |
| `foc.build` | Build/Flash für Board + App, schreibt die Konfiguration in eine Datei der Basissoftware. |

## 4. Host-GUI („FOC Studio“) – umgesetzt in v0.1

- **Nativ in C++17**: Dear ImGui (Docking) + ImPlot, GLFW/OpenGL 3, CMake. Unter Windows und Linux ergibt das eine einzelne ausführbare Datei.
- **Protokoll FOCP** (`protocol/`, C99, auf C28x lauffähig): Jede Nachricht hat höchstens 8 Byte und passt in einen CAN-Frame; für Streams kommt COBS + CRC-16 dazu. Parameter- und Signaltabellen sind X-Macros und gelten gleichzeitig für Firmware und Tool. Funktionen: Parameter lesen/schreiben (mit Bereichsprüfung und Sperre während des Betriebs), Kommandos, Telemetrie (8 Slots, Teiler), Status (10 Hz), Ereignisse, Heartbeat-Überwachung.
- **Transporte**: virtueller Antrieb (gleicher C-Code wie auf dem Target + PMSM-Modell), SocketCAN, PCAN-Basic (wird zur Laufzeit geladen). **Als Nächstes UART**: Die Platine hat USB-C → FT2232HL → SCIA (GPIO84/85), das Framing ist fertig, und es braucht weder CM-Core noch die fehlenden Ethernet-Patches. TCP über den CM folgt später.
- **Firmware-Anbindung** `firmware/focp_target.c` für die C-Caller-Blöcke in Simulink.

## 5. Hardware-Änderungen (Empfehlung, abgeglichen mit MA Darici)

Details und Pinbelegung: `claude/Hardware-Ueberblick_Frequenzumrichter.md`.

| Priorität | Änderung | Stand laut Hardware-MA |
|---|---|---|
| hoch | **Hardware-Überstromabschaltung** CMPSS → ePWM-Trip-Zone | Im QFP nur für **I_B** möglich (ADCINB2 = CMPIN3P → CMPSS3). I_A (ADCINB0/VDAC) hat keinen Komparator, I_C (CMPIN3N) nur als Gegeneingang. Lösung: Adapter/Drahtbrücke auf freie CMPINxP-Pins von J1 (ADCIN14, ADCINC4, ADCIND0, ADCIND2) bzw. Revision. Als ersten Schritt CMPSS3 auf I_B sofort nutzen. |
| hoch | **ADC-Referenz** | VREFHI hängt direkt an der 3,3-V-Schaltreglerschiene (gemessen 3,36 V). Software: Vref-Parameter auf den Messwert setzen. Revision: Präzisionsreferenz 3,0 V mit Puffer, VDDA getrennt filtern. |
| hoch | **Hall-Eingänge** entstören (RC + Schmitt-Trigger, Pull-up) und Input-Qualification in Software | Die Anpassungen gegen das Hall-Rauschen sind in der Hardware-MA nicht beschrieben, vermutlich stehen sie in `Erratas.xlsx`, die uns fehlt. |
| erledigt | Dritter Phasenstrom | **vorhanden** (ADCINB3), Firmware nutzt ihn noch nicht → einbinden |
| erledigt | Phasenspannungsmessung | **vorhanden** (ADCINA4/A3/A2), Firmware nutzt sie noch nicht → für Beobachter, Fliegenden Start und Motoridentifikation einbinden |
| mittel | Strommessung auf getrennte ADC-Module (gleichzeitige Abtastung) | alle drei Ströme hängen an ADC-B → bei einer Revision ändern |
| mittel | Encoder für Servo | **Anschluss vorgesehen**: eQEP3 auf J9 (GPIO62/63/64/65), nicht verifiziert. GPIOs sind nicht 5-V-tolerant → bei 5-V-/RS-422-Gebern einen Empfänger vorsehen. |
| mittel | Testpins/LEDs | User-LEDs auf der Platine vorhanden |

## 6. Basissoftware / Infrastruktur

- Git-Repo + **Simulink Project** (`.prj`) mit Pfaden, Startskript und Shortcuts.
- **Ethernet-Pinmux ohne Patch der MATLAB-Installation:** eigene Pinmux-Initialisierung im Custom Code/`System Initialize` des CM-Modells, die nach der Standard-Initialisierung umkonfiguriert. Alternativ die Patches der MA als versionierte Overlay-Dateien im Repo. Ziel: Build auf jedem Rechner reproduzierbar.
- Tests: MIL-Harnesses je Library-Block (Simulink Test), PIL für Rechenzeit, Requirements-Verlinkung (vorhandene `.slreqx` fortschreiben).

## 7. Roadmap (Vorschlag)

| Phase | Inhalt | Ergebnis |
|---|---|---|
| P0 Fundament | Repo/Projekt, Konfig-Skripte statt .mat, Ethernet-Pinmux-Lösung, bestehendes Modell baut und läuft wieder auf der Platine | **Skripte fertig** (`matlab/`, in Octave getestet), Lauf in MATLAB steht aus |
| P1 Bibliothek | P1a: Komponenten in focLib + app_speed (Äquivalenztest), P1b: Reglerkorrekturen B1/B2, danach HAL/Busse/Stateflow-Supervisor, MIL-Tests je Block | **Build-Skripte P1a/P1b fertig**, nicht in MATLAB getestet |
| P2 Schutz & HW | Supervisor mit Fehlerbehandlung, CMPSS-Trip, Hall-Hardware überarbeitet | sicherer Betrieb |
| P3 Kommunikation & GUI | Protokoll, CAN-Transport, Python-GUI (Plots, Parameter, Kommandos) | Laufzeit-Tool |
| P4 Inbetriebnahme | Offset-/Hall-Kalibrierung, Motoridentifikation, Reglerauslegung | gemessene Motorparameter, ausgelegte Regler |
| P5 Regelmodi | Momenten-, Drehzahl- (Rampen, Begrenzungen), Lageregelung | `app_position.slx` |
| P6 Sensorlos | Beobachter, I/f-Start, Hybrid-Umschaltung | Geber-Variante SENSORLESS/HYBRID |
| P7 Verifikation | Anforderungen, Testberichte, Doku | Abschluss Projektarbeit |

## 8. Offene Entscheidungen

- MATLAB-Version und verfügbare Toolboxen (MA: R2023a, Motor Control Blockset 2.0, C2000 Blockset)
- ~~GUI-Technologie~~ → nativ C++ (ImGui/ImPlot), erster Transport CAN
- Umfang der Projektarbeit: welche Phasen sind Pflicht, welche optional
- Hardware-MA liegt vor. Es fehlen noch: vollständiger Schaltplan, `Erratas.xlsx`, BOM, Ethernet-Patches der Firmware-MA
