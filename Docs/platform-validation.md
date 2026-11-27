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

Zwischen `c7452b8` und `ac5cc62` sind alle 105 Dateien unter `src` bytegleich, die an mindestens einem der beiden Stände Verweise auf `trap_AddCommand`, `ConsoleCommand(` oder `ClientCommand(` enthalten. Die Linux-Hostinventur nach Kartenstart ist unten dokumentiert. Dynamische Befehle aus optionalen Lua-Skripten hängen von der eingesetzten Konfiguration ab.

## Gameplay-Regression auf `84d2fb1`

| Linux-Ziel | Verfahren | Erfolgreiche Prüfungen je Konfiguration (Debug und Release) |
| --- | --- | --- |
| x86_64 | ASan und UBSan | 2.749 Movement, 167 Waffen, 6.858 Treffer- und Schadensverarbeitung, 7.492 Server-Lebenszyklus |
| i386, ARMv7 und ARM64 | QEMU-User, je ein Lauf pro Architektur und Konfiguration | 2.749 Movement einschließlich Schrittgeräuschen |

Alle aufgeführten Läufe meldeten null fehlgeschlagene Prüfungen. Die Trefferprüfung verwendet analytische Kollisionsdienste. Der Vergleich in einer laufenden Engine mit Referenzkarte und echten Client-Server-Paketen bleibt offen.

Für einen historischen Vergleich führte derselbe Testtreiber von `b8b1379` die ausgewählten Produktionsfunktionen aus Release 1.0.4 und aus dem PR-Stand jeweils unter Linux x86_64 aus. Mit GCC, ASan und UBSan bestanden für beide Quellstände in Debug und Release jeweils 2.749 Movement-Prüfungen einschließlich Schrittgeräuschen sowie 167 Waffenprüfungen. Die Testausgaben waren für jedes der vier Versionspaare bytegleich. Leak-Erkennung war in diesen Läufen ausgeschaltet. Der Vergleich verwendet kontrollierte Eingaben und synthetische Kollisionen. Ein Vergleich vollständiger Matches mit identischen Eingaben bleibt offen.

Der historische Treffervergleich gegen `6f70c99` verwendete pro Version die eigenen Header und Funktionskörper, einschließlich `Bullet_Fire`, `Bullet_Fire_Extended` und `G_Damage`. Der Testtreiber und seine Fixtures sind zwischen diesen Revisionen identisch. In Debug und Release bestanden auf beiden Ständen jeweils 6.858 Prüfungen mit GCC, ASan und UBSan (ohne Leak-Erkennung). Die Protokolle waren je Konfiguration bytegleich. Enthalten sind 288 Fälle mit bewegten Beinen bei 50, 100 und 150 ms sowie drei Referenzstrahlen für Railhouse. Die Kollisionsdienste sind analytisch. Die Ausführung mit der BSP-Kollision der Engine bleibt eine separate Abnahme.

## PK3-Vergleich mit Release 1.0.4

Das veröffentlichte Mehrplattform-PK3 von 1.0.4 enthält 346 Dateien. Das lokale Linux-x86_64-Testpaket von 1.0.5 enthält 324. Der SHA-256-Vergleich jedes entpackten Eintrags ergab 319 bytegleiche Dateien. Von den gemeinsam vorhandenen Pfaden unterscheiden sich nur `cgame.mp.x86_64.so` und `ui.mp.x86_64.so`.

