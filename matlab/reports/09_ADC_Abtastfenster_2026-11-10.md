# Schritt B: ADC-Abtastfenster verlängern (2026-11-10)

Bezug: Hardware-Review HW07 (keine Kondensatoren an den ADC-Pins, zu kurzes Abtastfenster).

**Befund vorher** (Blockparameter `SOCAcquisitionWindow` in SYSCLK-Takten zu 5 ns, ACQPS = Wert − 1):

| Kanal | SOC | vorher | Bemerkung |
|---|---|---|---|
| I_A, I_B Messung | ADCB SOC0/1 (ePWM6-SOCA) | 13 = **65 ns** | unter dem Datenblatt-Minimum 75 ns (12 bit) |
| I_A, I_B Offsetkalibrierung | ADCB SOC2/3 | 15 = 75 ns | **anderes Fenster als die Messung** |
| PVDD | ADCA SOC3 | 8 = 40 ns | Quellwiderstand ≈ 2,9 kΩ |
| GVDD | ADCC SOC4 | 8 = 40 ns | |
| Poti | ADCA SOC2 | 7 = 35 ns | |
| Temperatur | ADCC SOC5 | 15 = 75 ns | |

**Änderung** (`tools/foc_adc_acquisition.m`, Log `09_adc_acq.log`). Die Werte stehen in `config/foc_cfg_board_carpediem.m` (`adc_acq_cur_ns = 300`, `adc_acq_slow_ns = 1000`):
- Ströme (Messung **und** Kalibrierung): 60 Takte = **300 ns**
- PVDD, GVDD, Poti, Temperatur: 200 Takte = **1 µs**

**Nachweise:**
- `test_config`: ALL PASSED
- `foc_equivalence_p1a`: Abweichung 0
- MIL: 4/4
- `foc_codegen`: 0 Warnungen
- Im Code (`MW_c28xx_adc.c`): ACQPS 59 bzw. 199
- Die Wartezeit der softwaregetriggerten Kanäle passt das Blockset selbst an: `RPT #255` + `#5` NOP für die langsamen Kanäle (vorher kürzer), `RPT #120` für die Kalibrierung (vorher 68).
- Der Strom-ISR startet je Wandlung ≈ 0,2 µs später; bei 50 µs Periode unkritisch.

Neu: `tools/foc_eval_scope_csv.m`, wertet FOC-Studio-CSV aus (Drehzahlsprünge, Hall-Folge, Strom-Std und FFT-Linien).

**Hardware:** noch offen (Flash 1). Strom-Rauschen im Stillstand vorher/nachher, PVDD gegen ein Multimeter.

## Vorher-Messung (Firmware adc8175, vor Flash 1)

| Größe | Wert | Quelle |
|---|---|---|
| PVDD FOC Studio / Multimeter | **24,4 V / 23,95 V** (+1,9 %) | Mensch |
| Strom-Std im Stillstand ia / ib / ic | **0,127 / 0,139 / 0,212 A** (ic = −(ia+ib), daher größer) | `Strommessung.csv`, `foc_eval_scope_csv` |
| Mittelwert (Offsetrest) ia / ib | −0,100 / −0,086 A | dto. |
| Hall-Lauf 1000 rpm | 100 s (Pufferende), 37 543 Wechsel, **0 Sprünge, 0 unzulässige Übergänge**, ab t = 182 s Stillstand | `Hall.csv` |

Einschränkung: `Strommessung.csv` hat 16 110 Lücken (4 Signale × Teiler 6 ≈ 6700 CAN-Rahmen/s, Bus überlastet). Die Std ist verwertbar, die FFT nicht. Für die Nachher-Messung dieselben Einstellungen wählen und zusätzlich eine Aufnahme mit nur `ia` (Teiler 10, 1 Rahmen/Abtastung) machen, für eine lückenlose FFT.
