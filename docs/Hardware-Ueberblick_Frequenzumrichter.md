# Hardware-Überblick – modularer Frequenzumrichter (MA Darici, Dez 2023)

> Quelle: Masterarbeit E. Darici, „Entwicklung und Implementierung der Hardware eines modularen Frequenzumrichters“, HS Bochum, 17.12.2023. Abgeglichen mit dem F2838x-Datenblatt (SPRSP14E, im Archiv als [40]) und der Firmware (impmodel.slx).
> Schaltplan, BOM, Erratas und Lastenheft des Motorcontrollers sind laut Anhang A.1 eigene Dateien (`CarPediem_Motorcontroller_schematic.pdf`, `Erratas.xlsx`, `BOM.csv`, `Lastenheft_Motorcontroller.pdf`). Sie liegen uns **nicht** vor. In der Arbeit sind nur Ausschnitte des Schaltplans als Bilder enthalten.

## 1. Aufbau

| Baugruppe | Umsetzung |
|---|---|
| MCU | **TMS320F28386D im 176-Pin-QFP (PTP)**, nicht F28388D-BGA wie auf der controlCARD. Pin-kompatible Software, ohne EtherCAT. Externer **25-MHz-Oszillator** (über Puffer auch für den PHY). |
| Leistungsstufe | **DRV8300DIPW-EVM** (zugekauft, 6–100 V, 30 A, 3 Shunts mit CSA, Temperatursensor), über zwei 20-polige Stiftleisten J1/J2 im LaunchPad-F280049C-Raster angeschlossen. Die Regler des EVM sind per 0-Ω-Widerständen abgetrennt, GVDD (12 V) und 3,3 V kommen von der Hauptplatine. |
| Versorgung | Eingang 15,25 … 40,34 V (TPS26601 eFuse, Verpolschutz bis −60 V, 1,1 A Strombegrenzung; gemessen) → LT8609 12 V → LT8609 3,3 V → LTC3302 1,2 V. Alternativ **USB-C 5 V** (ORing), dann läuft die Logik ohne Leistungsstufe. Gemessen: 12,06 V / **3,36 V** / 1,22 V. |
| USB | FT2232HL: Kanal A = JTAG (CCS/Simulink-Flash), Kanal B = UART an **SCIA (GPIO84 TX / GPIO85 RX)** |
| CAN | ISO1050 (U8, galvanisch getrennt) an GPIO4 (TX, Pin 164) / GPIO5 (RX, Pin 165), Phoenix-Klemme PTS 1,5/5-5,0-H (5-polig, RM 5,0) – Belegung siehe unten |
| Ethernet | DP83822 (MII), wegen QFP an anderen GPIOs als auf der controlCARD (→ `config_CC_Custom.xlsx` und Patch `CARPEDIEM_PIN_CORRECTION` der Firmware-MA) |
| Leiterplatte | 4 Lagen (Eurocircuits Defined Impedance), MII 50 Ω, MDI/USB differentiell |

### CAN-Klemme (am Aufbau durchgemessen, 2026-11-03)

Klemme PTS 1,5/5-5,0-H am oberen Platinenrand, Kontakte **von links nach rechts** (Durchgang zum ISO1050 U8 gemessen; Signalnamen nach TI-Datenblatt ISO1050, SOIC-8):

| Kontakt | ISO1050-Pin | Signal |
|---|---|---|
| 1 | 8 | **VCC2** – 5-V-Versorgung der isolierten Busseite (**muss von außen eingespeist werden**) |
| 2 | 7 | **CAN_H** |
| 3 | 6 | **CAN_L** |
| 4 | 5 | **GND2** (isolierte Masse) |
| 5 | 5 | **GND2** |

