# TC:E-Rekonstruktion bauen

## Mapbasierter Waypoint-Generator (in Entwicklung)

Der Standard benoetigt **keine WPS-Datei und keine vorhandenen Waypoints**.
`tce_navgen` liest die aktuell geladene BSP-Datei und verwendet Spielerspawns,
Bodenflaechen und Leiterflaechen als Geometriequellen. Ein gemeinsames Raster
verhindert ueberlagerte Punktketten. Neue Wege werden gegen die reale TCE-
Spielerkollision geprueft. Spawnverbundene Bereiche haben Vorrang; isolierte
Geometrie wird gesondert ausgewiesen. Sprung-/Leiterkandidaten erhalten native
Omni-bot-Flags, gelten jedoch erst nach einem Bot-Probelauf als live bestaetigt.

Die Erkundung verwendet ein feines Suchnetz; der Export vereinfacht es danach
ueber kollisionsgepruefte Ersatzverbindungen. Enge Tueren zwischen Rasterlinien
werden gezielt mit feineren Suchschritten untersucht. Unverbundene Punkte werden
nicht exportiert, bleiben jedoch mit Position und Fehlergrund im Bericht.
Der Audit zaehlt ueberlappende Ankunftsradien, isolierte Punkte und Abbruchlimits.
Ein ausgeschlossener Spawn oder ein unentdeckter Raum ist dadurch nicht behoben.

Lange Leitern verwenden vier eigene `climb`-Fuehrungspunkte mit getrennten
Auf-/Abstiegsverbindungen nach dem Omni-bot-Waypointing-Schema. Der innere
Fuehrungspunkt ist ein Steuerungsziel, keine begehbare Position im Mauerwerk.
Punktradien werden einzeln gespeichert und gegen den JSON-Bericht geprueft.
Kollisionsnachweis und erfolgreicher Bot-Durchlauf bleiben zusaetzlich notwendig.

Nur auf einem leeren Testserver ausfuehren: die begrenzte Berechnung laeuft
synchron. Ergebnis: `navgen/<map>_auto.way` und JSON-Bericht im Schreibverzeichnis
`fs_homepath/tce2`. Der Bericht trennt Dateigueltigkeit, Erreichbarkeit,
Flaechensuch-Abbruch, isolierte Geometrie und ungepruefte Spezialwege.
Ein erfolgreicher Dateicheck bedeutet keine vollstaendige spielbare Map.

Python 3 und ein lokal erreichbares Server-Schreibverzeichnis werden benoetigt.
Der Testserver muss die gewuenschte Map mit der aktuellen qagame geladen haben.
Fuer Custom-Maps vorher `set sv_official 0` setzen.

```powershell
$env:TCE_RCON_PASSWORD = "DEIN_TESTSERVER_RCON_PASSWORT"
python src/tools/tce_navgen.py generate build/navgen-overkill/tce2 --map obj_overkill --port 27986
python src/tools/tce_navgen.py inspect build/navgen-overkill/tce2/navgen/obj_overkill_auto.json
```

`generate` installiert nichts automatisch. Exitcode 0 bedeutet bestandene
Graph-/Suchpruefungen, 2 verbleibenden Pruefbedarf, 1 einen Eingabe-/Serverfehler.
Die erzeugte Datei kann fuer Tests als `<map>_auto.way` ins Omni-bot-nav-
Verzeichnis kopiert und mit `bot waypoint_load _auto` geladen werden.
`install REPORT NAV_DIRECTORY --install` uebernimmt ein Ergebnis mit Backup;
getrennte Graphen werden standardmaessig abgelehnt. Standardnavigation und
Originalassets werden bei der Generierung nicht veraendert.

Optionales Camping: `vendor/omni-bot/tce2-scripts/goals/goal_tce2_roamcamp.gm`
nach `omni-bot/et/scripts/goals/` kopieren. `set omnibot_tce_roamcamp 1` aktiviert
kurze Aufenthalte an generierten `tce2_camp_*`-Punkten; `0` deaktiviert sie.
`set omnibot_tce_roamcamp_debug 1` protokolliert Anfahrt, Ankunft und Abbruch.
Kampf und hoeher priorisierte Aufgaben haben Vorrang. Deckung ist eine
Geometrieheuristik, keine garantierte taktisch gute Position.

