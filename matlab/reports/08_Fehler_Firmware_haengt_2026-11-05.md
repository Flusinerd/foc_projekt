# Fehler: Firmware läuft nach dem Flashen von 4a98dc5 nicht (2026-11-05)

## Befehl

```
matlab -batch "cd('matlab'); foc_startup('datasheet'); foc_flash('app_speed','FREIGABE')" -logfile matlab\reports\08_fix_flash.log
```

Build und Laden fehlerfrei („LOAD & RUN DONE“). Es gab nur die bekannten 4 Compilerwarnungen, keine neuen.

## Beobachtung (Mensch, nach Neustart der Platine)

- Die Heartbeat-LED blinkt nicht mehr.
- Das Poti reagiert nicht.
- FOC Studio kann sich nicht verbinden (CAN).

Der vorige Stand ca98e22 (RX-FIFO 8 tief, sonst gleich) lief auf der Hardware.

## Änderungen ca98e22 → 4a98dc5 (firmwarewirksam)

| Datei | Änderung |
|---|---|
| `firmware/focp_c28x.c` | RX-FIFO in den Objekten **17–31** (vorher 20–27), TX in **4–9** (vorher 28–31). `comm.telem_f` wird beim ersten Schritt gesetzt. |
| `protocol/include/focp/focp_params.def` | neuer Parameter `comm.telem_f` (43 statt 42) |
| `matlab/models/focLib.slx` | Totzone: Abs, Compare To Constant, OR vor dem Reset des Drehzahl-PID |

Speicher laut `app_speed.map`: RAMGS_DATA belegt 0x1302 von 0x5000, Stack 0x400. Kein Hinweis auf ein Platzproblem.

## Vermutungen (nicht geprüft)

1. **CAN-Objekte 4–9 oder 17–31 kollidieren mit dem Blockset**, z. B. mit Objekten, die es intern oder für Interrupts nutzt. Das kann Interrupts auslösen, die niemand bedient, oder einen Interrupt-Sturm.
2. **IF-Register-Konflikt:** `hal_rx` liest im 2-kHz-Task über IF2. Die eCAN-Blöcke des Blocksets nutzen IF2 im 0,2-s-Task. Mit 15 statt 8 Objekten wächst das Zeitfenster für eine Unterbrechung mitten in einer IF2-Übertragung.
3. Ein Trap oder Absturz durch etwas anderes (Stack, Parameterindex).

## Vorschlag

1. PVDD aus (Zustand der PWM unbekannt).
2. Per JTAG anhalten und PC samt Symbol lesen, ohne Neustart der Anwendung. Daran sieht man, ob die CPU in einer Fehler-ISR, einer Warteschleife oder einem Trap steht.
3. Bis zur Klärung den laufenden Stand ca98e22 wieder flashen.

## JTAG-Befund (Freigabe des Menschen, PVDD aus)

`host/tools/ccs_read_pc.js` (anhalten, PC lesen, 20 ms laufen lassen, 8 Proben):
- Nach dem Verbinden steht CPU1 im **Boot-ROM** (PC 0x3FB114, RPC 0x3FD33A), mit PIECTRL = 0, IER = 0 und CAN-Fehlerzählern 0.
- **Achtung:** Das Verbinden über CCS setzt die CPU offenbar zurück; auch `ccs_read_app_vars.js` vermerkt „CPU1 steht im Boot-ROM“ nach dem Verbinden. Der Zustand vor dem Verbinden ist damit **nicht beobachtbar**. Dieser Befund sagt über den Fehler nichts aus.

`host/tools/ccs_read_pc_start.js` (Anwendung ab `code_start` starten, 2 s laufen lassen, dann 8 Proben):
- PC pendelt zwischen 0x804F und 0x8054 = **`MW_IPC_Write` + 0x4F…0x54** (RAM-Funktion). Rücksprung (RPC) 0x8271B = **`ADCB1_INT` + 0x98** (Strom-ISR).
- IER = 0x409, PIECTRL = 0xD43: Die Anwendung hat initialisiert. CAN-A ES = 0x7 (LEC 7 = keine neue Meldung seit dem Lesen).
- Deutung: Die Strom-ISR wartet in `MW_IPC_Write` auf eine Quittung des CM-Kerns. Unter dem Debugger hält die GEL-Datei den CM in „wait boot“ („CM is out of reset and configured to wait boot“), deshalb kommt keine Quittung. Das ist **ein Effekt des Debuggers** und nicht sicher dasselbe wie der Fehler beim normalen Einschalten.

## Nächster Schritt (Vorschlag)

Eingrenzen per Flashen:
1. Aktuelles Modell (inkl. Totzone) mit den **C-Dateien von ca98e22** (FIFO 20–27, TX 28–31, ohne `comm.telem_f`). Läuft das, liegt die Ursache in der C-Änderung.
2. Sonst ca98e22 komplett. Läuft auch das nicht, liegt die Ursache im Aufbau bzw. im Neustart und nicht in 4a98dc5.

## Eingrenzung (Freigabe des Menschen)

| Versuch | Inhalt | Heartbeat nach Neustart |
|---|---|---|
| 1 | Modell 4a98dc5 (Totzone, `comm.telem_f` im Protokoll), C-Dateien von ca98e22 (FIFO 20–27, TX 28–31, ohne Setzen von `comm.telem_f`) | **blinkt nicht** |
| 2 | ca98e22 komplett (eigener Worktree `../foc_ca98e22`) | **blinkt** |

