# TCE2 Modern UI

Optionales Menü-Theme für TCE2 1.0.2 mit neu aufgebauten Einstellungsseiten, Server- und Profilansichten, Dialogen, Pausenmenü und nativen Barlow-Schriften. Passende optionale Clientmodule gestalten auch Scoreboard und Verbindungs-/Ladeanzeige neu. Dieses Verzeichnis ist die unabhängig bearbeitete Fassung. Die Originaldateien unter `pak/ui`, `pak/ui_obj`, `pak/ui_robj` und `pak/ui_tdm` bleiben unverändert.

Das Theme enthält UI-Ressourcen, Schriften und Bilder. Es enthält keine DLLs, Gameplay-Änderungen oder Engine-Implementierung. Alle 69 ursprünglichen Menüs einschließlich seltener und Editor-Menüs sind in der bearbeiteten Quelle enthalten. Der Quellvergleich schützt Aktionen, Cvars, Bindings, Listenwerte, Beschriftungen, Feeder, Ownerdraws und Bedingungen. Geometrie, Schrift, Farben und ausdrücklich numerische Farbwechsel dürfen sich unterscheiden. Dieser Vergleich ersetzt keine Bedienungsprüfung im Spiel.

Die überarbeiteten Unterseiten nutzen eine 852×480-Arbeitsfläche mit größeren Einstellungszeilen und festen Fußleisten. Vier Kategorieverweise auf den Settings-Unterseiten verwenden ausschließlich die vorhandene Zurück-Aktion und das bestehende Zielmenü; die aktive Kategorie bleibt ohne Klickaktion. Diese vier genau abgeleiteten Verweise sind ausdrücklich geprüft. Listen dürfen Positionen und Spaltenbreiten ändern; Anzahl, Reihenfolge und maximale Zeichenanzahl bleiben geschützt. `ui/modern_widgets.h` ist eine gesondert semantisch geprüfte Darstellung der ursprünglichen Widgets. Das zuvor akzeptierte Hauptmenü einschließlich seiner gemeinsamen Menüabhängigkeiten bleibt bytegleich.

## Installation und Rückkehr zum bisherigen Aussehen

Genau **eine** Variante auswählen:

| Variante | PK3 | Einsatz |
| --- | --- | --- |
| Game | `Game/zz_tce2-1.0.2-modern-ui.pk3` | Normales Spielen, ohne Editor-Menü und dessen Ladeverweise |
| Editor | `Editor/zz_tce2-1.0.2-modern-ui-editor.pk3` | Passende ausdrücklich aktivierte lokale Editor-Version; alle ursprünglichen Menüfunktionen erhalten |

Die gewählte PK3 in das tatsächlich verwendete Mod-Verzeichnis `tce2` der vorhandenen Installation kopieren, beispielsweise `<fs_homepath>/tce2/`. Keine zweite Spielinstallation anlegen. Game und Editor niemals zusammen installieren. Andere Theme-PK3s können dieselben Ressourcen überschreiben; für einen eindeutigen Test außerhalb des Spielsuchpfads aufbewahren.

Die bisherigen TCE2-Pakete einschließlich `tce2-1.0.2.pk3` und die normalen Spielmodule bleiben erforderlich. Das Theme ersetzt diese nicht. Danach das Spiel vollständig neu starten, damit Menüs, Shader und Schriften neu geladen werden. Für die Rückkehr zum bisherigen Aussehen ausschließlich die Theme-PK3 aus dem Spielsuchpfad verschieben und erneut starten.

Die Game-Variante übernimmt drei bereits vorhandene Produktionsressourcen aus `src/assets/ui`: `ingame_main.menu`, `options_controls.menu` und `options_customise_hud.menu`. So bleiben die bestehenden Game-Grenzen sowie die erweiterten Controls und HUD-Einstellungen erhalten. Ihre Inhalte werden gestaltet, nicht auf den älteren Stand aus `pak` zurückgesetzt. Die Editor-Variante verwendet für diese Menüs die ursprüngliche `pak`-Semantik. Die Ausnahmen sind im jeweiligen Paket unter `tce2_ui/package.json` ausgewiesen.

## Optionale passende Clientmodule

Zusätzlich zu den reinen Theme-PK3s gibt es getrennte Clientmodul-PK3s unter `dist/TCE2-Modern-UI/Game/tce2-1.0.2.pk3` und `Editor/tce2-1.0.2.pk3`. Jede enthält CGame und UI für **Windows 32 Bit und Linux x86_64** sowie die unveränderten erforderlichen Produktionsressourcen. Sie enthält kein QGame, keinen Omni-bot-Core und keine vollständige Spielinstallation. Die Linuxmodule sind mit der vorhandenen Zig-Toolchain crosskompiliert; daraus wird kein zusätzlicher Linux-Laufzeittest abgeleitet.