Verteilte Erkundung: `goal_tce2_explore.gm` aus demselben Quellverzeichnis
nach `omni-bot/et/scripts/goals/` kopieren. `omnibot_tce_explore 1` aktiviert
die Auswahl verschiedener erreichbarer Laufziele mit teamweisen Reservierungen
und Besuchshistorie; `0` schaltet sie ab. Standard-Objectives und Kampf behalten
Vorrang. Individuelle Map-Skripte mit Prioritaeten unter 0.115 koennen dagegen
unterbrochen werden. Das ersetzt weder fehlende Verbindungen noch Objective-AI.
Die mitgelieferte `goal_checkstuck.gm` ersetzt blindes Springen bei Teamstaus
durch kurzzeitiges Warten oder mit Spielerhuelle/Boden gepruefte Ausweichschritte.
`omnibot_tce_explore_debug 1` protokolliert Ziele, Ergebnisse und Ausweichen;
im normalen Betrieb auf `0` lassen. Vollstaendige Engpassfreiheit ist unbewiesen.

Nur fuer ausdruecklich gewuenschten Altbestand existiert `generate ... --wps
PFAD`: WPS sind optional und werden standardmaessig weder gelesen noch gebraucht.

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

NavStudio und Wegpunktbearbeitung sind standardmaessig aus den Spielmodulen
ausgeschlossen (`FEATURE_NAVSTUDIO=OFF`). Die normale Omni-bot-Spielanbindung
bleibt enthalten. Lokale Editor-DLLs werden separat mit
`FEATURE_NAVSTUDIO=ON` gebaut und duerfen nicht auf oeffentliche Server.

Beide Windows-Varianten ohne Installation oder Spielstart erstellen:

```powershell
python src/build_windows_variants.py --variant both
```

Die getrennten Pakete liegen unter `dist/TCE2-Windows-x86/Game/` und
`dist/TCE2-Windows-x86/NavStudio/`, dazu je eine ZIP und ein SHA256-Manifest.
Mit `--architecture x64` werden stattdessen die 64-Bit-Module erstellt.
Jede Variante hat einen eigenen Buildcache. Das Editor-UI-PK3 wird nur dem
NavStudio-Paket beigelegt. Vor einem Variantenwechsel alte DLL-PK3-Overrides
und das Editor-UI-PK3 aus dem Mod-Suchpfad nehmen. Vorhandene Spielassets,
Maps und der Omni-bot-Kern werden nicht dupliziert.

Ausgabe: `build/sdk-win32/bin/Release/` mit `cgame_mp_x86.dll`,
`qagame_mp_x86.dll` und `ui_mp_x86.dll`. Die Dateien in `tcetest/` werden beim
Build nicht ueberschrieben. Das originale ET benoetigt x86-DLLs;
x64-Module sind fuer passende 64-Bit-ETLegacy-Engines vorgesehen.

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

## Linux i386

CMake builds all three production modules for Linux as 32-bit ELF shared objects.
A native Linux build needs CMake, C/C++ compilers, make/Ninja and 32-bit libc/C++ development libraries:

```sh
cmake -S src -B build/linux-i386 -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/linux-i386 --parallel
```

Alternatively cross-compile from Windows using Zig 0.13.0 and Ninja (no WSL required):

```powershell
cmake -S src -B build/linux-i386 -G Ninja "-DCMAKE_TOOLCHAIN_FILE=cmake/linux-zig-i386.cmake" "-DTCE_ZIG_EXECUTABLE=C:/path/to/zig.exe" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/linux-i386 --parallel
```

Use separate build directories when switching compilers. The Zig toolchain targets
x86-linux-gnu with glibc 2.17. Outputs in `build/linux-i386/bin`:
`cgame.mp.i386.so`, `qagame.mp.i386.so`, `ui.mp.i386.so`.
Copy these into the Linux installation's `tce2` directory. They cannot be loaded
by the Windows engine. Local original-binary comparison tests remain Windows-only.
Linux uses existing portable implementations where reconstructed x87 assembly is
MSVC-only; compilation does not establish original behavior or complete porting.
The Linux link binds internal symbols locally, including direct calls in GNU x87
assembly, and rejects unresolved symbols.

## Linux client package (2026-10-08 correction)

Deploy bin/zz_tce2_linux_vm.pk3 alongside all three loose .so files in the active
tce2 directory. The normal Linux build now produces this package automatically.
It contains cgame.mp.i386.so and ui.mp.i386.so; qagame remains a loose server module.
ET: Legacy's FS_CL_ExtractFromPakFile searches archives and replaces differing loose
client modules. Without Linux entries in the mod archives it can extract the SDK
client modules from etmain/mp_bin.pk3 into the tce2 home directory. A Windows-only
zz_tce2_vm.pk3 does not protect Linux clients. Keep both platform packages.
The existing 837 Linux modules were packaged without rebuilding on 2026-10-08;
this fixes delivery, not pending Linux/Windows reconstruction parity.

