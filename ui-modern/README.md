# TCE2 Modern UI

Optionales Menü-Theme für TCE2 1.0.2: dunkle Flächen, gelbe Akzente, kompaktere Navigation und native Barlow-Schriften. Dieses Verzeichnis ist die unabhängig bearbeitete Fassung. Die Originaldateien unter `pak/ui`, `pak/ui_obj`, `pak/ui_robj` und `pak/ui_tdm` bleiben unverändert.

Das Theme enthält UI-Ressourcen, Schriften und Bilder. Es enthält keine DLLs, Gameplay-Änderungen oder Engine-Implementierung. Alle 69 ursprünglichen Menüs einschließlich seltener und Editor-Menüs sind in der bearbeiteten Quelle enthalten. Der Quellvergleich schützt Aktionen, Cvars, Bindings, Listenwerte, Beschriftungen, Feeder, Ownerdraws und Bedingungen. Geometrie, Schrift, Farben und ausdrücklich numerische Farbwechsel dürfen sich unterscheiden. Dieser Vergleich ersetzt keine Bedienungsprüfung im Spiel.

## Installation und Rückkehr zum bisherigen Aussehen

Genau **eine** Variante auswählen:

| Variante | PK3 | Einsatz |
| --- | --- | --- |
| Game | `Game/zz_tce2-1.0.2-modern-ui.pk3` | Normales Spielen, ohne Editor-Menü und dessen Ladeverweise |
| Editor | `Editor/zz_tce2-1.0.2-modern-ui-editor.pk3` | Passende ausdrücklich aktivierte lokale Editor-Version; alle ursprünglichen Menüfunktionen erhalten |

Die gewählte PK3 in das tatsächlich verwendete Mod-Verzeichnis `tce2` der vorhandenen Installation kopieren, beispielsweise `<fs_homepath>/tce2/`. Keine zweite Spielinstallation anlegen. Game und Editor niemals zusammen installieren. Andere Theme-PK3s können dieselben Ressourcen überschreiben; für einen eindeutigen Test außerhalb des Spielsuchpfads aufbewahren.

Die bisherigen TCE2-Pakete einschließlich `tce2-1.0.2.pk3` und die normalen Spielmodule bleiben erforderlich. Das Theme ersetzt diese nicht. Danach das Spiel vollständig neu starten, damit Menüs, Shader und Schriften neu geladen werden. Für die Rückkehr zum bisherigen Aussehen ausschließlich die Theme-PK3 aus dem Spielsuchpfad verschieben und erneut starten.

Die Game-Variante übernimmt drei bereits vorhandene Produktionsressourcen aus `src/assets/ui`: `ingame_main.menu`, `options_controls.menu` und `options_customise_hud.menu`. So bleiben die bestehenden Game-Grenzen sowie die erweiterten Controls und HUD-Einstellungen erhalten. Ihre Inhalte werden gestaltet, nicht auf den älteren Stand aus `pak` zurückgesetzt. Die Editor-Variante verwendet für diese Menüs die ursprüngliche `pak`-Semantik. Die Ausnahmen sind im jeweiligen Paket unter `tce2_ui/package.json` ausgewiesen.

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

`original-inventory.json` bindet die 102 Dateien der vier ursprünglichen UI-Verzeichnisse mit Größen und SHA-256 sowie die Version des Semantikprüfers. Bei einem Fehler dieses Inventars zuerst die einzelnen Original- und Werkzeug-Hashes untersuchen. Das Inventar nicht automatisch neu erzeugen, um eine Änderung zu übergehen.

Für eine bewusst neue Gestaltung gibt es `python tools/ui_modernize/restyle.py`. Dieser Generator schreibt ausschließlich in das unabhängige `ui-modern`-Verzeichnis und überschreibt dort manuelle Menüänderungen. Er ist für das gewöhnliche Paketieren der vorliegenden Fassung nicht erforderlich.

## Ressourcen und Nachweisgrenzen

Die Barlow-Schriften werden über die native FreeType-Schriftregistrierung geladen. Herkunft, feste Quellrevision, Hashes und die SIL-Open-Font-License-Texte liegen unter `assets/fonts/tce2_ui/`. Die Herkunft der selbst erzeugten Oberflächen, Symbole und des ursprünglichen generierten Hintergrunds steht in `assets/tce2_ui/SOURCE.json`. Für den Build sind diese Ressourcen bereits lokal vorhanden; es ist kein Font-Download erforderlich.

Die tatsächliche Laufzeitprüfung bezieht sich auf die vorhandene Windows-Installation mit ETLegacy 2.86 und dessen FreeType-Pfad. Daraus folgt keine Zusage für ET 2.60b, jeden Renderer, jede Auflösung oder die vollständige Live-Bedienung aller 69 Menüs. Der Quell- und Paketcheck prüft auch unbenutzte Menüs, führt aber keinen Engine-Präprozessor und keine Mausinteraktion aus.

Als gestalterische Referenz dient Valves [Panorama-Ankündigung vom 19. Juni 2018](https://blog.counter-strike.net/2018/06/20482/). Es werden weder Panorama-Code noch dessen Grafiken übernommen. Der [Dirty-Bomb-Bericht zu Options-Untermenüs bei 1440p](https://forums.splashdamage.com/t/game-bug-options-menu-sub-menus-do-not-work-at-1440p/212795) ist eine Erinnerung, Bedienbarkeit und Auflösungen gesondert zu prüfen; er ist weder eine Designspezifikation noch ein Nachweis für die Funktion dieses Themes bei 1440p.
