# Plattformvalidierung für PR #2

Die Ausgangsanalyse bezieht sich auf `c7452b8`. Die folgende Tabelle nennt für jeden späteren Nachweis den geprüften Stand. Ein Build umfasst UI, CGame, QGame und den Bot-Core, soweit in der Zeile angegeben. Die acht portablen CTests prüfen GeoIP, GeoIP-Richtlinien, Score-Pakete, Klassenanfragen, zwei Kompatibilitätsdatensätze sowie Client- und Itemdarstellung.

| Ziel | Nachweis für Debug und Release | Grenze |
| --- | --- | --- |
| Windows x86, x86_64, ARM64 | MSVC mit Bot-Core auf `254a765`, CTest auf allen drei Architekturen erfolgreich. Auf ARM64 je acht Tests. [CI](https://github.com/jay1110/true-combat/actions/runs/46017874085) | ARM64-Start mit externer Engine offen. |
| Windows ARM64, LLVM-MinGW | Auf `fcdff95` vier DLLs je Konfiguration gebaut. Die drei Suchmuster im Servermodul entsprechen dem Dateinamen des Bot-Cores. | Kontrolle der Binärdateien. Bot-Initialisierung im Spiel offen. |
| Linux x86_64 | GCC und Clang im öffentlichen Modul-Build. Separater Clang-Sanitizerjob. [CI](https://github.com/jay1110/true-combat/actions/runs/46019619874) | Der Bot-Core ist in diesen Linux-Jobs ausgeschaltet. |
| Linux i386, ARMv7, ARM64 | Lokale Zig-Cross-Builds mit Bot-Core auf dem Prüfstand `984f115`. Je acht CTests unter QEMU bestanden. | Ausführung auf physischer Zielhardware offen. |
| macOS x86_64, ARM64, Universal | Auf `d92757c` sechs Builds mit Bot-Core, Mach-O-Architekturprüfung aller vier Module und CTest erfolgreich. [CI](https://github.com/jay1110/true-combat/actions/runs/46019619874) | Alle Jobs liefen auf macOS-15-Runnern. Separate Intel-Hardware und Engine-Start offen. |
| Android armeabi-v7a, arm64-v8a, x86, x86_64 | Auf `98841dd` acht vollständige Builds mit NDK r27d. Alle 32 ELF-Maschinenkennungen stimmen. | Auf `7725f0c` fand adb bei x86 und x86_64 innerhalb von fünf Minuten keinen Emulator. In diesem Lauf starteten keine Tests. [CI](https://github.com/jay1110/true-combat/actions/runs/46020939090) |
| iOS ARM64, Simulator ARM64 und x86_64 | Auf `4711fb3` bestanden alle sechs Debug-/Release-Builds mit Bot-Core. lipo und vtool bestätigten für die vier Module jeweils CPU und SDK-Plattform. [Apple-SDK-Lauf](https://github.com/jay1110/true-combat/actions/runs/46021939121) | Unsignierte Module. Ausführung im Simulator und Geräteintegration sind gesondert zu prüfen. |
| wasm32 | Auf `98841dd` zwei vollständige Builds mit jeweils acht bestandenen CTests unter Node.js. Für alle vier Side-Module je Konfiguration wurden die vorgesehenen Funktionsexporte geprüft. | Integration in eine Browser-Engine offen. |

## Kompatibilitätsnachweise

Die CVar-Inventur auf `557af0e` stimmt bytegenau mit der Ausgangsinventur überein. Sie enthält 637 Registrierungen und 502 benannte CVars. Alle 612 aktiven Registrierungen der verglichenen Linux-x86_64- und Windows-ARM64-Modulbuilds stimmen nach Modul, Quelldatei, Name, Default und Flags überein. Die Menge der erfassten aktiven Command-Quellorte stimmt ebenfalls überein. Dynamische Registrierungen in Lua oder im Host erfordern einen Laufzeitvergleich.

Auf Windows x86 bestanden fünf isolierte Regressionstests auf `98841dd` in beiden Konfigurationen. Sie decken Kartenraster, Trefferdiagnose, Animationstiming, Statistikanzeige und Waffenstatistik samt Protokoll ab. Die Fixtures verwenden kontrollierte Engine-Aufrufe. [Testprotokolle](https://github.com/jay1110/true-combat/actions/runs/46019710267)

## Offene Abnahme in Prioritätsreihenfolge

1. Mobile Build- und Startfehler anhand der jeweiligen SDK-Protokolle beheben. Geräte- und Simulator-Binärdateien auf Architektur und Plattformkennung prüfen. Die acht Testprogramme unter dem Zielbetriebssystem ausführen und die Ergebnisse pro ABI ausweisen.
2. Die Module in einer kompatiblen externen Engine laden. Auf Windows ARM64 die Bot-Initialisierung mit der ausgelieferten DLL und den Skripten nachweisen. Für iOS den Paketpfad, die Signierung und das Lademodell mit dem vorgesehenen Host klären.
3. Mit festgehaltenen Versionen der Referenzmodule, PK3-Dateien, Maps und Konfigurationen Client und Server in beiden Versionsrichtungen verbinden. Dynamische Commands, CVar-Flags und Reset-Verhalten sowie Paket- und Gameplay-Traces unter denselben Eingaben vergleichen.
4. Speicher- und Laufzeitbefunde an reproduzierbare Szenarien binden. Performanceänderungen benötigen Messwerte vor und nach der Änderung sowie dieselben Kompatibilitätstests. Verhaltensändernde Korrekturen erfordern gesonderte Freigabe.