## Optional Omni-bot backend (built, not runtime validated)

CMake FEATURE_OMNIBOT (default ON) adds the ETmain-compatible C++ bridge and
matching 0.93 loader to qagame. Runtime omnibot_enable defaults to0, so existing
servers keep their internal bots until explicitly switched. Set
-DFEATURE_OMNIBOT=OFF for the previous C-only server build. Linux links libdl;
the Zig i386 toolchain now declares both C and C++ compilers.

The bot runtime is a separate library, built from this repository (no dependency
on the adjacent ETLegacy checkout):

    cmake -S src/vendor/omni-bot/source/Omnibot -B build/omnibot -DOMNIBOT_ET=ON -DOMNIBOT_RTCW=OFF -DOMNIBOT_STD_FILESYSTEM=ON
    cmake --build build/omnibot --config Release

Use a32-bit toolchain for both modules. For Visual Studio add -A Win32 to configure;
for a Linux cross-build use the same absolute linux-zig-i386.cmake toolchain path
and TCE_ZIG_EXECUTABLE argument as the game build. Supply
-DOMNIBOT_BOOST_INCLUDEDIR=<directory-containing-boost> when Boost headers are not
found automatically. Runtime requires C++17 in STD_FILESYSTEM mode; compiled Boost
libraries are then unnecessary. The runtime must be configured separately because
its vendored CMake uses CMAKE_SOURCE_DIR for dependency paths.

Place omnibot_et.dll (Win32) or omnibot_et.so (Linuxi386) beside global_scripts/
and et/ in the active tce2/omni-bot directory. The local runtime script/nav data
have been copied there. Windows/Linux runtime builds were requested on 2026-10-08;
see the local build manifest for deployed binaries. Transfer the entire
runtime directory to the Linux host. A Git source checkout needs a matching0.93
runtime data distribution; local navigation/runtime assets are intentionally not
published under the source-only repository policy.

Set omnibot_enable 1 before loading a map (it is latched). Empty omnibot_path uses
fs_homepath/fs_game/omni-bot, then fs_basepath/fs_game/omni-bot. An explicit path
selects a custom installation. Example local config: tce2/omnibot-example.cfg.
Server console command: bot addbot 1 1 OmniOne. In Omni-bot mode the old bare
addbot/kickbot commands are rejected to prevent mixing bot controllers; use bot
subcommands instead. Disable omnibot_enable and reload the map to restore the
internal backend.

This is new optional functionality, not a reconstructed TC original function.
Windows/Linux compilation was subsequently requested; no game launch or behavior
test was run for this integration. The bridge
uses native TC gear-slot IDs, TC-owned inventory and isolated TC weapon profiles;
profiles are AI heuristics; TC bomb/flag/VIP objective AI remains incomplete.
No obj_* TC navigation files are included. Map navigation and TC objective/weapon
scripts are required before claiming a playable replacement. Runtime ET-only
behaviors and visual waypoint drawing still need TC-specific adaptation.

### TC Omni-bot loadouts (2026-10-09)

Deploy src/vendor/omni-bot/tce2-scripts contents into omni-bot/et/scripts alongside
its matching rebuilt runtime and qagame. These native weapon-ID scripts must not
be mixed with the older compatibility-category binaries. The local Windows
installation is updated; Linux needs a matching rebuild before this script update.
Runtime binaries alone do not include these scripts.

Omni class IDs: 1 Assault, 3 Recon, 5 Sniper; team IDs remain 1/2.
`bot tceclass BotName RECON` selects the next spawn class; AUTO restores balancing.
`bot tceweapon BotName TH7` selects a primary slot; `SH1 secondary` a sidearm.
The active gear file, team, class rating, heavy quota and sidearm compatibility
remain authoritative. Rejected choices do not grant gear or override restrictions.

## Lua-Scripting (Branch lua)

`FEATURE_LUA=ON` baut Lua5.4.9 statisch in qagame ein. Ohne `lua_modules`
werden keine Scripts geladen. Serverbeispiel und API-Umfang: [lua/README.md](lua/README.md).
`src/lua/example.lua` nach `tce2/lua/example.lua` kopieren und vor Mapstart
`set lua_modules "lua/example.lua"` setzen. `lua_status` zeigt die Module.
Clientmodule brauchen kein Lua. Omni-bot/GameMonkey bleiben unabhaengig.
Mit `-DFEATURE_LUA=OFF` ist der optionale Hookpfad deaktiviert.
