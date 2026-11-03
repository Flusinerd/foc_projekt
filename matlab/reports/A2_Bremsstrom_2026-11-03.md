# A2 – Bremsstrom-Grenze als Parameter (2026-11-03)

Ziel (AGENTS.md §6, Schritt 8): Die Untergrenze des Drehzahlreglers soll per Konfiguration einstellbar sein, statt fest −0,5 im PID-Block. Default 0, damit ein Labornetzteil keine Rückspeisung abbekommt.

## Änderungen

| Commit/Datei | Änderung |
|---|---|
| `7b2f7f6` `config/foc_config.m`, `tests/test_config.m` | `ctrl_limits.Iq_min`: legacy −0,5, datasheet 0 |
| `config/foc_config.m`, `tests/test_config.m` | `ctrl_limits.reset_I_freemode`: legacy 0, datasheet 1 |
| `tools/foc_param_brake_limit.m` | PID `LowerSaturationLimit = ctrl_limits.Iq_min` (vorher `-0.5`) |
| `tools/foc_apply_fixes.m` | `speed_pi` setzt die Untergrenze `ctrl_limits.Iq_min` statt `'-0.5'` |
| `tools/foc_speed_pi_reset.m` | PID `ExternalReset = level`, Reset = `FreemodeEnabled? AND ctrl_limits.reset_I_freemode` |
| `tools/foc_speed_assessment_tol.m` | SW_ANF_05 `DesiredTorqueNegative`: `Iq_ref <= 0.03` statt `<= 0` (Entscheidung des Menschen) |
| `tools/foc_recapture_baseline.m` | Baseline `SpeedController_SW_ANF_05` neu (datasheet, Stand A2) |

## Warum Reset und Toleranz

Mit Untergrenze 0 hält die Anti-Windup-Klemmung den I-Anteil an der Grenze mit einem kleinen positiven Rest fest. Im Freemode ist der Regelfehler genullt, deshalb gibt der Regler diesen Rest als Iq_ref aus: bis +0,024 PU. Damit war SW_ANF_05 („Freemode ⇒ Iq_ref ≤ 0“) verletzt. Mit dem **Reset im Freemode** ist Iq_ref dort exakt 0, und der Regler startet nach dem Freemode ohne Rest.

Bei negativer Regelabweichung ohne Freemode bleibt der eingefrorene Rest bis +0,024 PU (≈ 1,3 A Sollwert) sichtbar, wenn die Abweichung gegen 0 läuft. Das ist normales PI-Verhalten an der Grenze. Toleranz 0,03 PU; 0,01 PU reichte nicht.

## Nachweise

| Prüfung | Ergebnis |
|---|---|
| `test_config` | ALL PASSED (43 Größen je Profil) |
| `foc_check_equivalence` (legacy: Iq_min −0,5, Reset aus; Regler im Speicher auf P1a) | 12 Signale, Abweichung **0** |
| MIL P1b.1 + A2 (datasheet, Test Manager, Baselines) | **4/4 bestanden** |
| MIL P1a (legacy, Regler unkorrigiert, nur Speicher) | 4/4 bestanden |

Logs: `08_A2_*.log`, `06_run_mil_A2.log`, `06_run_mil_P1a.log` (nicht versioniert).

## Hinweis

`foc_run_p1b` (Läufe A/B/C) wurde mit A2 nicht wiederholt. Es setzt focLib mit unkorrigierten Reglern voraus, und der Stand enthält inzwischen die Harnesses. Bei Bedarf als eigener Schritt.
