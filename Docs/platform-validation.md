# Plattformvalidierung für PR #2

Die Ausgangsanalyse bezieht sich auf `c7452b8`. Die folgende Tabelle nennt für jeden späteren Nachweis den geprüften Stand. Ein Build umfasst UI, CGame, QGame und den Bot-Core, soweit in der Zeile angegeben. Die acht portablen CTests prüfen GeoIP, GeoIP-Richtlinien, Score-Pakete, Klassenanfragen, zwei Kompatibilitätsdatensätze sowie Client- und Itemdarstellung.

| Ziel | Nachweis für Debug und Release | Grenze |
| --- | --- | --- |
| Windows x86, x86_64, ARM64 | MSVC mit Bot-Core auf `254a765`, CTest auf allen drei Architekturen erfolgreich. Auf ARM64 je acht Tests. [CI](https://github.com/jay1110/true-combat/actions/runs/46017874085) | ARM64-Start mit externer Engine offen. |
| Windows ARM64, LLVM-MinGW | Auf `fcdff95` vier DLLs je Konfiguration gebaut. Die drei Suchmuster im Servermodul entsprechen dem Dateinamen des Bot-Cores. | Kontrolle der Binärdateien. Bot-Initialisierung im Spiel offen. |
| Linux x86_64 | GCC und Clang im öffentlichen Modul-Build. Separater Clang-Sanitizerjob. [CI](https://github.com/jay1110/true-combat/actions/runs/46019619874) | Der Bot-Core ist in diesen Linux-Jobs ausgeschaltet. |
| Linux i386, ARMv7, ARM64 | Lokale Zig-Cross-Builds mit Bot-Core auf dem Prüfstand `984f115`. Je acht CTests unter QEMU bestanden. | Ausführung auf physischer Zielhardware offen. |
| macOS x86_64, ARM64, Universal | Auf `d92757c` sechs Builds mit Bot-Core, Mach-O-Architekturprüfung aller vier Module und CTest erfolgreich. [CI](https://github.com/jay1110/true-combat/actions/runs/46019619874) | Alle Jobs liefen auf macOS-15-Runnern. Separate Intel-Hardware und Engine-Start offen. |
| Android armeabi-v7a, arm64-v8a, x86, x86_64 | Auf `98841dd` acht vollständige Builds mit NDK r27d. Alle 32 ELF-Maschinenkennungen stimmen. | Auf `2fae2c9` bestanden auf API-29-Emulatoren für x86 und x86_64 je acht Testprogramme in Debug und Release. Für ARMv7 und ARM64 bestanden die acht Tests aus den Objekten von `98841dd` unter QEMU-User mit statischer Bionic-Verlinkung für API 29. Android-Systemintegration auf ARM und reale Geräte bleiben separat zu prüfen. [Emulatorprotokolle](https://github.com/jay1110/true-combat/actions/runs/46022781138) |
| iOS ARM64, Simulator ARM64 und x86_64 | Auf `84d2fb1` bestanden alle sechs Debug-/Release-Builds mit Bot-Core. lipo und vtool bestätigten für die vier Module jeweils CPU und SDK-Plattform. [Apple-SDK-Lauf](https://github.com/jay1110/true-combat/actions/runs/46024175310) | Im ARM64-Simulator bestanden die acht portablen Testprogramme in Debug und Release. Der x86_64-Simulatorjob prüft nur den Build. Geräteintegration und Engine-Start bleiben offen. |
| wasm32 | Auf `98841dd` zwei vollständige Builds mit jeweils acht bestandenen CTests unter Node.js. Für alle vier Side-Module je Konfiguration wurden die vorgesehenen Funktionsexporte geprüft. | In Chromium 156 bestanden dieselben acht Testprogramme je Konfiguration. Das Laden der Side-Module in einer Browser-Spielengine bleibt offen. |

## Kompatibilitätsnachweise

Die CVar-Inventur auf `51c3101` stimmt bytegenau mit der Ausgangsinventur überein. Sie enthält 637 Registrierungen und 502 benannte CVars. Alle 612 aktiven Registrierungen der verglichenen Linux-x86_64- und Windows-ARM64-Modulbuilds stimmen nach Modul, Quelldatei, Name, Default und Flags überein. Die Menge der erfassten aktiven Command-Quellorte stimmt ebenfalls überein. Dynamische Registrierungen in Lua oder im Host erfordern einen Laufzeitvergleich.

Auf Windows x86 bestanden fünf isolierte Regressionstests auf `98841dd` in beiden Konfigurationen. Sie decken Kartenraster, Trefferdiagnose, Animationstiming, Statistikanzeige und Waffenstatistik samt Protokoll ab. Die Fixtures verwenden kontrollierte Engine-Aufrufe. [Testprotokolle](https://github.com/jay1110/true-combat/actions/runs/46019710267)

Der Vergleich von Release 1.0.4 mit `ac5cc62` erhält alle 636 ursprünglichen CVar-Registrierungen einschließlich Defaults, Flags und Bedingungen. Die zusätzliche Registrierung `g_weaponWallAvoidance` (Default `1`, Flags `ARCHIVE|SERVERINFO`) war bereits in der Ausgangsanalyse auf `c7452b8` enthalten. Der Vergleich berücksichtigt Mehrfachregistrierungen und lässt Zeilennummern und Herkunftsannotation außer Betracht.

Zwischen `c7452b8` und `ac5cc62` sind alle 105 Dateien unter `src` bytegleich, die an mindestens einem der beiden Stände Verweise auf `trap_AddCommand`, `ConsoleCommand(` oder `ClientCommand(` enthalten. Die tatsächlich vom Host und von Lua bereitgestellte Befehlsliste bleibt offen.

## Gameplay-Regression auf `84d2fb1`

| Linux-Ziel | Verfahren | Erfolgreiche Prüfungen je Konfiguration (Debug und Release) |
| --- | --- | --- |
| x86_64 | ASan und UBSan | 2.749 Movement, 167 Waffen, 6.858 Treffer- und Schadensverarbeitung, 7.492 Server-Lebenszyklus |
| i386, ARMv7 und ARM64 | QEMU-User, je ein Lauf pro Architektur und Konfiguration | 2.749 Movement einschließlich Schrittgeräuschen |

Alle aufgeführten Läufe meldeten null fehlgeschlagene Prüfungen. Die Trefferprüfung verwendet analytische Kollisionsdienste. Der Vergleich in einer laufenden Engine mit Referenzkarte und echten Client-Server-Paketen bleibt offen.

## PK3-Vergleich mit Release 1.0.4

Das veröffentlichte Mehrplattform-PK3 von 1.0.4 enthält 346 Dateien. Das lokale Linux-x86_64-Testpaket von 1.0.5 enthält 324. Der SHA-256-Vergleich jedes entpackten Eintrags ergab 319 bytegleiche Dateien. Von den gemeinsam vorhandenen Pfaden unterscheiden sich nur `cgame.mp.x86_64.so` und `ui.mp.x86_64.so`.

Nur das Release enthält die 22 Clientmodule für die übrigen Plattformen und drei Metadaten-Dateien unter `tce2/1.0.4/`. Nur das Testpaket enthält `models/players/tce2/wall_high_ready.mdx` und die beiden Dateien `base-game.json` und `client.manifest` unter `tce2/1.0.5/`. Das lokale Paket wurde auf `84d2fb1` mit den Release-Binärdateien aus `6309cd9` erstellt. Die Engine-Abnahme mit Paketdownload in beiden Versionsrichtungen steht aus. [Referenzrelease](https://github.com/jay1110/true-combat/releases/tag/1.0.4)

## Offene Abnahme in Prioritätsreihenfolge

1. Mobile Laufzeitabdeckung ergänzen. Offen sind ein Intel-iOS-Simulator, die Ausführung auf realen Geräten und ein Android-Systemtest für die beiden ARM-ABIs.
2. Die Module in einer kompatiblen externen Engine laden. Auf Windows ARM64 die Bot-Initialisierung mit der ausgelieferten DLL und den Skripten nachweisen. Für iOS den Paketpfad, die Signierung und das Lademodell mit dem vorgesehenen Host klären.
3. Mit festgehaltenen Versionen der Referenzmodule, PK3-Dateien, Maps und Konfigurationen Client und Server in beiden Versionsrichtungen verbinden. Dynamische Commands, CVar-Flags und Reset-Verhalten sowie Paket- und Gameplay-Traces unter denselben Eingaben vergleichen.
4. Speicher- und Laufzeitbefunde an reproduzierbare Szenarien binden. Performanceänderungen benötigen Messwerte vor und nach der Änderung sowie dieselben Kompatibilitätstests. Verhaltensändernde Korrekturen erfordern gesonderte Freigabe.

Die sieben Actions-Workflows sind manuell auslösbar. Pushes und PR-Aktualisierungen lösen auf diesem Branch keine Matrix aus.
