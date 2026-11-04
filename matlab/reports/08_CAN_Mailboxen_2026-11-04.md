# CAN-Mailboxen getrennt: 0x111 (GVDD) auf Mailbox 2 (2026-11-04)

Anlass: Schritt 7 und `08_CAN_Test_2026-11-03.md`. In `app_speed` senden PVDD (0x110) und GVDD (0x111) beide über eCAN_A, Mailbox 1, also über dasselbe Message-Objekt. Vorbereitung für das erste Flashen (Schritt 8).

Keine Hardware angesteuert: kein Laden, kein Flashen, kein COM-Port, kein CAN. Build nur mit Build action „Build“ im Speicher.

## Wo die Blöcke sitzen

`app_speed/Input Controller` ist ein aufgelöster Bibliotheksverweis auf `focLib/Input Controller` (keine inaktiven Links in `app_speed`). Die eCAN-Blöcke liegen also in **focLib**; `app_speed.slx` selbst enthält keinen eCAN-Block und bleibt unverändert (SHA-256 `9813d3f6…73cfa40` vorher und nachher). Log: `08_mailbox_diag.log`.

## Änderung (`tools/foc_can_mailboxes.m`, Log `08_mailbox_change.log`)

| Block in `focLib/Input Controller` | Modul | ID | Mailbox vorher | Mailbox nachher |
|---|---|---|---|---|
| `BatteryVoltage_CodeGen/eCAN Transmit` (PVDD) | eCAN_A | 0x110 | 1 | 1 |
| `GVDD_Calculation/eCAN Transmit` (GVDD) | eCAN_A | 0x111 | 1 | **2** |
| `Freemode_Sig_CodeGen/eCAN Receive` | eCAN_A | 0x101 | 9 | 9 |
| `speedMode_CodeGen/eCAN Receive1` | eCAN_A | 0x102 | 15 | 15 |

Das Skript prüft vor dem Setzen, dass Mailbox 2 von keinem anderen eCAN-Block im Input Controller belegt ist, und speichert nur `focLib`.

## Nachweise

| Prüfung | Ergebnis |
|---|---|
| Erzeugter Code (`slbuild`, `ForceTopModelBuild`, Build action im Speicher „Build“, ExtMode aus) | erfolgreich, 71 s, `lastwarn` leer; dieselben 4 cl2000-Warnungen wie in Schritt 7 (3× IPC-Write-Init, 1× `MW_IPC_Init` im Blockset). Log `08_mailbox_build.log` |
| Message-Objekte im Code (`app_speed_ert_rtw/app_speed.c`) | 0x110: `CAN_setupMessageObject(CANA_BASE, 2, 0x110, …TX…)`, `CAN_sendMessage(CANA_BASE, 2, 2, …)`; 0x111: `CAN_setupMessageObject(CANA_BASE, 3, 0x111, …TX…)`, `CAN_sendMessage(CANA_BASE, 3, 2, …)`; Empfang 0x101 → Objekt 10, 0x102 → Objekt 16. **Getrennt** ✔ |
| `foc_check_equivalence` (legacy_f2838, Regler in focLib nur im Speicher auf P1a: P/I/UseKiTs/Grenzen aus `impmodel`, Iq_min −0,5, Reset aus) | 12 Signale, Abweichung **0**. Log `08_mailbox_equivalence.log` |
| `foc_run_mil_tests` (datasheet) | **4/4 bestanden**, Baselines ok. Log `08_mailbox_mil.log` |

Hinweis zur Zählung: Der Blockparameter `mailboxNo` ist 0-basiert, das DCAN-Message-Objekt im Code ist `mailboxNo + 1` (Mailbox 1 → Objekt 2, Mailbox 2 → Objekt 3). Objekt 1 bleibt frei.

Binärausgaben dieses Builds (nicht versioniert, nach `matlab/app_speed_ert_rtw/_out/` verschoben): `app_speed.out` SHA-256 `6cd5c58a…f8394ec3`, `app_speed.hex` `f7eed816…77b4e62d`. Diese Firmware enthält Mailbox-Trennung und externen 25-MHz-Takt, DeviceID weiterhin F28388D (siehe `08_DeviceID_2026-11-04.md`).

## Aufräumen

`git checkout -- matlab/slprj/sim/varcache/app_speed` (3 Dateien vom Build geändert), `matlab/MW_c28xx_pie.h` gelöscht, `app_speed.out/.hex/.elf` aus `matlab/` nach `matlab/app_speed_ert_rtw/_out/` verschoben. `matlab/impmodel.slxc` war nicht geändert.

## Für Schritt 8

Der CAN-Mitleser sieht 0x110 und 0x111 jetzt als eigenständige Botschaften (alle 200 ms). Beim Plan `08_Inbetriebnahme_Plan_2026-11-03.md` ändert sich sonst nichts.
