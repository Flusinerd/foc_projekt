# Befund: Überstrom-Trip beim Einschalten (2026-11-20)

## Beobachtung

| Stand | Nach Neustart (Steuerplatine und PVDD am selben Netzteil, gleichzeitig) |
|---|---|
| 9192bfe (ohne Löschen beim Scharfschalten) | dreimal „Überstrom (HW)“; ACK sofort möglich, danach normal |
| 4ca5efb (HW-Trip wird beim Scharfschalten nach 500 ms einmal gelöscht) | einmal „Überstrom (SW)“, HW-Trip nicht mehr gemeldet |

Nach dem Fehler im Stillstand (`focp_test record ia,ib,ic 10 2`):

| Signal | Mittel | Min | Max |
|---|---|---|---|
| ia | −0,17 A | −0,55 A | 0,30 A |
| ib | −0,05 A | −0,52 A | 0,47 A |
| ic | 0,00 A | −0,58 A | 0,49 A |

Offsets und Rauschen sind normal. Ein Dauerstrom fließt nicht.

## Schluss

- Die Löschung beim Scharfschalten wirkt: Der HW-Trip war beim Scharfschalten gespeichert, und der Komparator war inaktiv.
- **Danach** hat die Software-Überwachung |i| > 15 A gesehen. Das kann vorher nicht sichtbar werden: Die Software-Prüfung läuft nur, solange kein Trip gespeichert ist (`armed && !hal_trip_active()`). Mit gespeichertem HW-Trip war sie bisher verdeckt.

## Vermutungen (nicht belegt)

1. **Poti-Betrieb nach dem Löschen:**
   - Ohne FOCP-Rahmen läuft der Antrieb aus dem Poti. `freemode` ist dann 0, die Endstufe wäre also ohne Trip aktiv.
   - Während des gespeicherten Trips (≈ 500 ms) liefen Strom- und Drehzahlregler weiter, ohne dass Strom floss. Sie können sich dabei aufintegriert haben.
   - Nach dem Löschen gibt die Endstufe sofort die aufgelaufene Spannung aus. Ein Stromstoß > 15 A wäre die Folge.
   - Steht das Poti nicht auf 0, würde der Motor anlaufen.
2. **Offset I_C beim Hochlaufen:**
   - Der Offset von I_C wird in den ersten 500 ms gemittelt, während die Versorgung (gemeinsames Netzteil) noch einschwingt.
   - Dagegen spricht, dass ic jetzt den Mittelwert 0,00 A hat.
3. **Sprung der Stromsignale** beim Einschwingen der Messverstärker nach 500 ms. Unwahrscheinlich, die Versorgung sollte dann stabil sein.

## Vorschlag

- Nicht weiter probieren, sondern messen:
  - Mensch: Poti-Stellung beim Einschalten prüfen. Zuckt der Motor beim Einschalten?
  - JTAG ohne Reset, nach dem Einschalten: `focp_c28x_boot_trips` (0x16CE2) lesen. 1 heißt, die Löschung beim Scharfschalten hat stattgefunden.
- Bestätigt sich Vermutung 1, ist das Löschen im Poti-Betrieb ohne zurückgesetzte Regler nicht sicher. Dann gibt es zwei Möglichkeiten:
  - (a) Die Änderung 4ca5efb zurücknehmen; der Trip beim Einschalten bleibt bis ACK bestehen (sicherer Zustand).
  - (b) Im Modell bis zum Scharfschalten Freemode erzwingen; die Regler starten dann zurückgesetzt. Das ist eine Modelländerung.

## Messung mit Diagnose-Firmware (`focp_c28x_trip_diag`, JTAG ohne Reset, `host/tools/ccs_read_trip_diag.js`)

| Start | USB/Netzteil | HW-Trip gesehen | gelöscht | SW-Trip | Ströme beim SW-Trip | max \|i\| vorher |
|---|---|---|---|---|---|---|
| 1 | erst USB, dann Netzteil | 1 ms | 500 ms | keiner | – | 0,41 A |
| 2 | erst Netzteil, dann USB | 1 ms | 500 ms | **bei 500 ms** | ia 17,2 A, ib 10,5 A, ic −27,0 A (Summe 0,7 A) | 0,66 A |

Offset I_C: 2034 counts (plausibel). FOCP war nicht aktiv (Poti-Betrieb).