Nur das Release enthält die 22 Clientmodule für die übrigen Plattformen und drei Metadaten-Dateien unter `tce2/1.0.4/`. Nur das Testpaket enthält `models/players/tce2/wall_high_ready.mdx` und die beiden Dateien `base-game.json` und `client.manifest` unter `tce2/1.0.5/`. Das lokale Paket wurde auf `84d2fb1` mit den Release-Binärdateien aus `6309cd9` erstellt. Den anschließenden Paketdownload in beiden Versionsrichtungen dokumentieren die Linux-Beitrittstests unten. [Referenzrelease](https://github.com/jay1110/true-combat/releases/tag/1.0.4)

## Linux-Serverstart im Engine-Host

ET: Legacy 2.86.2 (Linux x86_64, Dedicated Server) lud mit den ET-Basispaketen auf der Karte `oasis` in zwei getrennten Läufen das Servermodul und fand `vmMain`. Geprüft wurden das veröffentlichte 1.0.4-Modul und das Linux-Testpaket von 1.0.5. Beide Läufe erreichten die Spielinitialisierung und brachen beim Token `MP40` in `animations/scripts/human_base.script`, Zeile 21, ab. Die anschließend angeforderte TC:E-Karte `maps/obj_northport.bsp` fehlte. Beide Logs melden außerdem eine fehlgeschlagene IPv4-Bindung. Der Versuch mit `obj_village` endete bereits bei der Kartensuche. In einem separaten Lauf mit Netzwerkberechtigung öffnete ET: Legacy einen UDP-Socket auf 127.0.0.1:50947. Die Bindung wurde über /proc/net/udp bestätigt. Der Kartenstart brach erneut beim Token `MP40` ab. Die folgenden Läufe ergänzten die TC:E-Pakete und bestätigen den Beitritt.

## Beitritt und PK3-Download unter Linux x86_64

ET: Legacy 2.86.2 lud die Karte `obj_northport` mit vier CRC-geprüften TC:E-0.49-Assetpaketen aus dem archivierten 0.49-Installer. Dieser erste Lauf verwendete den 0.49-Installer. Die Wiederholung mit den fünf 0.49b-Referenzpaketen folgt unten. Im offiziellen Modus lehnte die originale 0.49-Datei `official.dat` die getestete Paketliste ab. Im dokumentierten Community-Modus (`sv_official 0`, `sv_pure 1`) gelangen beide Beitritte über Loopback mit getrennten Clientverzeichnissen und lokalem HTTP-Redirect.

| Client beim Start | Server und übertragenes PK3 | Empfangener SHA-256 | Serverereignis |
| --- | --- | --- | --- |
| 1.0.5 | 1.0.4 | `33f1cfd9aebdc44ef8b7f4dfd7e5307dce04db0f2bf8789aa081519a31d5fcaf` | ClientBegin bei 16.400 ms Serverzeit |
| 1.0.4 | 1.0.5 | `5fe949e40a9d15bc38b6433073a17fc12fd8f45e45fb3d6e5a42c96e7caf1d72` | ClientBegin bei 15.500 ms Serverzeit |

Beide empfangenen Archive stimmen bytegenau mit der jeweiligen Serverdatei überein. Das extrahierte CGame entspricht in beiden Fällen dem Modul im heruntergeladenen Archiv. Der Client übernimmt beim Beitritt somit den Modulstand des Servers. Für die Erfolge wurden externe HTTP-Anfragen des Testclients über einen unbesetzten lokalen Proxy geleitet und der lokale Paketserver davon ausgenommen. Im ersten Versuch kollidierte die HTTP-Weiterleitung mit einem anderen aktiven Download. Dauerhaft unterschiedliche aktive Client-/Servermodule und der Gameplay-Vergleich unter identischen Eingaben bleiben offen.

### Wiederholung mit den 0.49b-Referenzpaketen

Die fünf PK3-Dateien vom [Clan-Paketserver](https://et.clan-etc.de/tce2/) bestanden die ZIP-CRC-Prüfung. Die ersten vier sind bytegleich mit den geprüften 0.49-Installerdateien. Das zusätzliche `pak4.pk3` enthält 113 Einträge (SHA-256 `607c4cf82703320c4c96cb5cf5eab88e1872468b14f70ea3e833585283d3d0c5`). Die dort angebotene 1.0.4 stimmt bytegenau mit dem GitHub-Release überein. Für 1.0.5 wurde das lokale Linux-Testpaket verwendet.

Mit frischen Clientverzeichnissen, allen fünf Paketen und derselben Community-Konfiguration erreichten beide Clients auf `obj_northport` nach HTTP-Download den Zustand `ClientBegin`. Der 1.0.4-Server protokollierte 15.750 ms und der 1.0.5-Server 15.150 ms Serverzeit. Die empfangenen SHA-256-Werte entsprechen in beiden Richtungen der Tabelle oben. Beim Beenden des zweiten Clients lief die viersekündige Wartefrist ab.

### Konsoleninventur nach Kartenstart

Auf beiden Linux-x86_64-Servern meldete `cmdlist` dieselben 65 Befehlsnamen. `cvarlist` lieferte 337 Einträge auf 1.0.4 und 338 auf 1.0.5. Alle gemeinsamen Namen hatten dieselben angezeigten Flags. 1.0.5 ergänzt `g_weaponWallAvoidance`. Die angezeigten Defaults unterschieden sich bei `com_pid`, `gamedate`, `net_qport` und `tce_version`. Bei den aktuellen Werten kamen `fs_basepath`, `fs_homepath`, `sv_pakNames`, `sv_paks`, `sv_referencedPakNames`, `sv_referencedPaks` und `sv_serverid` hinzu. Beide Prozesse liefen mit frischen Benutzerverzeichnissen und denselben 0.49b-Assets auf `obj_northport`. Der Vergleich erfasst die vom Dedicated Server aufgelisteten Einträge. Clientbefehle, modulinterne Dispatch-Tabellen und das Verhalten nach einem Reset benötigen eigene Tests.

Beim Start beider Linux-Clients mit getrennten frischen Profilen, nach dem Laden des jeweiligen UI-Moduls und vor dem Verbinden, ergaben sich dieselben 190 Befehlsnamen und 593 CVar-Namen mit identischen angezeigten Flags. Abweichende angezeigte Defaults betrafen `cl_guid`, `com_pid` und `net_qport`. Bei den aktuellen Werten unterschieden sich zusätzlich `fs_basepath` und `fs_homepath`. Nach dem Beitritt zu einem Server desselben jeweiligen Versionsstands auf obj_northport meldeten beide Clients mit geladenem CGame dieselben 324 Befehlsnamen und 754 CVar-Einträge einschließlich eines unbenannten Eintrags. Alle angezeigten Flags stimmten überein. Die drei Unterschiede bei den Defaults blieben dieselben wie vor dem Beitritt. Bei 14 aktuellen Werten gab es Unterschiede, darunter cg_etVersion, Paketlisten, Speicherbelegung und Sitzungswerte.

Auf den beiden Dedicated Servern stellte `reset` bei `g_motd`, `g_logSync`, `g_inactivity` und `sv_hostname` nach einer temporären Änderung jeweils den beobachteten Default wieder her. Auf 1.0.5 bestand zusätzlich `g_weaponWallAvoidance` die Folge 1 → 0 → 1.

### Omni-Bot im Linux-Server

Mit den fünf 0.49b-Paketen auf `obj_northport` fanden 1.0.4 und 1.0.5 bei leerem `omnibot_path` den jeweiligen Core unter `tce2/omni-bot/omnibot_et.x86_64.so`. Beide luden Wegpunkte und meldeten den Beitritt des angeforderten Bots. Beim 1.0.5-Testpaket aktivierte die mitgelieferte `omnibot-release.cfg` die Bots. Das veröffentlichte 1.0.4-ZIP enthält diese Datei nicht. Dort gelang der Lauf mit `+set omnibot_enable 1` vor dem Kartenstart. Mit explizitem Core-Pfad traten auf beiden Versionen auch zwei Bots unterschiedlichen Teams bei und erschienen in der Statusausgabe. In je einem weiteren GT5-Lauf traten zwei Bots auf unterschiedlichen Seiten an. Beide Server protokollierten auf obj_northport nach je 325.900 ms Serverzeit das Ende durch Zeitlimit (Exit: Timelimit hit.) und beendeten sich anschließend regulär auf quit. Ein Sieg durch das Missionsziel, ein identischer Bewegungsverlauf und die Windows-ARM64-Initialisierung bleiben offen.

### Kartenwechsel mit GT2 und GT5

Vier Linux-x86_64-Serverläufe (je Version und Modus ein Prozess) wechselten über Delta, Northport, Railhouse, Snow, Stadtrand und Village. Bei allen 24 Kartenstarts bestätigte die Statusabfrage den erwarteten Kartennamen. Das Protokoll zeigte für jede Karte das modusspezifische Skript und geladene Omni-Bot-Wegpunkte. Diese kurzen Läufe prüfen den Start und Wechsel der Karten.

## Offene Abnahme in Prioritätsreihenfolge

1. Mobile Laufzeitabdeckung ergänzen. Offen sind ein Intel-iOS-Simulator, die Ausführung auf realen Geräten und ein Android-Systemtest für die beiden ARM-ABIs.
2. Die Module in einer kompatiblen externen Engine laden. Auf Windows ARM64 die Bot-Initialisierung mit der ausgelieferten DLL und den Skripten nachweisen. Für iOS den Paketpfad, die Signierung und das Lademodell mit dem vorgesehenen Host klären.
3. Mit festgehaltenen Versionen der Referenzmodule, PK3-Dateien, Maps und Konfigurationen Client und Server in beiden Versionsrichtungen verbinden. Dynamische Commands, CVar-Flags und Reset-Verhalten sowie Paket- und Gameplay-Traces unter denselben Eingaben vergleichen.
4. Speicher- und Laufzeitbefunde an reproduzierbare Szenarien binden. Performanceänderungen benötigen Messwerte vor und nach der Änderung sowie dieselben Kompatibilitätstests. Verhaltensändernde Korrekturen erfordern gesonderte Freigabe.

Die sieben Actions-Workflows sind manuell auslösbar. Pushes und PR-Aktualisierungen lösen auf diesem Branch keine Matrix aus.
