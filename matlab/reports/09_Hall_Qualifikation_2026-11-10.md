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
