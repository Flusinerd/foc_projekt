# Stromregler: I-Anteil im Freemode zurücksetzen (2026-11-05)

Anlass: Beim Freigeben aus FOC Studio gab es einen Stromstoß von **−31 A** gegen die Sollrichtung (3,5 ms, Rotor ruckt rückwärts). Ursache: Im Freemode ist die PWM aus, die I-Anteile der Stromregler integrieren aber den Messoffset bis an die Spannungsgrenze ±1 PU (`08_FOCP_Einbau_Plan_2026-11-04.md`, Abschnitt Hardwaretest Telemetrie). Entscheidung des Menschen: umsetzen.

## Änderungen

| Datei | Änderung |
|---|---|
| `config/foc_config.m` | `ctrl_limits.reset_I_cur_freemode`: legacy **0** (bitgleich), datasheet **1** |
| `tests/test_config.m` | prüft den Wert in beiden Profilen; ergänzt in der Liste der referenzierten Größen → **ALL PASSED** |
| `tools/foc_current_pi_reset.m` (neu) | `focLib/Torque Controller`: Goto `FreemodeSig_I` (scoped) und Goto Tag Visibility. `PI_Controller_Id/Iq`: PID `ExternalReset = level`, Reset = From `FreemodeSig_I` AND `ctrl_limits.reset_I_cur_freemode` |

Ausgabe (Log `08_cur_reset.log`): für d und q jeweils vorher `ExternalReset=none`, nachher `ExternalReset=level`; focLib gespeichert.

## Nachweise

| Prüfung | Ergebnis |
|---|---|
| `foc_equivalence_p1a` (legacy, Reset aus) | 12 Signale, Abweichung **0** |
| MIL (datasheet) | 3/4; SW_ANF_05 weiterhin nur Baseline (Iq_max) |
| `foc_codegen` (datasheet) | erfolgreich, keine neuen Warnungen; `Reset_I_Cur_Enable = true` an beiden Reglern |

Auf der Hardware noch zu prüfen: Anlauf aus FOC Studio mit Teiler 1. Erwartung: kein negativer Stromstoß, i_q steigt direkt auf ≈ Iq_max, und die Hall-Folge beginnt vorwärts.

## Hardwaretest (2026-11-05)

Geflasht (`08_cur_reset_flash.log`). Aufnahme `focstudio_scope_20261105_122838.csv` (Teiler 1, real 2 kHz, ≈ 54 s, 1000 rpm, 24 V, Leerlauf). Sie enthält Stopp → Ausrollen → erneutes Freigeben aus dem Stillstand.

| Größe | vorher (ohne Reset) | jetzt |
|---|---|---|
| i_q im ersten Abtastwert nach dem Freigeben | **−27,8 A**, Minimum −30,7 A, 3,5 ms negativ | **+4,0 A**, danach 4,6–5,3 A = Iq_max ✔ |
| kleinster i_q der ganzen Aufnahme | −30,7 A | **−0,76 A** |
| Hall-Folge beim Anlauf | zuerst rückwärts (3→2→6→4) | nicht aufgezeichnet (Hall nicht ausgewählt) |
| Anlauf 0 → 990 rpm | – | 175 ms |

**Ergebnis: Der Stromstoß beim Freigeben ist behoben.**

Nebenbefund: Einmal in ≈ 54 s springt die gemessene Drehzahl für 2,5 ms auf **1975 rpm** (≈ doppelter Wert). Der Drehzahlregler setzt i_q-Soll dabei für 2,5 ms auf 0. Vermutlich ein Störimpuls auf einer Hall-Leitung: Ein zusätzlicher Interrupt halbiert das gemessene Zeitintervall (bekanntes Problem aus der MA, `docs/Firmware-Ueberblick_CarPediem.md`). Offen, gehört zur Hall-Filterung.

Zuvor: „Freigeben“ war nach dem Flashen gesperrt. Grund: Die Antwort auf `ctrl.mode` war im Parameter-Schub beim Verbinden verloren gegangen (RX-FIFO mit 4 Plätzen lief über). Workaround: neu verbinden. Behoben in ca98e22 (FIFO 8 tief), **noch nicht geflasht**.
