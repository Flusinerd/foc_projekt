# Schritt 6 – Vorbereitung: Test-Harnesses auf focLib übertragen (2026-11-03)

**Nur gelesen.** `impmodel.slx` und die Testdatei wurden geladen, aber nicht gespeichert; der SHA-256 beider Dateien ist vorher und nachher gleich. Logs: `06_vorbereitung_harness.log`, `06_vorbereitung_tests.log` (nicht versioniert).

## Testdatei der MA

`C:\Users\Student\Desktop\Projekt_FU\CarpeDiem_SW\Anhang\testing\Testfile_F2838.mldatx` (außerhalb des Repos)

Struktur: `Motorcontroller_Test_Suite / Functional_Tests / Unit_Tests / Motorcontroller / …`

| Suite | Testfall | Typ | Harness | Owner in impmodel | StopTime |
|---|---|---|---|---|---|
| Freemode_Undervoltage_Temperature | `Freemode_SW_ANF_01_07_08` | **baseline** | Freemode_Check_TestHarness | Speed Control/Digital_Freemode_Check | 10 s |
| MaxVelocity | `MaxVelocity_SW_ANF_04` | baseline | MaxVel_TestHarness | Speed Control/Speed calculator | 10 s |
| SpeedController | `SpeedController_SW_ANF_05` | baseline | SpeedController_TestHarness | Speed Control/PI_Controller_Speed | 10 s |
| SafeState | `SafeState_SW_ANF_03` | baseline | SafeState_TestHarness | Speed Control/Mechanical_Freemode_Check | 10 s |

Es sind keine Anforderungen verknüpft; die IDs stehen nur im Testnamen. Der Freemode-Test deckt laut Name SW_ANF_01, 07 und 08 ab (AGENTS.md nennt nur 01).

## Harnesses (alle intern in impmodel.slx, Quelle Test Sequence, Senke Test Assessment)

### Freemode_Check_TestHarness → `Digital_Freemode_Check`
- Sequenz: Run (UV = 1) → Step1 … Step7, alle 2 s (Step6/7: 1 s). Durchläuft alle Kombinationen von `Undervoltage_Sig`, `Freemode`, `hot_temp`.
- Assessment: `verify(Freemode_Sig == 1)` bzw. `verify(Freemode_Sig == 0)`, Bedingungen in den Unterzuständen `FreemodeEnabled`/`FreemodeDisabled`.

### SpeedController_TestHarness → `PI_Controller_Speed`
- Sequenz: N_ref 0 → 1, N_fb rampt hoch (1 s), N_ref 0, N_fb rampt runter. Das Ganze einmal mit Freemode aus (FD) und einmal mit Freemode ein (FE).
- Assessment: Iq_ref ∈ [0, 1] bei positivem Moment, ∈ [−0,5, 0] bei negativem, ≤ 0 bei Freemode.
- **Bezug zu P1b:** Geprüft werden nur die Begrenzungen [−0,5, 1] und das Vorzeichen. `speed_pi` ändert diese Grenzen nicht → der Test sollte bestehen. Die **Baseline-Kriterien** (Typ baseline) schlagen dagegen sicher fehl, weil der I-Anteil jetzt wirkt.

### MaxVel_TestHarness → `Speed calculator`
- Sequenz: Für speedMode 1/2/3 jeweils Speed_Ref_PU hochrampen (1 s), halten (2 s), runterrampen (1 s).
- Assessment: N_Ref ∈ [0, 0,34] / [0, 0,67] / [0, 1].

### SafeState_TestHarness → `Mechanical_Freemode_Check`
- Sequenz: 4 Kombinationen von `Freemode_Sig_Dig` und `SafeState`, je 1 s.
- Assessment: `Freemode_Sig == true`, sobald einer der beiden Checks aktiv ist.

## Plan für Schritt 6 (wartet auf Freigabe)

Skript `matlab/tools/foc_build_harnesses.m`, gibt den Zustand vorher und nachher aus:
1. `impmodel` ohne Speichern laden, je Harness `sltest.harness.load`.
2. In `focLib` (entsperrt) mit `sltest.harness.create` je einen Harness auf dem gleichnamigen Bibliotheksblock anlegen (`'Source','Test Sequence','Sink','Test Assessment'`, gleicher Name).
3. Die Inhalte von Test Sequence und Test Assessment übertragen: Schritte, Aktionen, Übergänge und Symbole mit Datentyp (`sltest.testsequence.*`). Die Unterzustände der Assessments (GlobalAssessment.*) bleiben erhalten.
4. `focLib` speichern; die Harnesses liegen intern in `focLib.slx`.
5. `matlab/tests/focLib_MIL.mldatx` per `sltest.testmanager` anlegen: 4 Testfälle vom Typ **Simulation** (Bewertung über `verify`, ohne Baseline), Namen mit SW_ANF-IDs, StopTime 10 s.
6. Laufen lassen im Stand P1a (Tag) und im Stand P1b.1. Erwartung: alle 4 bestehen. Abweichungen werden berichtet, nicht nachgestellt.

**Entscheidungen des Menschen (2026-11-03):**
- Die Baselines der MA werden **nicht** übernommen. Stattdessen werden **neue Baselines** auf den focLib-Harnesses erzeugt. Die Testfälle sind damit vom Typ *baseline* (verify-Prüfungen plus Baseline-Vergleich). Die Baselines werden im Stand P1b.1 aufgenommen und dokumentiert. Gegen P1a laufen nur die verify-Prüfungen; ein Baseline-Fehlschlag beim SpeedController ist dort erwartet.
- Die MA-Testdatei wird **nicht** ins Repo kopiert. Sie bleibt nur Lesequelle unter `Projekt_FU/…/Anhang/testing/`.
