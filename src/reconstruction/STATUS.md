# Stand der TC:E-Rekonstruktion

Datum: 2026-10-03. **Gesamtziel noch nicht erreicht.** Die drei erzeugten DLLs
sind weiterhin das ET-SDK mit separat rekonstruierten TC:E-Komponenten.
Die neuen Funktionen sind im CMake-Build enthalten, aber noch nicht in den
gesamten Spielablauf eingebunden; unbenutzte Funktionen kann der Linker entfernen.
Es wurde weder ein vollstaendiger TC:E-Neubau noch ein erfolgreicher Spielstart
mit diesen DLLs nachgewiesen. Originaldateien in `tcetest/` bleiben erhalten.

Verbindliche Funktionsliste: [FUNCTIONS.md](FUNCTIONS.md), bearbeitbare Daten
in `functions.json`. Arbeitsjournal und naechster Schritt: [WORKLOG.md](WORKLOG.md).
Arbeitsanweisung fuer folgende Sitzungen: [../AGENTS.md](../AGENTS.md).
10.553 bekannte interne Einstiegspunkte aller sechs Module sind erfasst;
14 Windows-Einstiegspunkte sind verifiziert, ihre Integration bleibt offen.

## Nachgewiesen

- BG_ParseGearDef: 95 Originalvergleiche mit sechs echten Gear-Dateien,
  allen 6152 Strukturbytes, Rueckgabewerten, Fallback-Reihenfolge und Freigaben.

- BG_InitializeWeaponDef und die 30x28-Charakteristiktabelle rekonstruiert:
  56.606 synthetische und 38 echte Waffen-Spezifikationen verglichen, jeweils
  kompletter 460-Byte-Zustand und Rueckgabewert. Integration bleibt offen.
- Build-Skript legt die drei DLLs nach erfolgreichen Tests in `tce2/` ab;
  SHA-256-Abgleich und `tce2/build-info.json` dokumentieren den Teststand.

- Release-Build aller drei x86-DLLs mit MSVC 19.39 / VS 2022 erfolgreich.
- 492.685 Vergleiche gemeinsamer Funktionen mit der originalen Windows-cgame-DLL.
  Darin neu: 408.170 Faelle fuer Waffenverfuegbarkeit und 85 Waffen-ID-Pruefungen.
- 54 Waffenparser-Faelle: 38 Original-`.specs`-Dateien aus `pak2.pk3` plus
  16 synthetische Normal-/Fehlerfaelle. Rueckgabewert, alle 460 Bytes der
  Zielstruktur und Anzahl der Parserfreigaben stimmen mit der DLL ueberein.
- Tests starten keinen Spielclient. Die DLL wird in einem separaten
  x86-Testprozess geladen; benoetigte Daten werden nur dort veraendert.
- Tests pruefen vor dem Laden SHA-256 der DLL. Funktionsadressen gelten nur
  fuer die erfasste Version, nicht fuer beliebige TC:E-Binaries.

Die Parser-Vergleiche verwenden einen gemeinsamen vereinfachten Tokenlieferanten
und die urspruenglichen `PC_*`-Hilfsfunktionen. Sie pruefen die rekonstruierte
Parserlogik und Strukturbelegung, nicht den vollstaendigen Engine-Praeprozessor.
Im Modulbuild werden die `PC_*`-Funktionen des SDK verwendet. Diagnoseausgaben
aus negativen Testfaellen sind erwartet.

## Rekonstruierte Funktionen

Adressen beziehen sich auf die bevorzugte Imagebase `0x30000000` der Windows-DLL.

| Originalfunktion | Windows-Adresse | Quelle |
|---|---|---|
| BG_WolfClassToTCE | 0x30008250 | game/tce_bg.c |
| BG_WeapIDToWeaponNum | 0x30008290 | game/tce_bg.c |
| BG_CheckUTWeapon | 0x300066a0 | game/tce_bg.c |
| BG_FiremodeWeapon | 0x30006700 | game/tce_bg.c |
| BG_SidearmAvailableForPrimary | 0x300081e0 | game/tce_bg.c |
| BG_GrenadeSelectionForPrimary | 0x30008220 | game/tce_bg.c |
| BG_WeapToWeaponOnBack / BG_WeaponOnBackToWeap | 0x30006770, gemeinsame Identitaetsfunktion | game/tce_bg.c |
| BG_BBoxCollision | 0x30006900 | game/tce_bg.c, vorerst TCE_BG_BBoxCollision |
| BG_FootstepForSurface | 0x30006950 | game/tce_bg.c, vorerst TCE_BG_FootstepForSurface |
| BG_ParseWeaponDef | 0x30007390 | game/tce_weapon_parse.c |
| BG_WeaponInWolfMP | 0x30003900 | game/tce_bg.c, vorerst TCE_BG_WeaponInWolfMP |
| BG_WeaponIsAvailable | 0x30006650 | game/tce_bg.c |
| BG_ParseGearDef | 0x30007b90 | game/tce_gear_parse.c |
| BG_InitializeWeaponDef | 0x30006a50 | game/tce_weapon_init.c |