Folgen:
- Die Busseite des ISO1050 hat **keine eigene Versorgung auf der Platine**: Am Bus müssen 5 V (4,75–5,25 V laut Datenblatt) auf Kontakt 1 und GND2 auf 4/5 anliegen. Ohne VCC2 bleibt der Transceiver busseitig stumm.
- **Terminierung (120 Ω):** auf der Platine nicht belegt. Hinter der Klemme sitzt eine 2-polige Stiftleiste mit Widerstand (möglicherweise ein Terminierungs-Jumper, nicht verifiziert). Für einen Laboraufbau Terminierung an beiden Busenden sicherstellen bzw. messen (CAN_H–CAN_L ≈ 60 Ω bei korrekt terminiertem Bus).
- Bitrate laut Modell 1 Mbit/s (eCAN-A).
- Quellen: MA Darici Abb. 5.6 (S. 32, GPIO4/5), Abb. 5.17 (S. 47, U8 und Klemme), Literatur [45]. Vollständiger Schaltplan (`CarPediem_Motorcontroller_schematic.pdf`) liegt weiterhin nicht vor.

## 2. Pinbelegung zur Leistungsstufe (J1 analog, J2 digital)

| Signal | MCU-Pin | Firmware nutzt | Bemerkung |
|---|---|---|---|
| PWM U/V/W (High/Low) | GPIO10/11, 12/13, 14/15 = ePWM6A/B, 7A/B, 8A/B | ja | |
| Hall A/B/C | GPIO24/25/26 | ja (eCAP1–3 über Input-X-BAR) | gleiche Pins können **eQEP2** A/B/Index (Mux 2) |
| FAULT/SPI_SIMO | GPIO91 | nein | aus dem LaunchPad-Raster |
| I_A / I_B / I_C | ADCINB0 / ADCINB2 / **ADCINB3** | nur A und B | **dritter Phasenstrom ist verdrahtet, aber ungenutzt** |
| U_A / U_B / U_C (Phasenspannung) | **ADCINA4 / A3 / A2** | nein | **Phasenspannungsmessung ist vorhanden** (sensorlos, Fliegender Start, Motoridentifikation) |
| PVDD (Zwischenkreis) | ADCINA5 | ja | |
| GVDD | ADCINC3 | ja | |
| Temperatur (TMP23x) | ADCINC2 | ja | |
| Poti/Sollwert | ADCINA0 (J1-2) | ja | |

Zusätzliche Stecker: **J3** (ADCINA1/B1, SCI GPIO38/39, GPIO28–31, 36/37, 48/49, SPI), **J9** (ePWM1/2/4 auf GPIO0–3/6/7, **GPIO62/63/64/65**, weitere GPIOs). Im Datenblatt geprüft: GPIO62/63/64/65 = **eQEP3** A/B/Strobe/Index (Mux 5). Damit ist ein Inkrementalgeber-Anschluss auf J9 vorgesehen; die Arbeit hat ihn allerdings nicht verifiziert. GPIO28/29/31 auf J3 können ebenfalls eQEP3 (Mux 6). Achtung: GPIO30 (= eQEP3 Strobe) ist in der Firmware der SafeState-Taster.

## 3. Befunde mit Folgen für die Weiterentwicklung

1. **ADC-Referenz = 3,3-V-Schaltreglerschiene.** VREFHIA–D hängen direkt an der 3,3-V-Ebene (LT8609, 2 MHz, gemessen 3,36 V), aus der auch VDDA und die Logik versorgt werden. Laut Datenblatt muss VREFHI extern eingespeist werden; empfohlen sind 2,5 oder 3,0 V, zulässig sind 2,4 V bis VDDA. Folgen: +1,8 % Verstärkungsfehler gegenüber den in der Firmware angenommenen 3,3 V, Schaltreglerrauschen geht direkt in Strom- und Spannungsmessung ein. Das erklärt die ADC-Probleme aus der Firmware-MA.
   - Sofort: `ADC_Vref` = gemessene 3,36 V setzen, besser zur Laufzeit gegen einen Referenzkanal kalibrieren.
   - Nächste Revision: eigene Präzisionsreferenz (3,0 V) mit Puffer für VREFHI, VDDA über Ferrit/LC getrennt filtern.
