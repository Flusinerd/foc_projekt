# FOC Studio – Installation

## 1. Für Anwender (Windows 10/11, 64 bit)

1. `FOCStudio-<Version>-win64.exe` ausführen. Der Assistent zeigt die Lizenzen, legt einen Startmenü-Eintrag an und ersetzt eine ältere Version. Ohne Installation geht es auch: `FOCStudio-<Version>-win64.zip` entpacken und `bin\focstudio.exe` starten.
2. **CAN-Adapter:**
   - **CANable / candleLight** (Firmware candleLight, Geräte-Manager zeigt „canable gs_usb“ unter *USB-Geräte*): kein Treiber nötig, Windows bindet WinUSB automatisch.
   - **PEAK PCAN-USB:** PEAK-Treiber installieren (liefert `PCANBasic.dll`).
   - CANable mit **slcan**-Firmware (erscheint als COM-Port) wird nicht unterstützt. Auf candleLight umflashen.
3. Start: *Antrieb → Verbinden …* (Strg+K), Schnittstelle, Adapter, Bitrate (Standard 500 kbit/s) und Knoten wählen, **Verbindung testen**, dann **Verbinden**. Die Einstellungen werden gespeichert.
4. Ohne Hardware: *Antrieb → Demo starten* verbindet mit dem virtuellen Antrieb.

**Sprache:** folgt der Windows-Sprache; umstellbar unter *Ansicht → Sprache* (wirkt nach Neustart).

**Logdatei:** `%LOCALAPPDATA%\Hochschule Bochum\FOC Studio\logs\focstudio_<Datum>.log` (Ordner über *Ereignisse → Log-Ordner öffnen*).

**Einstellungen** (letzte Verbindung, Sprache) liegen in der Registry unter `HKCU\Software\Hochschule Bochum\FOC Studio`.

### Bedienung in Kürze

| | |
|---|---|
| **STOPP** | Knopf oben rechts, **Leertaste** oder **Esc** – in jedem Fenster des Programms. Deshalb lösen Leertaste und Esc keine anderen Knöpfe aus; dafür gibt es die unterstrichenen Buchstaben (Alt + Buchstabe) und Enter in Dialogen. |
| Freigeben | Nur im Zustand „Bereit“ mit gewähltem Regelmodus. Vor der ersten Freigabe an echter Hardware fragt ein Sicherheitsdialog vier Punkte ab (Netzteil strombegrenzt, Welle frei, SafeState-Taster griffbereit, Spannung im Blick). |
| Expertenmodus | *Ansicht → Expertenmodus* mit Rückfrage: Parameter ändern/importieren, Scope, Kalibrierungen, Parameter des virtuellen Antriebs. |

## 2. Für Entwickler: bauen

Voraussetzungen (Windows):

| Werkzeug | Version | Hinweis |
|---|---|---|
| Visual Studio 2022 Build Tools | 17.x | Workload „Desktopentwicklung mit C++“ (MSVC, Windows SDK, CMake, Ninja) |
| Qt | 6.12 (≥ 6.7), **MSVC 2022 64-bit** | Qt Online Installer. Die MinGW-Variante passt nicht zu MSVC. |
| NSIS | 3.x | nur für den Installer (`winget install NSIS.NSIS`) |
| Git | | FetchContent lädt Qwt, ImGui, ImPlot, GLFW |

In einer „Developer PowerShell for VS 2022“ (oder nach `vcvars64.bat`):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build build
ctest --test-dir build --output-on-failure
```

Starten aus dem Build-Ordner: `C:\Qt\6.12.0\msvc2022_64\bin` muss im `PATH` liegen (oder das ZIP-Paket verwenden).

```bash
build\host\studio\focstudio.exe --demo
```

CMake-Optionen: `FOC_BUILD_GUI` (Qt-Anwendung `focstudio`, Standard EIN), `FOC_BUILD_DEV_GUI` (ImGui-Entwicklerwerkzeug `focstudio-dev`, Standard EIN), `FOC_BUILD_TESTS`. Ohne Oberflächen, z. B. für CI: `-DFOC_BUILD_GUI=OFF -DFOC_BUILD_DEV_GUI=OFF`.

**Linux:** Qt 6 (≥ 6.7) aus den Paketquellen oder dem Qt-Installer, sonst wie oben. Der CANable erscheint dort über den Kernel-Treiber `gs_usb` als SocketCAN-Schnittstelle (`sudo ip link set can0 up type can bitrate 500000`), in FOC Studio „SocketCAN“ wählen. *Stand 2026-11-03: Der Linux-Build der Qt-Oberfläche ist noch nicht geprüft (kein Linux-System verfügbar).*

## 3. Installer erzeugen

```bash
cmake --build build
cd build
cpack
```

Ergebnis in `build/`: `FOCStudio-<Version>-win64.exe` (NSIS) und `FOCStudio-<Version>-win64.zip`. Die Version kommt aus `project(VERSION …)` im obersten `CMakeLists.txt`.

Was ins Paket kommt (`host/studio/CMakeLists.txt`, `cmake/Packaging.cmake`):

- `bin/`: `focstudio.exe`, Qt-DLLs und `qt.conf` (über `qt_generate_deploy_app_script` → `windeployqt`), MSVC-Laufzeit app-lokal (`InstallRequiredSystemLibraries`, kein vc_redist nötig)
- `plugins/`: Qt-Plattform-, Stil- und Bild-Plugins
- `licenses/`: `LICENSES.md`, Qt- und Qwt-Lizenz

Übersetzungen sind in die exe eingebettet. Prüfen, ob das Paket vollständig ist: ZIP entpacken und `bin\focstudio.exe --demo` auf einem Rechner **ohne** Qt im `PATH` starten.

## 4. Übersetzungen pflegen

```bash
cmake --build build --target update_translations
C:\Qt\6.12.0\msvc2022_64\bin\linguist.exe host\studio\translations\focstudio_de.ts
```

`update_translations` (lupdate) übernimmt neue `tr()`-Texte in die `.ts`-Dateien. Quellsprache im Code ist Englisch; `focstudio_en.ts` enthält nur die Pluralformen. Beim normalen Build werden die `.qm`-Dateien erzeugt und eingebettet; die Qt-eigenen Texte (Standarddialoge) werden dabei hineingemischt.
