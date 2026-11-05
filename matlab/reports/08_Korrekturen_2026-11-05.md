# Sammelkorrekturen nach dem FOCP-Test (2026-11-05)

Auftrag des Menschen: „Behebe auch die anderen Probleme. Scope Zeitachse etc.“ (inkl. Host-Änderungen).

## 1. Parameter gehen beim Verbinden verloren („Freigeben“ manchmal gesperrt)

Messung mit `host/tools/gsusb_can_check.ps1 -ParamBurst 42` (neu; 42 PARAM_REQ ohne Pause wie FOC Studio beim Verbinden). Drei Läufe:

| gesendet | auf dem Bus (TX-Echo 0x211) | Antworten 0x221 |
|---|---|---|
| 42 | 32 / 35 / 36 | 32 / 30 / 31 |

- **PC/CANable (Hauptursache):** Der Adapter verwirft Rahmen, wenn man schneller schreibt, als der Bus sie abnimmt. → `GsUsbTransport::send` wartet, solange 8 Rahmen ohne TX-Echo unterwegs sind (Timeout 20 ms, danach wird trotzdem gesendet, damit ein fehlendes ACK die Verbindung nicht blockiert). `receive` gibt beim Echo einen Platz frei.
- **Firmware (Nebenursache):** RX-FIFO 4 → 8 → **15** Mailboxen (17–31), TX 4–9. Die Blockset-Mailboxen 2, 3, 10 und 16 bleiben unberührt.
- **FOC Studio:** `Device::retryParams()` fragt unbeantwortete Parameter nach 300 ms erneut an, bis zu 4-mal und höchstens 4 Anfragen je 100-ms-Runde. Eine verlorene Schreibantwort wird durch Lesen nachgeholt.

## 2. Scope-Zeitachse um den Faktor 10 falsch

Die Firmware tastet die Telemetrie im Drehzahltakt ab (2 kHz). FOC Studio rechnete mit `inv.f_pwm` (20 kHz).
- **Protokoll:** neuer RO-Parameter `comm.telem_f` (ID 0x0C01, Default 0 = `inv.f_pwm`). Ältere Firmware und der virtuelle Antrieb bleiben unverändert.
- **Firmware:** setzt `comm.telem_f = 2000`.
- **Host:** `Device::telemetryBaseHz()` nutzt `comm.telem_f`, wenn gültig, sonst `inv.f_pwm`. Verwendet in Zeitachse, Scope-Abtastratenanzeige (Studio) und focstudio-dev.
- Später möglich: Abtastung im 20-kHz-Interrupt (Ströme in voller Auflösung).

## 3. Kriechen bei Poti 0 (Ruck alle ~20 s)

- `ctrl_limits.N_ref_min`: legacy 0 (aus, bitgleich), datasheet **0,005 PU = 20 rpm**.
- `tools/foc_speed_ref_deadband.m`: In `PI_Controller_Speed` gilt jetzt Reset = Freemode-Reset OR (|N_ref| < N_ref_min). Ausgabe: vorher Reset ← `Reset_I_And`, nachher Reset ← `Reset_I_Or`.
- Folge: Unter 20 rpm Sollwert regelt nur der P-Anteil, der Motor läuft dort also nicht an.

## 4. Baseline SW_ANF_05

Neu aufgenommen (`foc_recapture_baseline`, datasheet). Die Abweichung kam nur von Iq_max (alte Baseline bis 1,0, jetzt 0,1; Plot `img/MIL_SpeedController_SW_ANF_05_Baseline.png`).

## Nachweise

| Prüfung | Ergebnis |
|---|---|
| `test_config` | ALL PASSED |
| `foc_equivalence_p1a` (legacy) | Abweichung **0** |
| MIL (datasheet) | **4/4** bestanden |
| `foc_codegen` (datasheet) | erfolgreich, keine neuen Warnungen |
| `ctest` (PC-Teil, 5 Tests) | alle bestanden |

Offen: Der Knopf „Verbindung testen“ wird nur im getrennten Zustand angeboten. Die Fehlermeldung des Menschen fehlt noch. Hardwaretest aller Punkte steht aus.

## Hardwaretest

Der erste Flash dieses Stands hing (Strom-ISR in `MW_IPC_Write`, siehe `08_Fehler_Firmware_haengt_2026-11-05.md`). Nach dem Entfernen der IPC-Schreibblöcke (d006b47) meldet der Mensch: **alle Punkte funktionieren** (Verbinden und Freigeben, Scope-Zeitachse, kein Kriechen bei Poti 0).
