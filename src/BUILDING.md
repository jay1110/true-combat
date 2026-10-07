# TC:E-Rekonstruktion bauen

Das Projekt baut die rekonstruierten TC:E-Module. Die Rekonstruktion ist
noch unvollstaendig. Git enthaelt Produktionsquellen und Build-Dateien;
Referenzbinaries, Tests, Rekonstruktionslisten und Belege bleiben lokal.

## Voraussetzungen

- Visual Studio 2022 mit C++-Buildtools fuer x86 und Windows SDK
- CMake ab 3.20
- Nur fuer optionale lokale Vergleichstests: Python 3, die lokalen Tests und
  unveraenderte Referenzdateien unter `tcetest/`.

Im Repository-Stamm in PowerShell:

```powershell
.\src\build.ps1
```

Ohne lokale Tests/Referenzen baut das Skript nur die Produktionsmodule.
Es kopiert nach erfolgreichem Build (und gegebenenfalls Tests) alle drei DLLs nach
`tce2/`, prueft SHA-256 und schreibt `tce2/build-info.json`. Dies gilt auch
bei spaeteren Builds. Zusaetzlich wird `tce2/zz_tce2_vm.pk3` mit cgame/ui erzeugt,
damit Pure-Clients nicht die Vanilla-DLLs aus `etmain/mp_bin.pk3` extrahieren.
Laufende Spiel-/Serverprozesse vor dem Deploy schliessen; Dateisperren werden
vor dem Kopieren geprueft. `-SkipTests` wird im Manifest als ungeprueft vermerkt.
`tce2` ist das neue Test-`fs_game`; die vollstaendige TC:E-Einbindung fehlt noch.

Die einzelnen Build-Schritte (anschliessend mit `build.ps1` nach `tce2` deployen):

```powershell
cmake -S src -B build/sdk-win32 -A Win32
cmake --build build/sdk-win32 --config Release --parallel
```

Vergleichstests sind standardmaessig deaktiviert. Nur in der lokalen
Rekonstruktionsumgebung mit Tests und Referenzdateien einschalten:

```powershell
cmake -S src -B build/sdk-win32 -A Win32 -DBUILD_TESTING=ON
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
- `cgame/tce_cg_gear.c`, `game/tce_g_load.c`: Client-/Server-Gear-Lader und
  Server-Waffenlader; Spielstart-Einbindung noch offen.
- `cgame/tce_weapon_media.c`, `.h`: rekonstruierter Medienparser-Unterbaum
  mit separater TC:E-Struktur und Originalvergleichen.
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