Die `TCE_`-Praefixe vermeiden Kollisionen mit vorhandenen SDK-Funktionen.
Insbesondere die TC:E-Fussschrittnummern duerfen erst nach Wiederherstellung
der Soundtabellen in den Client eingebunden werden. `BG_BBoxCollision` stimmt
bereits im Verhalten mit dem SDK ueberein; es ist kein neu entdecktes Modfeature.
Die beiden WeaponOnBack-Funktionen sind im benannten Linux-Modul getrennte
Identitaetsfunktionen; Windows verwendet den identischen Funktionskoerper.

`weaponDef`: 64 Eintraege zu 0x1cc Bytes. Parserfelder, Gewichtsfeld und
Feuermodusfelder sind lokalisiert; unbekannte Felder bleiben ausdruecklich
als Bytebereiche oder Offsetnamen markiert. `loadoutWeight` ist ein beschreibender rekonstruierter
Name, kein nachgewiesener Originalbezeichner. `gearDef` (0x1808 Bytes) ist anhand
des Linux-Parsers skizziert, noch nicht im Windows-Spielablauf verifiziert.

Weitere belegte Funktionsnamen wurden in der geoeffneten Windows-cgame-Ghidra-Analyse
gesetzt und gespeichert. `BG_WeaponIsAvailable` und die TC:E-Waffen-Whitelist
sind portiert und gegen die Windows-DLL verifiziert. Linux-Decompilate liefern
zusaetzliche Namens-/Strukturbelege, aber noch keinen Linux-Laufzeittest.
Die Referenzen in `evidence/` sind bewusst `.c.txt`.

## Quellen-Repository

Neue Commits enthalten nur `src/` und die Root-`.gitignore`. Bisher verfolgte
Assets/Binaries werden aus dem Git-Index entfernt, bleiben lokal erhalten.
Diese Umstellung ist im Index vorgemerkt: 326 alte Pfade entfernt, Quellen
hinzugefuegt. Ein isolierter Build mit ausschliesslich `src/` wurde erfolgreich
geprueft. Die lokale Root-`AGENTS.md` verweist auf die versionierte Arbeitsanweisung.
Alte Commits enthalten weiterhin die bisherige Historie; keine Bereinigung der
Historie, kein Commit/Push vorgenommen. Build ohne lokale Referenzen ist mit
`-DBUILD_TESTING=OFF` moeglich; Originalvergleiche benoetigen weiterhin `tcetest/`.

## Umfang der Referenzen

| Modul | Definierte ELF-Funktionen | Namen ohne SDK-Referenz |
|---|---:|---:|
| qagame | 2375 | 159 |
| cgame | 1828 | 96 |
| ui | 836 | 31 |

Die letzte Spalte ist **keine Restaufwands- oder Fortschrittsmessung**:
gleichnamige Funktionen koennen stark veraendert sein; die Liste enthaelt auch
Laufzeitfunktionen. Erfasste Datenobjekte: 520 / 1001 / 328.
Die Inventur sucht Namensreferenzen, keine semantisch identischen Implementierungen.

Ghidra-Export erzeugte 3811 / 2952 / 1258 Decompilate einschliesslich PLT-Thunks
und zusaetzlich erkannter Funktionen. Kein Export meldete einen fehlgeschlagenen
Decompilationsaufruf; Warnungen und falsche Typrekonstruktionen sind trotzdem
vorhanden. Die Dateien sind nicht automatisch baubar.
Ghidra rebasiert diese ELF-Dateien hier um 0x10000: `st_value` der Inventur und
Adresse im Decompilat unterscheiden sich entsprechend. Linux ist eine
Namens-/Analysehilfe; Windows-Verhalten muss separat bestaetigt werden.

## Noch erforderlich fuer einen TC:E-Build

1. `BG_InitializeWeaponDef`, `weapCharTable`, `BG_ParseGearDef` und
   `CG_LoadGearDef`/Server-Gear-Lader portieren; alle Waffenfelder und
   TC:E-Waffen-IDs konsistent in den gemeinsamen Headern definieren.
2. `playerState_t`, `entityState_t`, `gclient_t`, `gentity_t` und Clientstrukturen
   anhand der DLL-Zugriffe und Engine-Uebergaben abgleichen. Keine pauschale
   Uebernahme der ET-Strukturgroessen.
3. Bewegungs-/Waffenlogik (`bg_pmove.c`), Nachladen, Aiming, Feuermodi,
   Schaden/Ballistik und benoetigte Mod-Konstanten rekonstruieren.
4. Server-Spiellogik: Runden, Bomben/Objective, Teams, Klassen, Ausruestung,
   Spawn, Kommandos, Cvars, Bot-Erweiterungen und gespeicherter Zustand.
5. Client: Waffenanimationen, HUD/Limbo, Radar, Treffer-/Schall-/Raucheffekte,
   Client-/Server-Kommandos und Vorhersage.
6. UI-Menues und Mod-Konfiguration anhand der TC:E-Assets und DLL rekonstruieren.
7. Erst danach isolierte Engine-Tests: Laden der DLLs, Mapstart, Verbindung,
   Spawn, Waffenwechsel, Schiessen, Rundenwechsel und Abgleich mit Original-TC:E.

Naechster konkreter Einstieg: Linux-Referenzen `BG_InitializeWeaponDef`,
`BG_ParseGearDef`, `CG_LoadGearDef`, Windows `BG_ParseWeaponDef` bei 0x30007390.
Die Initialisierungsroutine davor liegt bei 0x30006a50. Der Waffenparser allein
initialisiert keine komplette Waffe; er setzt nur die explizit gelesenen Felder.