Für die native Theme-Darstellung von Scoreboard und Verbindungsanzeige die zur eigenen Game-/Editor-Variante passende Clientmodul-PK3 gemeinsam mit der entsprechenden `zz_...modern-ui...pk3` verwenden. Vor dem Ersetzen einer vorhandenen gleichnamigen `tce2-1.0.2.pk3` eine Kopie **außerhalb des Spielsuchpfads** sichern und das Spiel schließen. Die vorhandenen Servermodule bleiben unverändert. Editor-Clientmodule gehören ausschließlich zur passenden lokalen Editor-Umgebung, nicht zu einer normalen Game-Installation. Für den vollständigen Rollback die gesicherte Clientmodul-PK3 zurücklegen und die Theme-PK3 entfernen.

Diese zusätzliche Lieferung ist ausdrücklich ein Paket für die zwei genannten Client-ABIs. Ihr internes `tce2/1.0.2/client-abis.json` trägt deshalb `scope: local-diagnostic`; es ist keine vollständige Veröffentlichung für alle zehn Plattformziele. Die gemeinsame PK3 enthält beide ausgewählten Plattformpaare, damit Windows und Linux nicht verschiedene gleichnamige Ein-Plattform-Pakete benötigen. Die Erzeugung und Rückprüfung verwenden `tools/release/client_packages.py` (`canonical_client`, `check_canonical`, `check_module`) und den Produktionsressourcen-Validator. Die Theme-PK3 bleibt davon unabhängig und enthält weiterhin keine nativen Module.

Die aktuellen vorhandenen Caches lassen sich gezielt ohne QGame bauen:

```powershell
cmake --build build/releases/1.0.2/x86/game --config Release --target cgame ui --parallel 4
cmake --build build/releases/1.0.2/x86/navstudio --config Release --target cgame ui --parallel 4
cmake --build build/modernization-validation/linux-x86_64 --config Release --target cgame ui --parallel 4
cmake --build build/ui-modern-linux-x86_64-editor --config Release --target cgame ui --parallel 4
```

Windows-Builds benötigen die zur Cache-Konfiguration passende MSVC-Entwicklungsumgebung. Die Linux-Caches verwenden die vorhandene Zig-Toolchain; der Editor-Cache hat `FEATURE_NAVSTUDIO=ON`, der Game-Cache `OFF`. Die kanonische Paketierung muss für jede Variante genau deren Release-CGame/UI-Paar aus beiden Plattformen einlesen und darf keine Module zwischen den Varianten mischen.

Nach den Builds erzeugt dieser produktive, rein paketierende CLI beide Clientmodul-PK3s. Er verwendet ausschließlich die angegebenen CMake-Discovery-Manifeste; es gibt keine automatische Suche nach möglicherweise älteren Dateien in anderen `bin`-Verzeichnissen. Bei anderen Cachepfaden die vier Manifestargumente anpassen.

```powershell
python tools/ui_modernize/client_modules.py `
  --windows-game build/releases/1.0.2/x86/game/tce-modules-Release.json `
  --windows-editor build/releases/1.0.2/x86/navstudio/tce-modules-Release.json `
  --linux-game build/modernization-validation/linux-x86_64/tce-modules-Release.json `
  --linux-editor build/ui-modern-linux-x86_64-editor/tce-modules-Release.json `
  --out dist/TCE2-Modern-UI --report build/ui-modern-client-modules.json
