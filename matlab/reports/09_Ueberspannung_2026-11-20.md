# Schritt C: Überspannungsabschaltung (2026-11-20)

Bezug: Hardware-Review HW19 (kein Bremschopper, keine Überspannungsklemme), offene Aufgabe A1. Entscheidung des Menschen: **30 V, selbstrücksetzend, Hysterese 1 V**.

## Änderungen

| Datei | Änderung |
|---|---|
| `config/foc_config.m` | `ctrl_limits.Udc_max = 30`, `Udc_hyst = 1`, `ov_enable` (legacy 0, datasheet 1), `focp_tune_udc_max` (zur Laufzeit änderbar) |
| `tools/foc_overvoltage.m` (neu) | **focLib/Input Controller:** Relay auf das feine Udc (V) mit EIN ≥ `focp_tune_udc_max` und AUS ≤ `focp_tune_udc_max − Udc_hyst`, AND `ov_enable` → neuer Ausgang `Overvoltage_Sig`. **app_speed:** Rate Transition auf Ts_speed, `Voltage_Fault_Or` mit Undervoltage_Sig vor dem Speed-Control-Eingang (Speed Control und die MIL-Harnesses sind unverändert); Eingang `ov` am FOCP-Block |
| `tools/foc_tunable_params.m` | `focp_tune_udc_max` als ExportedGlobal |
| `firmware/focp_c28x.c/.h` | Eingang `overvoltage` → `FOCP_FAULT_OVERVOLTAGE` im STATUS und Freemode-Zustand. `inv.Udc_max` wird mit dem Firmwarewert veröffentlicht und ist aus FOC Studio änderbar (7. Tunable, Faktor 1) |
| `tools/foc_test_protection.m` (neu) | minimaler Simulationstest |
| `host/tests/test_protocol.cpp` | Überspannung → Fehlerbit im STATUS, `inv.Udc_max` veröffentlicht |

Ausgabe `foc_overvoltage` (Log `09_overvoltage.log`):
- vorher: Input Controller 8 Ausgänge, kein Overvoltage_Sig, Unterspannung von Speed Control ← Rate Transition1, FOCP 12 Eingänge
- nachher: 9 Ausgänge, Overvoltage_Sig ja, Unterspannung ← Voltage_Fault_Or, FOCP 13 Eingänge

## Nachweise

| Prüfung | Ergebnis |
|---|---|
| `test_config` | ALL PASSED |
| `foc_test_protection` (Simulation 0,3 s) | 31 V/ein → 1, 24 V/ein → 0, 31 V/aus → 0: **ALL PASSED** |
| `foc_equivalence_p1a` (legacy) | Abweichung **0** |
| MIL | **4/4** |
| `ctest` protocol | bestanden |
| `foc_codegen` | erfolgreich, 0 Warnungen. Im Code `Overvoltage_Relay` auf `rtb_Product2` (Udc in V) gegen `focp_tune_udc_max` |

**Einschränkung:** Der Input Controller läuft mit 0,2 s. Gegen Rückspeisung reicht das, solange `Iq_min = 0` gilt (kein Bremsen). Einen schnellen Schutz kann nur die Hardware bieten (HW19).

**Hardware:** noch offen. Motor steht, PVDD 24 → 31 V: Freemode und „Überspannung“ in FOC Studio; bei 28,5 V wieder bereit.