→ Die C-Änderung (Mailboxen) ist nicht die Ursache. Verbleibend: **Sollwert-Totzone** (focLib) oder **neuer Parameter** in `focp_params.def`.
| 3 | Stand 4a98dc5 ohne `comm.telem_f` (Zeile in `focp_params.def` und Setzen in `focp_c28x.c` vorübergehend entfernt) | **blinkt** |

→ Auslöser ist **der zusätzliche Eintrag in der Parametertabelle**, nicht die Totzone. Der Protokollcode hat keine Schleife und kein Feld, das von der Parameterzahl abhängt. Der Eintrag verschiebt aber die Speicheraufteilung: `.text` 0x7478 → 0x747D, `.const` 0x1164 → 0x11A6, `.bss` 0x92B → 0x92D. Alle Flash-Abschnitte beginnen 128-bit-ausgerichtet. Vermutung: ein latenter Fehler (Speicher, Zeit, Initialisierung), der durch die Verschiebung sichtbar wird.

**Warum die JTAG-Messung nichts zeigte:** `f2838x_cpu1.gel` ruft in `OnTargetConnect()` `GEL_Reset()` auf. Jedes Verbinden setzt CPU1 zurück. Neu: `host/tools/ccs/f28388D_nogel.ccxml` (Kopie ohne GEL-Dateien) und `host/tools/ccs_read_pc_noreset.js`. Geprüft an der laufenden Firmware aus Versuch 3: PC liegt in der Anwendung (0x82BF1, 0x86DCx), IER = 0x409, also kein Reset beim Verbinden.

Nächster Schritt: den hängenden Stand flashen, Platine neu starten, mit `ccs_read_pc_noreset.js` den PC lesen.

## Echter Hängezustand (ohne Reset gelesen, nach Einschalten)

Stand HEAD (4a98dc5 + Bericht) geflasht, Platine neu gestartet, `ccs_read_pc_noreset.js`:
- PC 0x804F…0x8054 = **`MW_IPC_Write`** (Warteschleife `while(*(H_PU2).mutex.ptr);`), RPC 0x8271B = **`ADCB1_INT`**. IER = 0x409.
- Die Strom-ISR blockiert also dauerhaft in einem IPC-Schreibblock zum CM-Kern. Alle niedriger priorisierten Tasks (Drehzahl, CAN, Poti, LED) stehen.

`ccs_read_ipc_noreset.js` (IPC-Steuerworte, Kanal k bei Basis + 12·k):
- CM→CPU1 (0x38000, schreibt nur der CM): alle Worte 0, also auch alle `mutex` = 0.
- CPU1→CM (0x39000): Kanäle 1–7 initialisiert (`bufLen` = 1, `dtBytes` = 4), **Kanal 0 nicht** (`bufLen` = 0, `dtBytes` = 0).
- Die Warteschleife liest also offenbar nicht die erwarteten Sperrworte. Wahrscheinlich ist der Handle des betroffenen Schreibblocks nicht (richtig) initialisiert und zeigt auf eine andere Adresse. Deren Inhalt hängt von der Speicheraufteilung ab; das erklärt, warum ein zusätzlicher Parametereintrag den Fehler auslöst. Endgültig bewiesen ist das nicht.

Hintergrund: Die IPC-Schreibblöcke stammen aus der MA (Hall-Daten, Drehzahl usw. an den CM-Kern für Ethernet-Logging über `TCP_Server.slx`, `docs/Firmware-Ueberblick_CarPediem.md`). Auf dem CM läuft in diesem Aufbau kein Programm. Die Ethernet-Patches des Blocksets fehlen. Schon die MA nennt als offenen Punkt „IPC-Daten werden permanent gesendet, auch ohne Ethernet-Verbindung“. Unbenutzte IPC-Schreibblöcke mit Warteschleife in der Strom-ISR sind unabhängig von der genauen Ursache ein Risiko.

## Vorschlag zur Behebung

IPC-Schreibblöcke (CPU1→CM) aus `app_speed`/`focLib` per Skript entfernen oder über eine Variante abschalten. Die Telemetrie übernimmt FOCP über CAN. Danach erneut mit und ohne `comm.telem_f` prüfen.

## Behebung (Entscheidung des Menschen: entfernen)

`tools/foc_remove_ipc_cm.m` (Log `08_remove_ipc.log`): vorher **8** IPC-Write-Blöcke, nachher **0**. Ersetzt durch Terminatoren:
- focLib: `Input Controller/Subsystem/TCP_temp`, `Speed Control/TCP_SpeedRef`, `Torque Controller/TCP_Position`, `Torque Controller/TCP_Speed`
- app_speed: `IPC Write for eCAP INTs/TCP_HallA`, `_HallB`, `_HallC`, `_HallState`

| Prüfung | Ergebnis |
|---|---|
| `foc_equivalence_p1a` (legacy) | Abweichung **0** |
| MIL (datasheet) | **4/4** |
| `foc_codegen` (mit `comm.telem_f`) | erfolgreich, **0 Compilerwarnungen**. Alle 4 bisherigen Warnungen hingen an den IPC-Blöcken, inkl. „missing return in `MW_IPC_Init`“. Kein `MW_IPC_Write` mehr im Code, `CPU1_CM_IPC` nicht mehr definiert. |

Hardware: noch offen.
