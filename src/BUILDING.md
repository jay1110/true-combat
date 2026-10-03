# TC:E-Rekonstruktion bauen

Stand: 3. Oktober 2026. Das Projekt baut das ET-2.60-SDK mit ersten
rekonstruierten TC:E-Komponenten. **Es ist noch kein spielbarer Ersatz fuer TC:E.**
Den genauen Umfang beschreibt [reconstruction/STATUS.md](reconstruction/STATUS.md).

## Voraussetzungen

- Visual Studio 2022 mit C++-Buildtools fuer x86 und Windows SDK
- CMake ab 3.20
- Python 3 fuer die Vergleichstests
- Die unveraenderten Referenzdateien unter `tcetest/`, einschliesslich `pak2.pk3` und `pak3.pk3`

Im Repository-Stamm in PowerShell:

```powershell
.\src\build.ps1
```

Das Skript kopiert nach erfolgreichem Build und Tests alle drei DLLs nach
`tce2/`, prueft SHA-256 und schreibt `tce2/build-info.json`. Dies gilt auch
bei spaeteren Builds. `-SkipTests` wird im Manifest als ungeprueft vermerkt.
`tce2` ist das neue Test-`fs_game`; die vollstaendige TC:E-Einbindung fehlt noch.

Die einzelnen Build-Schritte (anschliessend mit `build.ps1` nach `tce2` deployen):

```powershell
cmake -S src -B build/sdk-win32 -A Win32
cmake --build build/sdk-win32 --config Release --parallel
ctest --test-dir build/sdk-win32 -C Release --output-on-failure
```

Ausgabe: `build/sdk-win32/bin/Release/` mit `cgame_mp_x86.dll`,
`qagame_mp_x86.dll` und `ui_mp_x86.dll`. Die Dateien in `tcetest/` werden beim
Build nicht ueberschrieben. x64 wird bewusst abgelehnt: ET benoetigt x86-DLLs.

Die alten `wolf.sln`/`.vcproj`-Dateien sind historische SDK-Dateien. Fuer den
neuen Build CMake verwenden; die alten Projekte enthalten die neuen Quellen nicht.
`-SkipTests` baut ohne Python und Referenzvergleich. `-Configuration Debug`
waehlt den Debug-Build.

Nur `src/` und `.gitignore` werden kuenftig versioniert. Nach einem Clone ohne
lokale Spieldateien `src/build.ps1 -SkipTests` benutzen. Fuer die Originaltests
muessen die eigenen Referenzdateien wieder unter `tcetest/` vorliegen.

Funktionschecklisten aktualisieren/pruefen:

```powershell
python src/tools/reconstruction_tracker.py sync
python src/tools/reconstruction_tracker.py check
```

Der Fortschritt steht dauerhaft in `reconstruction/functions.json` und bleibt
bei erneuter Inventur erhalten. Einstieg: `reconstruction/FUNCTIONS.md`.

## Quellen und Referenzen

- `cmake/sdk_sources.cmake`: Quelllisten aus den urspruenglichen VS-Projekten,
  ohne das nur fuer die VM bestimmte `bg_lib.c`.
- `ui/menudef.h`: unveraendert aus `etmain/pak0.pk3`, Eintrag `ui/menudef.h`.
  Die acht SDK-Includes zeigen jetzt auf diese lokale Kopie.
- `game/tce_bg.c`, `game/tce_bg.h`: rekonstruierte gemeinsame Funktionen und
  teilweise bekannte Datenstrukturen.
- `game/tce_weapon_parse.c`, `game/tce_gear_parse.c`: Waffen- und Gear-Parser.
- `game/tce_weapon_init.c`: Waffeninitialisierung mit Charakteristiktabelle.
- `reconstruction/evidence/`: Windows-Decompilerbelege und Waffen-ID-Zuordnung.
- `reconstruction/inventory/`: Linux-Symbole, SDK-Namensreferenzen und SHA-256
  der unveraenderten DLL-/SO-Eingaben.

Inventur erneut erzeugen:

```powershell
python src/tools/tce_inventory.py
```

Benannte Linux-Decompilerreferenzen mit installiertem Ghidra erzeugen:

```powershell
.\src\tools\export-reference.ps1 -GhidraHome 'C:\Pfad\zu\ghidra'
```

Ohne Parameter wird Ghidras `lastrun`-Datei zur Pfadsuche verwendet.
Der Export verwendet ein eigenes Projekt in `build/ghidra-reference/`, nicht
das geoeffnete Benutzerprojekt `decompiled/tce`. Ergebnisse liegen in
`build/reference/<modul>.mp.i386.so/`. Das sind Analyseartefakte mit Ghidra-Typen
und Platzhaltern, keine direkt kompilierbaren Originalquellen.
