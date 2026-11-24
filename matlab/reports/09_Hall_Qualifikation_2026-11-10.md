# Schritt A: Eingangsqualifikation der Hall-Eingänge (2026-11-10)

Bezug: Hardware-Review HW10 (kein Filter, kein Schmitt-Trigger auf der Steuerplatine) und eigener Befund vom 05.11.: ein Drehzahlsprung auf den doppelten Wert in ≈ 54 s.

**Änderung** (`tools/foc_hall_qualification.m`, Log `09_hall_qual.log`). Die Werte stehen in `config/foc_cfg_board_carpediem.m` (`hall_qual_samples = 6`, `hall_qual_prd = 10`):

| | vorher | nachher |
|---|---|---|
| GPIO24/25/26 | Synchronize to SYSCLKOUT only | Qualification using 6 samples |
| GPIO24_31.QualPRD | 0 | 10 (Abtastperiode 100 ns) |

Störimpulse kürzer als 5 × 100 ns = **500 ns** werden verworfen. Bei 6000 rpm kommen 2400 Hall-Flanken/s, also ≥ 400 µs zwischen zwei Flanken: unkritisch. QualPRD gilt für GPIO24–31, also auch für den SafeState-Eingang GPIO30 (erwünscht).

**Nachweise:**
- `test_config`: ALL PASSED (neue Prüfung Fensterbreite = 500 ns)
- `foc_codegen`: erfolgreich, 0 Warnungen
- Im Code (`MW_c28xx_board.c`): `GPAQSEL2 = 0x2A0000` (Bits 16–21 = 2 = 6 Samples für GPIO24–26), `GPACTRL.QUALPRD3 = 10`
- Die Simulation ist nicht betroffen (nur Hardware-Einstellungen).

**Hardware:** noch offen (Flash 1). Messung: 10 min bei 1000 und 2000 rpm, Sprünge > 1,3 × Mittelwert und ungültige Hall-Folgen zählen.

## Hardware (2026-11-20, Stand 9192bfe)

Lauf mit 1000 rpm, 60 s, 2 kHz Telemetrie ohne Lücken (`data/09_E_Lauf1000.csv`):

| Größe | Ergebnis |
|---|---|
| Drehzahl (t = 3–60 s) | Mittel 1000,0 rpm, Std 13,5 rpm, 964–1030 rpm |
| Sprünge > 1,3 × Median | **0** |
| Hall | 24 099 Wechsel, 0 ungültige Zustände. 1 unzulässiger Übergang (3→5) bei t = 0,34 s, im ersten Ruck des Anlaufs aus dem Stand, im Lauf keiner |

Noch offen:
- 10-min-Lauf bei 1000 und 2000 rpm für Zielkriterium 2. Der frühere Stand lag bei 1 Sprung je ≈ 54 s.
- Mit 60 s ohne Sprung ist eine Verbesserung wahrscheinlich, aber statistisch noch nicht belegt.

## Zielkriterium 2: 10-min-Lauf 1000 rpm (2026-11-24, Stand 89b56b1)

`focp_test run 1000 600 speed,hall_state 10 ... --freigabe`, 2 kHz. Daten: `data/09_Z2_Lauf1000_10min.zip` (CSV 21 MB, gepackt 2,5 MB), Plot `img/09_Z2_Lauf1000_10min.png`.

| Größe | Ergebnis |
|---|---|
| Laufzeit, Fehler | 600 s, kein Fehler, Antrieb danach READY |
| Drehzahl (t = 3 s bis Ende) | Mittel **1000,0 rpm**, Std 13,6 rpm, 963,8–1034,2 rpm |
| Sprünge > 1,3 × Median | **0** (vorher ≈ 1 je 54 s, in 10 min also ≈ 11 zu erwarten) |
| Hall | 240 700 Wechsel, 0 ungültige Zustände. 1 unzulässiger Übergang (5→6) bei t = 0,33 s im Anlaufruck aus dem Stand, im Lauf **keiner** |
| Telemetrie | 768 von 1,2 Mio. Abtastungen verloren (CAN), größte Lücke 0,13 s |

**Bewertung:** Für 1000 rpm ist das Kriterium „10 min ohne Hall-Aussetzer“ erfüllt.

Hinweis zur CSV: `Device::exportCsv` schreibt die Zeit mit 6 signifikanten Stellen (ab 100 s nur noch ms-Auflösung). Für die Auswertung genügt das, weil die Abtastungen in fester Folge vorliegen. Für lange Aufnahmen sollte der Export die Zeit mit mehr Stellen schreiben (Änderung in `host/core`, nur auf Anweisung).

Offen: der gleiche Lauf bei 2000 rpm.