```

Der CLI prüft beide Varianten vor dem ersten Paketschreiben: Release-/Variantenidentität, native Dateinamen und ABI-Header, aktuelle tatsächliche UI-Autorenbefehle, kanonische Produktionsressourcen und identische gemeinsame Assets. Er liest QGame nicht ein, baut keine Module und verändert weder Theme-PK3s noch eine Spielinstallation. Der JSON-Beleg bindet Eingabemanifeste, alle acht Clientmodule, Paketbytes und die verwendeten Werkzeuge. Er ersetzt nicht den vorher erforderlichen Build oder einen Laufzeittest.

## Lokal prüfen und paketieren

Alle Befehle im Repository-Stamm ausführen. Python 3 ist erforderlich; diese Schritte bauen keine Spielmodule und starten kein Spiel.

```powershell
python -m unittest discover -s tools/ui_modernize -p "test_*.py"
python tools/ui_modernize/generate_assets.py --check
python tools/ui_modernize/check.py --baseline ui-modern/original-inventory.json --original pak --edited ui-modern --require-styled --out build/ui-modern-parity.json
python tools/ui_modernize/package.py --out dist/TCE2-Modern-UI
python tools/ui_modernize/verify_package.py --package dist/TCE2-Modern-UI/Game/zz_tce2-1.0.2-modern-ui.pk3 --variant game --baseline ui-modern/original-inventory.json --out build/ui-modern-game-package.json
python tools/ui_modernize/verify_package.py --package dist/TCE2-Modern-UI/Editor/zz_tce2-1.0.2-modern-ui-editor.pk3 --variant editor --baseline ui-modern/original-inventory.json --out build/ui-modern-editor-package.json
```

Der Paketcheck prüft die aktuellen Quellbytes, die drei Game-Ausnahmen, vollständige Menüabdeckung, Assets und Aliasdateien, Manifest-Hashes, UI-Semantik und die Game/Editor-Grenze. Unbekannte zusätzliche Dateien, Binärdateien und verschachtelte Archive werden abgelehnt. Berichte liegen außerhalb der Original- und Theme-Ressourcen.

Der feste Paketmarker `tce2_ui/runtime.cfg` enthält ausschließlich `TCE2_MODERN_UI 1` mit abschließendem Zeilenumbruch. Passende aktuelle TCE2-Module können darüber die native Theme-Darstellung von Ladeanzeige und Scoreboard aktivieren. Die Datei wird nicht als Konfigurationsskript ausgeführt und enthält keine Konsolenbefehle.

`original-inventory.json` bindet die 102 Dateien der vier ursprünglichen UI-Verzeichnisse mit Größen und SHA-256 sowie die Version des Semantikprüfers. Bei einem Fehler dieses Inventars zuerst die einzelnen Original- und Werkzeug-Hashes untersuchen. Das Inventar nicht automatisch neu erzeugen, um eine Änderung zu übergehen.

Für eine bewusst neue Gestaltung gibt es `python tools/ui_modernize/restyle.py`. Dieser Generator schreibt ausschließlich in das unabhängige `ui-modern`-Verzeichnis und überschreibt dort manuelle Menüänderungen. Er ist für das gewöhnliche Paketieren der vorliegenden Fassung nicht erforderlich.

## Ressourcen und Nachweisgrenzen

Die Barlow-Schriften werden über die native FreeType-Schriftregistrierung geladen. Herkunft, feste Quellrevision, Hashes und die SIL-Open-Font-License-Texte liegen unter `assets/fonts/tce2_ui/`. Die Herkunft der selbst erzeugten Oberflächen, Symbole und des ursprünglichen generierten Hintergrunds steht in `assets/tce2_ui/SOURCE.json`. Für den Build sind diese Ressourcen bereits lokal vorhanden; es ist kein Font-Download erforderlich.

Die tatsächliche Laufzeitprüfung bezieht sich auf die vorhandene Windows-Installation mit ETLegacy 2.86 und dessen FreeType-Pfad. Daraus folgt keine Zusage für ET 2.60b, jeden Renderer, jede Auflösung oder die vollständige Live-Bedienung aller 69 Menüs. Der Quell- und Paketcheck prüft auch unbenutzte Menüs, führt aber keinen Engine-Präprozessor und keine Mausinteraktion aus.

Die ServerInfo-Listen behalten die ursprüngliche maximale Zeichenanzahl ihrer Feeder-Spalten. Lange Werte können dort weiterhin abgeschnitten werden; die breitere Darstellung ist keine Erweiterung des Feeder-Vertrags. Große Scoreboard-Roster werden vollständig in automatisch wechselnden Seiten dargestellt, statt Spieler still wegzulassen. Die bisherigen Sichtbarkeitsregeln für GT5-Statistiken bleiben erhalten.

Weitere Bildreferenzen für die Aufteilung sind Dirty Bombs Teamübersicht ([Screenshot-Eintrag](https://www.mobygames.com/game/windows/dirty-bomb/screenshots/gameShotId%2C974413/)) und die [Audio-Einstellungen von The Finals](https://media.dotesports.com/wp-content/uploads/2023/12/audio-settings-the-finals.jpg?w=640). Daraus wurden keine Bilder, Logos oder UI-Quellen ins Theme übernommen.

Als gestalterische Referenz dient Valves [Panorama-Ankündigung vom 19. Juni 2018](https://blog.counter-strike.net/2018/06/20482/). Es werden weder Panorama-Code noch dessen Grafiken übernommen. Der [Dirty-Bomb-Bericht zu Options-Untermenüs bei 1440p](https://forums.splashdamage.com/t/game-bug-options-menu-sub-menus-do-not-work-at-1440p/212795) ist eine Erinnerung, Bedienbarkeit und Auflösungen gesondert zu prüfen; er ist weder eine Designspezifikation noch ein Nachweis für die Funktion dieses Themes bei 1440p.
