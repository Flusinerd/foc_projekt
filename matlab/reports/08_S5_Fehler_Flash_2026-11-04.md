# Schritt 8 / S5: Ungewollter Download beim Codegenerieren (2026-11-04)

## Was passiert ist

Auftrag des Menschen: Für den ersten Motorlauf den Stand **P1b.1 mit `datasheet`** vorbereiten. Gemeint war nur Code generieren; eine Freigabe zum Flashen gab es dafür nicht.

Befehl (PowerShell, ohne Sandbox):

```
matlab -batch "cd('matlab'); foc_startup('datasheet'); load_system('models/app_speed.slx'); ... slbuild('app_speed'); ..." -logfile matlab\reports\08_S5_build_datasheet.log
```

Ausgabe (Ende):

```
C28xx_CPU1: GEL Output:
 CM is out of reset and configured to wait boot.
*** Program is running.
*** Disconnecting from target...
*** LOAD & RUN DONE.
### Successful completion of build procedure for: app_speed
Rebuild Reason: Global variable PI_params changed.
```

**`slbuild` hat die Firmware über JTAG auf die Platine geladen und gestartet.** Das verstößt gegen AGENTS.md Regel 4.

## Ursache (Vermutung)

In `app_speed.slx` ist als *Build action* gespeichert: **Build, load and run**. Bei Schritt 7 und beim Takt-Umbau wurde sie nur im Speicher auf „Build“ gesetzt (`08_Takt_ext25_2026-11-03.md`). Für das Flashen in S1 brauchte es den Download. Diesmal hat der Agent die Build action vor `slbuild` nicht im Speicher zurückgesetzt.

## Zustand der Hardware (unbekannt)

- Auf CPU1 läuft jetzt `app_speed` mit **P1b.1-Reglern und `datasheet`-Parametern** (Kp_i 1,03 PU, I_base 56 A, Iq_min 0).
- Ob PVDD anliegt und ob der Motor angeschlossen ist, weiß der Agent nicht. Stand bei S3: PVDD ≈ 20 V, Motor ab.
- Nach einem Laden über CCS 21 wird der Takt nicht sauber hergestellt (AGENTS.md §5). Die Firmware sollte deshalb neu gestartet werden, bevor ihr Verhalten bewertet wird.

## Vorschlag

1. Mensch: PVDD ausschalten bzw. prüfen, dass der Motor abgeklemmt ist.
2. Build action in `app_speed.slx` dauerhaft auf **Build** setzen, per Skript mit Ausgabe vorher/nachher. Flashen nur noch ausdrücklich über einen eigenen Befehl.
3. Die Codeprüfung für S5 ohne Download wiederholen.

## Nachtrag: Maßnahmen und Codeprüfung (Entscheidung des Menschen: Option C)

Hardware nach dem Vorfall laut Mensch: Platine aus, Netzteil aus, kein Motor.

### Absicherung

| Skript | Zweck |
|---|---|
| `tools/foc_build_action.m` | Build action dauerhaft setzen (Standard `Build`), Ausgabe vorher/nachher, nur dieser Wert darf sich ändern |
| `tools/foc_codegen.m` | Code erzeugen; Abbruch vor `slbuild`, wenn BuildAction ≠ `Build` oder ExtMode ≠ `off` |
| `tools/foc_flash.m` | Flashen nur mit `foc_flash(model, 'FREIGABE')`; „Build, load and run“ nur im Speicher, Modell danach ohne Speichern geschlossen |
| `tools/foc_compare_profiles.m` | Profile `legacy_f2838` und `datasheet` feldweise vergleichen |

`foc_build_action()` (Log `08_S5_codegen.log`):

```
vorher:  Runtime.BuildAction=Build, load and run, DeviceID=F28386D, FlashLoad=1, UseInternalOsc=0, OSCCLK=25, PLLCR=16, ClosestCpuClock=200, ExtMode=off
nachher: Runtime.BuildAction=Build,               (alle übrigen Werte unverändert)
app_speed gespeichert.
```

`.gitignore`: Build-Ausgaben `matlab/*.elf|*.hex|*.out`, `matlab/MW_c28xx_pie.h`.

### Codegenerierung P1b.1 / `datasheet` (ohne Download)

`foc_startup('datasheet'); foc_codegen()`: erfolgreich, „Code generated and compiled“, **kein Laden**. `test_config` (datasheet): ALL PASSED. Compilerwarnungen wie bei den früheren Builds: 3 × „used before its value is set“ in `app_speed.c`, 1 × fehlendes `return` in `mw_soc_c2000_ipc.c` (Blockset).

Werte im generierten Code (`app_speed_data.c`, fixdt(1,32,17): 131072 = 1,0):

| Größe | Wert im Code | Profil `datasheet` |
|---|---|---|
| Kp Stromregler d/q | 1106836583 / 2³⁰ = 1,031 | 1,0308 ✔ |
| Ki Stromregler d/q | 1271641295 / 2²¹ = 606,4 | 606,37 ✔ |
| Grenzen Stromregler | ±131072 = ±1 PU (13,9 V) | ✔ |
| Kp Drehzahlregler | 1632553857 / 2³² = 0,380 | 0,3801 ✔ |
| Ki Drehzahlregler | 1602756001 / 2²⁹ = 2,985 | 2,9854 ✔ |
| Grenzen Drehzahlregler (i_q,ref) | **[0, 1] PU = [0, 56 A]** | Iq_min = 0 ✔ |
| Reset I-Anteil im Freemode | `true` | reset_I_freemode = 1 ✔ |

### Befunde für S5

1. **Obergrenze i_q,ref = 1 PU = 56 A.** Mit I_base = 56 A darf der Drehzahlregler bis zu 56 A anfordern. Das liegt über dem Motornennstrom (17,6 A) und dem Hardware-Grenzwert des EVM in der Konfiguration (`I_max_hw` 30 A). Im Altprofil war 1 PU = 17,6 A. Begrenzt wird bisher nur durch das Netzteil. Vorschlag: Obergrenze analog A2 als Parameter (`ctrl_limits.Iq_max`, für die ersten Läufe z. B. 0,1 PU ≈ 5,6 A).
2. **Hall 000/111** führen in den Zweig „Bad hall (glitch or wrong connection)“: Drehzahl-Gültigkeit 0, Winkel aus der Konstante „Hall Value of 7“. **Kein Freemode.** Die Bedingungen für Freemode sind nur Freemode-Anforderung, Übertemperatur und Unterspannung. Mit ungültigen Hall-Signalen bleibt die Stromregelung also an (vgl. S3).
3. Geflasht ist weiterhin der ungewollt geladene Stand (dieselben Parameter wie oben, ohne erneute Prüfung auf der Hardware).
