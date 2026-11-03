# Programmsymbol

Erzeugte Dateien – nicht von Hand bearbeiten. Quelle ist `docs/img/logo/carpediem_mark.svg` (Bildmarke) bzw. `carpediem_logo.svg` (mit Schriftzug).

Neu erzeugen, nachdem das Logo geändert wurde:

```bash
cmake -S . -B build -DFOC_BUILD_TOOLS=ON
cmake --build build --target svgicon
build/svgicon docs/img/logo/carpediem_mark.svg host/studio/icons/carpediem --ico host/studio/icons/carpediem.ico 16 24 32 48 64 128 256
build/svgicon docs/img/logo/carpediem_logo.svg docs/img/logo/carpediem_logo --width 1200
```

(Unter Windows muss `C:\Qt\6.12.0\msvc2022_64\bin` im `PATH` liegen.)

| Datei | Verwendung |
|---|---|
| `carpediem.ico` | exe-Symbol (`focstudio.rc`), NSIS-Installer |
| `carpediem_*.png` | Fenstersymbol (`QApplication::setWindowIcon`) |
| `docs/img/logo/carpediem_logo.png` | „Über“-Dialog, README |