2. **Hardware-Überstromabschaltung (CMPSS) nur für Phase B möglich.** Im QFP-Gehäuse gilt:
   - ADCINB0 (I_A) ist zugleich VDAC und hat **keinen Komparatoreingang**, dafür einen fest eingebauten 100-pF-Kondensator.
   - ADCINB2 (I_B) = CMPIN3P → CMPSS3 mit DAC-Schwelle oben/unten → Trip-Zone ePWM6–8 möglich.
   - ADCINB3 (I_C) = CMPIN3N → nur als Gegeneingang zu CMPIN3P nutzbar, keine eigene Schwelle.
   - Für eine vollständige HW-Abschaltung I_A und I_C auf freie CMPINxP-Pins legen. Auf J1 liegen ADCIN14 (CMPIN4P), ADCINC4 (CMPIN5P), ADCIND0 (CMPIN7P), ADCIND2 (CMPIN8P). Das geht per Adapterplatine/Drahtbrücke oder in der nächsten Revision.
   - **Lösung ohne Umbau (Software-Konfiguration):** I_B über CMPSS3, I_A und I_C über die **ADC-Postprocessing-Blöcke (PPB) mit Grenzwertprüfung**. Laut TRM SPRUII0E, Kap. 20.8.3 (S. 2564) lösen diese ohne CPU einen ePWM-Trip aus: ADCBEVTx → ePWM-X-BAR → Digital Compare → One-Shot-Trip. Die Wirkung kommt aber erst mit dem nächsten Abtastwert.
   - **Grenze jeder Shunt-Lösung auf dieser Leistungsstufe:** Die Shunts sitzen auf der Low-Side und sehen Strom nur, solange der Low-Side-Schalter leitet. Ein harter Kurzschluss ohne Motorinduktivität (Brückenkurzschluss, Kurzschluss an den Klemmen) wird damit nicht sicher erkannt. Dafür sind eine Sicherung im Leistungspfad und bei einer Revision ein Zwischenkreis-Shunt mit Komparator oder ein Gatetreiber mit VDS-Überwachung nötig.
   - Außerdem liegen alle drei Ströme auf ADC-B und werden daher nacheinander gewandelt. Bei einer Revision die Ströme auf verschiedene ADC-Module verteilen, damit sie gleichzeitig abgetastet werden.
3. **Encoder ohne neue Platine möglich** (eQEP3 auf J9). Vorher prüfen: Pegel. Die GPIOs des F2838x sind **nicht 5-V-tolerant**. Ein 5-V- oder RS-422-Encoder braucht einen Leitungsempfänger bzw. Pegelwandler.
4. **Hall-Entstörung undokumentiert.** Die Firmware-MA verweist bei den „Hardwareanpassungen“ gegen das Hall-Rauschen auf diese Arbeit. Hier ist dazu nichts beschrieben; vermutlich stehen die Details in `Erratas.xlsx`. Der Hall-Anschluss sitzt auf dem DRV8300-EVM.
5. **UART über USB ist sofort nutzbar** (FT2232HL → SCIA). Damit kann das FOCP-Protokoll per COBS-Framing laufen, ohne Ethernet, ohne CM-Core und ohne die fehlenden Basissoftware-Patches.
6. **Takt:** externer 25-MHz-Oszillator → in den Simulink-Hardware-Settings Oszillatorquelle und -frequenz prüfen.
7. **Sicherheitsrelevant:** Der DRV8300 hat keine eigene Überstrom- oder Kurzschlussüberwachung (reiner Gatetreiber). Ohne CMPSS-Trip schützt nur die Software und die 1,1-A-Begrenzung der eFuse. Die eFuse liegt nur im Logikpfad, nicht im Leistungspfad.

## 4. Offene Unterlagen (beim Institut anfragen)

- `CarPediem_Motorcontroller_schematic.pdf` (vollständiger Schaltplan)
- `Erratas.xlsx` (Hardwareänderungen: FT2232HL-Beschaltung, Boot-Mode-Register, Hall-Entstörung?)
- `BOM.csv`, `HW_Architektur_Erfassung.xlsx`, `Lastenheft_Motorcontroller.pdf`
- Layoutdaten (für eine Revision)