**Ursache (belegt): Vermutung 1.**
- Der SW-Trip fällt in dieselbe Millisekunde wie das Löschen. Die Summe der drei Ströme ist ≈ 0, es floss also wirklich ein Strom von 27 A.
- Mechanismus: Im Poti-Betrieb ist Freemode aus. Strom- und Drehzahlregler integrieren, während der Trip die Brücke sperrt. Nach dem Löschen liegt die aufgelaufene Spannung an: di/dt ≈ 24 V / 38 µH ≈ 0,6 A/µs, also ≈ 30 A in einer PWM-Periode.
- Dass der SW-Trip nur bei Start 2 kam, erklärt sich wohl so: Wie weit die Regler auflaufen, hängt vom Rauschen und vom Offsetrest ab, und damit von der Einschaltreihenfolge.
- Der Fehler ist durch 4ca5efb sichtbar geworden. Die gleiche Gefahr bestand aber schon vorher, nämlich nach einem ACK im Poti-Betrieb. Das ACK geht allerdings nur über FOCP, und dann ist Freemode aktiv.

## Behebung

`focp_c28x_step` gibt Freemode aus:
- solange ein Trip gespeichert ist,
- bis zum Scharfschalten (500 ms),
- und wie bisher, wenn FOCP aktiv und der Antrieb nicht freigegeben ist.

Freemode setzt im Modell den Drehzahlregler (`Reset_I_Or`) und beide Stromregler (`Reset_I_Cur_And` über `RateTransition12`) zurück. Freemode von FOCP wirkt auch im Poti-Betrieb (`Freemode_Or`).

Damit startet der Antrieb nach dem Löschen aus zurückgesetzten Reglern. Ein Trip im Poti-Betrieb lässt die Regler nicht mehr auflaufen.

**Nachweise:**
- `test_protocol`: Freemode bis 500 ms und während des Trips, danach 0. Alle 5 PC-Tests bestehen.
- `foc_codegen`: 0 Warnungen.
- Simulation, Äquivalenz und MIL sind nicht betroffen, nur C im Code-Pfad.

Die Diagnosevariablen bleiben in der Firmware: 20 Wörter RAM, Schreiben nur beim ersten Trip.

**Hardware (nach Flash):** mehrere Neustarts in beiden Einschaltreihenfolgen. Erwartet: READY ohne Fehler, `diag[0]` = 0, `boot_trips` = 1.

## Hardware nach dem Flash (31131a0)

| Start | Reihenfolge | Status über CAN | Diagnose (JTAG) |
|---|---|---|---|
| 1 | erst Netzteil, dann USB | **READY, kein Fehler** (ohne ACK) | `boot_trips` 1, HW-Trip 1 ms → gelöscht 500 ms, `diag[0]` 0 (kein SW-Trip), max \|i\| 0,63 A |
| 2 | erst USB, dann Netzteil | **kein Rahmen auf dem Bus** | `boot_trips` 1, `diag[0]` 0, max \|i\| 0,41 A |

Die Trip-Behebung wirkt in beiden Reihenfolgen.

## Zweiter Befund: CAN bleibt nach dem Start mit USB zuerst im Bus-Off

JTAG ohne Reset (`host/tools/ccs_read_can_noreset.js`) nach Start 2:

```
CAN_CTL  0x00011441  Init=1 ABO=0
CAN_ES   0x000000E5  LEC=5 (Bit-0-Fehler) EPass=1 EWarn=1 BOff=1
CAN_ERRC 0x000000F8  TEC=248
```

**Mechanismus:**
- Über USB läuft der Controller schon, bevor PVDD da ist. Der CAN-Transceiver ist dann vermutlich noch nicht versorgt, und jeder Sendeversuch scheitert.
- Das Modul geht in Bus-Off. Das Blockset schaltet Auto-Bus-On nicht ein (keine Option in den Hardware-Einstellungen), deshalb bleibt das Modul bis zum Neustart im Init.
- Das erklärt auch die früheren „stummen“ Starts. Ich hatte sie vorher dem JTAG-Zugriff zugeschrieben.

**Behebung (`hal_init` in `firmware/focp_c28x.c`):**
- Auto-Bus-On mit 10 ms Wartezeit einschalten (`CAN_enableAutoBusOn`, `CAN_setAutoBusOnTime`).
- Einen schon bestehenden Bus-Off einmal mit `CAN_startModule` beenden.
- `foc_codegen` erfolgreich, 0 Warnungen. Der erste Lauf brach mit „Modell nach dem Build verändert (Dirty)“ ab. Die Modelle waren unverändert (`git status` leer), der zweite Lauf lief sauber.

**Hardware (nach Flash):** Start mit erst USB, dann Netzteil → STATUS über CAN muss kommen.
