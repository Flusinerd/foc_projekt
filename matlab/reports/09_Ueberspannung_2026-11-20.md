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

## Hardware (2026-11-20, Stand 9192bfe)

Ablauf: Motor steht, nicht freigegeben. Der Mensch dreht PVDD von 24 V langsam auf 31 V, hält dort 10 s, fährt auf 28,5 V herunter und zurück auf 24 V. Mitgeschrieben hat `focp_test watch 300` (STATUS alle 0,5 s). Protokoll: `data/09_C_Ueberspannung_watch.txt`.

| Ereignis | Udc, Firmware | Bemerkung |
|---|---|---|
| Fehler „Überspannung“ gesetzt (t = 114,0 s) | 30,09 V (vorher 29,87 V) | Schwelle 30 V |
| Haltephase 31 V | Mittel 31,31 V, max. 31,87 V | Netzteil 31 V, an den Klemmen 30,98 V (Multimeter) |
| Fehler zurückgesetzt (t = 163,8 s) | 29,15 V (vorher 29,24 V) | Schwelle 29 V (1 V Hysterese) |

**Bewertung:**
- Die Abschaltung setzt bei der eingestellten Schwelle, die Hysterese wirkt, und die Rückkehr erfolgt selbstständig. Der Antrieb blieb dabei im Zustand READY mit gesetztem Fehlerbit 0x0008; der Motor war nicht freigegeben.
- Die Firmware misst ≈ 0,33 V zu viel (31,31 V gegen 30,98 V an den Klemmen; am 2026-11-10: 24,4 V gegen 23,95 V). Die Schwelle 30 V entspricht an den Klemmen also ≈ 29,7 V. Das liegt auf der sicheren Seite.
- Udc schwankt in der Haltephase um ±0,5 V. Die Hysterese von 1 V verhindert Flattern.
- **Nicht geprüft:** Abschaltung während eines Laufs. Bewusst nicht gemacht: Rückspeisung kann das Labornetzteil nicht aufnehmen, und bei laufendem Motor wäre ein Spannungssprung auf 31 V nicht unter Kontrolle.
