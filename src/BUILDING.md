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
