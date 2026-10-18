# TCE2 NavStudio

Ein Desktop-Programm für BSP-Ansicht, Recast, Omni-bot-Wegpunkte, Goals und Spawns. Eine Map gehört zu einem benannten Projekt. NavStudio verwendet eine vorhandene sichtbare Testinstallation und benötigt keine WPS-Dateien.

Die vollständige Hilfe öffnet **Anleitung** oben rechts im Programm. **English / Deutsch** schaltet die Oberfläche um; Mapnamen, Projektinhalte und Diagnosedaten behalten ihre ursprünglichen Werte.

## Deutsch

### Start

1. `build/navgen-overkill/TCE2-NavStudio.exe` starten. Die Anwendung öffnet ihr eigenes Fenster.
2. **Neues Projekt erstellen** wählen oder rechts ein vorhandenes Projekt öffnen. Ist bereits ein Projekt in einem laufenden Batch geöffnet, zeigt **Laufendes Projekt anzeigen** genau dieses Projekt an.
3. Erst eine Map wählen. Beim bloßen Öffnen des Programms wird kein Testspiel gestartet und keine feste Standardmap geladen.

Die EXE und der Ordner `_internal` gehören zusammen. Beide liegen neben `ET.exe` beziehungsweise `etl.exe` in der vorhandenen Installation. Die Unterordner heißen `etmain`, `tce2`, `omni-bot` und, für normales ET, `omnibot`. Zum Verschieben den vollständigen App-Paketinhalt einschließlich `Start.cmd`, `verify-package.ps1` und `navstudio-package.json` gemeinsam kopieren; vorhandene Spieldaten und Projekte behalten.

### Neues Projekt mit dem Wizard

1. Einen Projektnamen eingeben, zum Beispiel **Delta – Testnetz**. Der Name darf bis zu 100 Zeichen enthalten.
2. **TCE2** oder **Enemy Territory** wählen. TCE2 zeigt `obj_`-Maps und verwendet `fs_game tce2`. ET zeigt Maps ohne `obj_`, verwendet `fs_game omnibot` und ist in Studio auf **Objective GT2** festgelegt. TCE2-GT5 ist der Bombenmodus; die Gametypen beider Mods sind nicht gleichzusetzen.
3. Eine Map aus der Liste wählen. Das Suchfeld filtert den Mapnamen. Ein Eintrag zeigt, ob das Paket in **all_maps** liegt oder bereits installiert ist. Den gewünschten Gametype wählen.
4. **Automatisch generieren** oder **Vorhandenes Netz importieren** wählen. Die Generierung benötigt den passenden Prüfadapter; wenn die Option ausgegraut ist, den angezeigten Hinweis beachten. Ein Import übernimmt vorhandene `.way`- und passende `.gm`-Dateien in das Projekt. Er verändert deren Originale nicht.
5. **Projekt erstellen** drücken. Beim Generieren öffnet NavStudio die Map in der bestehenden sichtbaren Testinstanz. Ein importiertes Netz wird zuerst im Viewer angezeigt und erhält dadurch noch keinen neuen Prüfstatus.

Bei TCE2 erledigt **Netz, Goals & Spawns automatisch erstellen** den gesamten Generierungsablauf. NavStudio erkennt die vorhandenen Spieltypen an den tatsächlich geladenen, exakten `<map>_gtN.script`-Dateien. Nur für vorhandene GT2-/GT5-Scripts entstehen passende Goal-Dateien. Bei vorhandenem GT7-Script werden zusätzliche Deathmatch-Spawns ausgewählt, im Spiel geprüft und in eine projektlokale Scriptkopie eingetragen. Eine neue Generierung beginnt ein neues Netz und sichert den bisherigen Bearbeitungsstand als Archiv. Für einzelne Korrekturen **Bearbeiten** oder **Automatisch reparieren** verwenden.

Nach erfolgreichem Abschluss wird **Wegpunkte, Goals & Mapscripts als ZIP exportieren** freigegeben. Das Paket enthält die aktuelle WAY-Datei, die passenden GM-Dateipaare und die Mapscripts einschließlich des erzeugten GT7-Spawnblocks. Eine beiliegende Anleitung nennt die Zielordner. Originalscripts bleiben beim Export bytegetreu; der GT7-Block bewahrt den ursprünglichen Zeichensatz. Änderungen an Netz, Goals, Mapscript oder Spawns machen den bisherigen Exportstatus ungültig. Ein nicht fertig geprüftes Paket wird nicht als fertig exportiert.

### Mapbibliothek und Projektdateien

Neue vollständige PK3-Kartenpakete in `etmain/all_maps` ablegen. Für dieses Repository ist das `C:/Users/micha/Documents/GitHub/true-combat/etmain/all_maps`. Beim Öffnen eines Projekts wird dessen Paket in `etmain` der Testinstallation ausgeliehen. **Schließen** sichert das Projekt, beendet dessen eigenes Testspiel und gibt das Paket an **all_maps** zurück. Bereits installierte Basispakete bleiben an Ort und Stelle.

Das Projekt bleibt nach dem Zurückgeben der Map gespeichert. Zum Weiterarbeiten das Projekt wieder öffnen; das Kartenpaket wird erneut bereitgestellt. Eine lose BSP ersetzt kein vollständiges PK3 mit Texturen, Modellen und Mapscripts.

Die eigenen Dateien liegen unter `projects/<Projekt-ID>/` neben dem Programm. `project.json` enthält Name, Map und Mod; `preview/` die Geometrie und Vorschläge; `native/` die geprüften Berichte und WAY; `bundles/` die importierten oder erzeugten Begleitdateien; `work/` Entwurf, Spawns und Prüfnachweise. Der tatsächliche Projektpfad steht links unter dem Projektnamen. Die technische ID sorgt dafür, dass gleichnamige Projekte ihre Daten nicht überschreiben.

### Oberfläche und Schaltflächen

Oben stehen die Aktionen für den gesamten Ablauf. Links befinden sich Projekt, Spiel und Darstellung, in der Mitte die Karte, rechts Eigenschaften und Spawns. Direkt unter der Karte stehen die aktuellen offenen Stellen. Die vollständige Liste wächst nach unten mit der Seite; sie hat keinen eigenen Scrollkasten und verkleinert die Kartenansicht nicht. **Diagnose / Erweitert** enthält technische Werkzeuge, verworfene Prüfproben und Quellennachweise.

| Schaltfläche | Wirkung |
| --- | --- |
| **Speichern** | Sichert Projekt und aktuelle Bearbeitung. Ändert das laufende Wegnetz nicht. |
| **Netz, Goals & Spawns automatisch erstellen** | Liest BSP/Mapmodelle, erzeugt Recast und Wege und prüft die Scanproben. Das akzeptierte Netz wird als Entwurf mit demselben Reparaturablauf wie **Automatisch reparieren** weitergeprüft: Duckstellen, Umwege, Aktionsanschlüsse und Abstiege. Erst nach vollständiger nativer Netzprüfung folgen Goals und gegebenenfalls GT7-Spawns. Offene Bewegungsfehler bleiben sichtbar und sperren den fertigen ZIP-Export. |
| **Prüfen** | Prüft genau den aktuellen Entwurf oder Generierungsstand und startet/nutzt dafür automatisch die bestehende Testinstanz. Speichern und Export erledigt die Aktion. |
| **Automatisch reparieren** | Versucht zusätzliche Verbindungen und prüft das Ergebnis erneut. |
| **Ins Spiel übernehmen** | Übernimmt den aktuell geprüften WAY-Stand in dieselbe Testinstallation und lädt ihn nach. Bei Bearbeitung werden Sichern, Export und Prüfung vorgeschaltet. |
| **Testspiel starten** | Öffnet die Projektmap sichtbar; startet bei Bedarf dieselbe vorhandene Installation. |
| **Spiel beenden** | Beendet das eigene Testspiel, auch wenn dessen Spielport nicht mehr antwortet. |
| **Schließen** | Sichert und schließt das Projekt und gibt eine ausgeliehene Map zurück. |

Der Arbeitsstatus zeigt, ob ein Auftrag läuft, abgeschlossen ist oder fehlgeschlagen ist. Einen laufenden Auftrag abschließen lassen; fehlgeschlagene Übernahmen gelten nicht als geladen. **↻**, **F5** oder **Strg+R** lädt die Oberfläche neu und erhält das geöffnete Projekt und seinen Entwurf.

**Probleme beheben** zeigt Fehler im aktuellen Wegnetz. **Generierungsdiagnose** bewahrt zusätzlich die verworfenen Scanproben auf: Eine Kollisionsprobe ist noch kein Wegpunkt. Bodenproben ohne belegte Route werden nach Bereichen zusammengefasst und bleiben offene Diagnosen; darunter können auch Dächer, Mapränder und nicht spielbare Flächen liegen. Mit **Kamera hin** den Bereich ansehen, die Einzelproben bei Bedarf aufklappen oder eine Source-ID suchen. Diese Diagnose ist keine vollständige Kartenfreigabe.

Das bloße Öffnen von **Bearbeiten** ändert weder das Netz noch seinen Prüfstatus. **Prüfen** und **Automatisch reparieren** arbeiten auf demselben aktuellen Entwurf. Erst eine tatsächliche Änderung macht dessen bisherigen Prüfnachweis ungültig.

### Kamera und Darstellung

| Eingabe | Wirkung |
| --- | --- |
| Rechte Maustaste halten + Maus | Kamera drehen. |
| Rechte Maustaste halten + W A S D | Vorwärts, links, zurück, rechts bewegen. |
| Rechte Maustaste halten + E / Q | Hoch / runter. |
| Shift / Strg während der Kamerabewegung | Schneller / langsamer. |
| Mittlere Maustaste ziehen | Kamera seitlich oder vertikal verschieben. |
| Mausrad | Vor / zurück bewegen. |
| **F** bei einer Auswahl | Kamera an die Auswahl setzen. |
| **Augenhöhe** | Ansicht am ausgewählten Wegpunkt auf Spielerhöhe setzen. |
| **Kamera fliegen** | Freie Kamera; ins Bild klicken, WASD bewegen, Leertaste/C hoch/runter, Esc löst die Maus. |

**Auswählen** kehrt zur Bearbeitungsansicht zurück. Unter **Darstellung** lassen sich BSP, Wegpunkte, Verbindungen, Stand-/Duck-Navmesh und Höhenfilter wählen. **Nur Auswahl** bei Verbindungen reduziert das Liniengewirr. **Durch Wände anzeigen** ist eine Anzeigehilfe; eine sichtbare Linie durch eine Wand beweist keine begehbare Verbindung. Die Viewer-Kamera ist kein Test der Spielerphysik.

### Wegpunkte mit der Maus bearbeiten

1. **Bearbeiten** über der Karte oder **Wegnetz** im Projektbaum wählen. Der vorhandene Stand wird als Entwurf geöffnet.
2. **Verschieben** wählen, einen Pfosten anklicken und mit gedrückter linker Maustaste auf den Boden ziehen. Loslassen übernimmt die Position. Esc oder Loslassen ohne gültigen Boden bricht die Gestik ab.
3. Mit **+ Punkt** eine Bodenfläche anklicken. Ein neuer Punkt besitzt zunächst keine automatische Verbindung.
4. Mit **Verbinden** zuerst den Start, dann das Ziel anklicken. **Beide Richtungen** erzeugt Hin- und Rückweg. Ohne diesen Haken entsteht nur die gewählte Richtung.
5. Mit **Trennen** eine Verbindung oder zwei Punkte wählen. **Löschen / Entf** entfernt die Auswahl. **Strg+Z** macht eine Änderung rückgängig; **Strg+Umschalt+Z / Strg+Y** stellt sie wieder her.
6. **Speichern** sichert den Entwurf. **Prüfen** bewertet ihn. **Ins Spiel übernehmen** lädt diesen Stand in das Testspiel.

Rechts stehen die ausgewählte Source-ID, Radius und Flags. Ein vorhandener nativer Wegpunktindex und die WAY-UID werden getrennt angezeigt: Der Index ist die Position in der aktuellen nativen Datei, die Source-ID identifiziert den Projektpunkt, die UID ist die gespeicherte Omni-bot-Identität. Ohne nativen Index wird keiner geraten. **Genaue Koordinaten / Verbindungs-IDs** erlaubt Änderungen per Zahl. **Radius / Flags übernehmen** schreibt die Eigenschaften des ausgewählten Wegpunkts; **Koordinaten übernehmen** schreibt dessen Position. Das Anklicken eines Spawns deaktiviert die Eigenschaften eines vorher ausgewählten Wegpunkts.

**Wegpunktname** und **Blickrichtung · X Y Z** gehören ebenfalls zu **Radius / Flags übernehmen**. Die Blickrichtung ist ein Richtungsvektor, etwa `1 0 0`; `0 0 0` entfernt die Vorgabe. Bei Sprung-, Leiter- und Türaktionen bestimmt das native Rezept die Richtung, daher bleibt dieses Feld gesperrt. Namen, gewöhnliche Blickrichtungen und importierte positive WAY-UIDs bleiben im Projekt, in Undo und im nativen Metadatenvertrag erhalten. Die Ingame-Darstellung zeigt an der Auswahl Namen, Radius, Flags und Aktionswerte; ein roter Strahlenstern markiert die Ankunftstoleranz, ein blauer Pfeil die Blickrichtung.

### Radien, Flags, Sprünge und Leitern

Der Radius ist die Ankunftstoleranz eines Bots. Er ist weder Körperbreite noch Abstand zwischen Punkten. Gewöhnliche Wegpunkte erlauben wie das native DMS 10–300; importierte Werte wie 150 bleiben im Entwurf, beim Speichern und beim Prüfkandidaten unverändert. 35 ist ein brauchbarer Ausgangswert; auf Freiflächen können 60–100 helfen, an engen Übergängen etwa 10–20. Größere Radien müssen dieselben vollständigen nativen Flächen-/Körperprüfungen bestehen. Sprung-, Leiter-, Abstiegs- und Türrezepte behalten ihren bisherigen Aktionsschutz und ihre präzisen Radien, höchstens 100. Überlappende Ankunftsradien sind nicht automatisch ein Fehler. Die native Prüfung kann einen gewünschten Radius geometrisch begrenzen; die Eigenschaften zeigen den wirksamen Wert und den Grund.

| Flag | Verwendung |
| --- | --- |
| **CROUCH** | Ducken an einem niedrigen Durchgang. |
| **JUMP / JUMPGAP / JUMPLOW** | Sprungübergang beziehungsweise spezielle Sprungart. Start, Ziel und gerichtete Verbindung müssen zur Bewegung passen. |
| **CLIMB** | Leiterführung. Einstieg, Führung, Ausstieg und Rückweg müssen zur tatsächlichen Leiter passen. |
| **SPRINT / SNEAK / PRONE** | Bewegungsverhalten; nur dort einsetzen, wo Mod und Pfad es unterstützen. |
| **TEAM1 / TEAM2 / TEAMONLY** | Teambezogene Wege. In TCE2 entsprechen die ersten beiden Teams Terrorists und Specops. |
| Weitere Flags | Alle registrierten Omni-bot-Bits bleiben auswählbar und kombinierbar. ET-Waffen-, Klassen- und Scriptflags erzeugen keine fehlenden TCE2-Fähigkeiten. |

Die Generierung sucht erforderliche Sprünge, Leitern und Duckstellen aus Geometrie und Spielrückmeldung. Ein gesetztes Flag allein baut keinen ausführbaren Übergang. Auch automatisch erzeugte Aktionspunkte und ihre Links lassen sich bearbeiten und bleiben bei Export/Übernahme erhalten; eine ungültige Bewegung wird mit konkreter Stelle zurückgewiesen. Hoch-/Runterklettern und Sprünge danach mit Bots beobachten.

Eine Einbahnverbindung wird nur in ihrer vorhandenen Richtung geprüft. Sie erzwingt keinen Rückweg, beweist aber auch nicht automatisch einen gültigen Sprung oder Abstieg. Kontrollierte Abstiege verwenden die echte TCE2-Fallphysik samt Landung und Schaden; eine LKW-Höhe ist nicht pauschal zu hoch. Freiklettern an einer Kante ist eine eigene TCE2-Bewegung mit Leertaste/Sprung und Blick nach oben. Der Generator unterscheidet diesen Kantenaufstieg von einer echten Leiter und speichert ein gerichtetes Zielrezept; ein CLIMB-Flag allein reicht nicht. Beim Prüfen, Speichern und Übernehmen bleiben bestätigte Aktionsrezepte erhalten.

Eine echte Spawnplattform darf einen geprüften Einbahn-Abstieg ins Hauptnetz besitzen. NavStudio belegt dafür den Spieler-Spawn aus BSP und laufender Engine sowie die vollständige Abstiegs- und Anschlusskette. Eine isolierte gewöhnliche Punktgruppe erhält diese Ausnahme nicht. CAMP-, Attack-, Defend- und Missionsziele werden im gemeinsam erreichbaren Hauptnetz ausgewählt; ein Missionskontakt außerhalb dieses Netzes bleibt eine konkrete offene Aufgabe.

Für einen Sprung aus dem Stand den Startpunkt auswählen und rechts **Stand-Sprungziel** auf den verbundenen Landepunkt stellen, dann **Radius / Flags übernehmen** drücken. Die Liste enthält nur ausgehende Nachbarn. Der Start erhält ein präzises Rezept mit kleinem Aktionsradius; die Prüfung bewertet Sprunghöhe, Hindernisse und Landung. **Kein Stand-Sprung** entfernt dieses Rezept. Beim Löschen des Landepunkts wird auch die zugehörige Zielreferenz entfernt; Undo stellt beides wieder her. Der spezielle Aktionsradius ist keine Empfehlung für gewöhnliche Wegpunkte.

Für TCE2-Freiklettern den Startpunkt auswählen, zuerst den gerichteten Anschluss zum höheren Landepunkt herstellen und unter **TCE2-Freikletterziel** dieses Ziel wählen. **Radius / Flags übernehmen** speichert das Rezept. Es setzt die nötigen Stand-/CLIMB-Eigenschaften; **Prüfen** untersucht die tatsächliche Bewegung. **Kein Freiklettern** entfernt das Rezept. Normale Leiterpunkte behalten ihre CLIMB-Flags auch bei Radiusänderungen. Sprung und Freiklettern sind verschiedene Startaktionen und verwenden jeweils ein eigenes Rezept.

### Aktuelle Fehler finden und beheben

**Offene Stellen** unter der Karte betrifft den aktuellen Stand. Ohne aktuelle Prüfung steht dort ein Strich statt einer vermeintlich bestätigten Null. Nach einer Änderung werden frühere Fehler ausgeblendet, bis derselbe Entwurf erneut geprüft wurde. **Kamera hin** fährt die zugehörige Position an. Falls eine alte ID im aktuellen Entwurf nicht mehr existiert, nicht nach dieser alten ID weiterarbeiten.

| Anzeige | Nächster Schritt |
| --- | --- |
| **Prüfung offen / Änderungen noch nicht geprüft** | **Prüfen**. |
| **Kein Ausgang / Kein Zugang / Hin- oder Rückweg fehlt** | **Kamera hin**, Verlauf ansehen, **Automatisch reparieren** oder sinnvoll gerichtete Links ergänzen; erneut prüfen. |
| **Grundlage geändert** | Projekt speichern. Quelle/Map prüfen und einen neuen Generierungsstand anlegen; alte Bearbeitung bleibt gesichert. |
| **Native Prüfung fehlgeschlagen** | Konkrete Meldung beachten. Der fehlgeschlagene Stand wird nicht durch einen früheren erfolgreichen Prüfstand ersetzt. |
| **Übernahme nicht bestätigt** | Spiel-/Arbeitsstatus prüfen. Es wird kein Erfolg angenommen. |

Verworfene Boden-, Dach- oder Kollisionsproben in **Diagnose / Erweitert** sind getrennt von den offenen Verbindungen. Ihre Menge ist nicht die Anzahl fehlerhafter aktiver Wegpunkte. Geometrie- und Laufzeitgrenzen bleiben dort sichtbar.

**Türdiagnosen** im aktuellen Prüfbericht zeigen verriegelte Türen, fehlende oder unklare Schalterzuordnungen sowie noch nicht bestätigte Schwenkbereiche und Öffnungszeiten. Hin- und Rückrichtung derselben Tür werden zusammengefasst. **Kamera hin** fährt den belegten Türpunkt an; fehlt dessen Position im Bericht, ist der Knopf deaktiviert. Bei veralteter Prüfung werden die früheren Fälle ausgeblendet. Ein ausgewählter Türpunkt oder dessen Verbindung zeigt Modell, Aktivator, Eintritt/Austritt, Bedienpunkt und Teamfreigabe im Inspektor. Diese Angaben sind Geometrie- und Rezeptnachweise; erfolgreiche Botbedienung bleibt eine separate Liveprüfung.

### Goals und Mapscripts

**Gespeicherte Goals** zeichnet die echte `_MG`-Tabelle aus der passenden GM-Datei im BSP-Viewer. GT2/GT5 wählen ihre eigenen Dateien; ein gemeinsames Legacy-Paar wird nur verwendet, wenn beide GT-Dateien fehlen und beide Legacy-Dateien vorhanden sind. Grüne Marker zeigen Goals und Radien, blaue Pfeile deren AimVectors. Anklicken zeigt die gespeicherten Eigenschaften, zum Beispiel Stance, Timeout und Use, und die Datei samt SHA256. **Omni-bot-Texte / Eigenschaften** schaltet die Kartenbeschriftung um. Goalmarker und Radien liegen genau an der gespeicherten GM-Position wie im nativen Omni-bot-Renderer. Es gibt keinen pauschalen Körperoffset: Ingame-Anlegen benutzt den unteren Rand der Spielerhülle; explizite Positionsänderungen oder Missionsziele können andere Weltpositionen verwenden. **Active** und scriptgesteuerte Änderungen sind Laufzeitdaten und werden aus einer Datei nicht geraten. Änderungen im Live-Editor erst mit **Save Goals** sichern; anschließend wird die Studio-Ansicht erneuert.

TCE2 verwendet ein gemeinsames `<map>.way` und getrennte Gametype-Paare, zum Beispiel `<map>_gt2.gm` + `<map>_gt2_goals.gm` sowie `<map>_gt5.gm` + `<map>_gt5_goals.gm`. Normales ET verwendet `<map>.gm` + `<map>_goals.gm` ohne GT-Suffix und bleibt bei Objective GT2. Das Projekt zeigt deshalb **ET-Goals** statt zweier TCE2-Gametype-Paare. **GM ist GameMonkey**, nicht Lua und nicht das ET-Mapscript `.script`.

1. **Goals GT2 + GT5 generieren** erzeugt TCE2-Ziele auf Basis der aktuell geprüften Navigation und Mapaufgaben. Bereits importierte oder von Hand geänderte Dateien werden nicht ungefragt ersetzt.
2. **Goals · GT2** oder **Goals · GT5** öffnet die zugehörigen Daten. Der Dialog enthält Goal-Daten, `.gm`-Maplogik und das Map-Script aus der PK3. Für den gewählten Gametype wird etwa `maps/obj_delta_gt5.script` gesucht; ein gemeinsames Legacy-Script kann als Fallback angezeigt werden.
3. Textänderungen mit **Skript speichern** sichern. Das führt den GM-Code nicht aus. **Paket als ZIP** exportiert die vorhandenen direkten Begleitdateien.
   Nicht gespeicherter Scripttext verhindert das Schließen des App-Fensters. Erst speichern oder im Dialog mit **Erneut laden** verwerfen.
4. Zuerst **Ins Spiel übernehmen** für das Wegnetz verwenden. Danach oben links den gewünschten Test-Gametype 2 oder 5 wählen und **Goals für GT… ins Spiel übernehmen** drücken. Diese Aktion sichert das alte Paar, übernimmt das Projektpaar, startet dieselbe Testinstanz mit diesem Gametype neu und prüft geladene Namen/Maplogik. Die WAY muss zum aktuellen Prüfnachweis passen.

**Omni-bot Live-Editor** bedient das laufende Spiel. Goal-Auswahl, Position und Fadenkreuz stammen vom Spieler im Spiel, nicht von der Studio-Kamera. Für Live-Änderungen Goals anzeigen, auswählen, bearbeiten und **Save Goals** verwenden. Bei bestätigtem Save übernimmt Studio die neue Laufzeitdatei in das Projekt und aktualisiert die Ansicht. Nicht gespeicherte Studio-Wegpunktänderungen müssen vorher gesichert werden. Nach einem Live-WAY-Save ist die bisherige Prüfung ungültig; erneut prüfen. Nicht gespeicherter Text im Scriptdialog wird nicht überschrieben. `waypoint_load` lädt nur Wege und aktiviert keine geänderte `.gm`-Maplogik.

Die Studio-Goalebene zeigt gespeicherte Daten. Bei SWITCH wird `Facing` als blauer Richtungsvektor mit Länge `Facing × 32` gezeichnet. Eine nichtleere `AimPosition` hat wie im nativen Render-Script Vorrang und ist ein absolutes gelbes Ziel. Der Start liegt 64 Units über der Goalposition, beim Ducken 24 und im Liegen 50 Units tiefer. Gespeicherte Werte werden dadurch nicht verändert; aktuelle Laufzeitaktivierung und dynamische Ziele werden daraus nicht abgeleitet.


Für ein eigenes taktisches Goal im Live-Editor:

Im lokalen TCE2-Testspiel öffnet das **Voicemenü → 8. TCE2 Omni-bot DMS** den Editor; beim Buchstaben-Voicemenü ist es **O**. Der Editor gehört ausschließlich zur optionalen `zz_tce2_navstudio_editor.pk3` der Testinstallation. Das normale TCE2-Menü und öffentliche Modpakete enthalten keinen Editor-Eintrag. Im DMS funktionieren Maus und Zahlentasten; **V** führt zurück, **=** zur nächsten Seite und **Z/Escape** gibt die Eingabe sofort an das Spiel zurück. **N** öffnet Namen und eigene Zahlenwerte. Der Studio-Live-Editor bietet denselben nativen Katalog, einschließlich SWITCH, Timeout in Millisekunden, Use, Stance, Rollen, Triggerboxen, Wegpunktnamen und allen registrierten Flags. ET-Ausrüstungsgoals und ET-Testklassen werden nur im ET-Modus angeboten. **Goal Help** zeigt, welche Eigenschaften der ausgewählte Goal-Typ unterstützt.

1. Das Testspiel öffnen und den Spieler an die gewünschte Position bringen. **Goals anzeigen** wählen.
2. Unter **Benanntes Goal erstellen** einen Typ wie `CAMP`, `Snipe`, `Attack` oder `Defend` wählen, einen Namen aus Buchstaben/Ziffern/Unterstrich eingeben und **Goal erstellen** drücken.
3. Ein vorhandenes Goal mit **Nächstes Goal bearbeiten** oder **Goal im Fadenkreuz** auswählen. **Goal zum Spieler** verschiebt es an die Spielerposition; **Goal verschieben: Start / Ende** beginnt/beendet das Verschieben im Spiel.
4. Radius, Gruppe, Blickrichtung und Campzeiten über die angebotenen Eigenschaften einstellen. **Bearbeitung beenden**, dann **Save Goals** drücken und die Bestätigung abwarten.
5. Das Goal im gewählten Gametype tatsächlich von Bots benutzen lassen. Eigene taktische Goals ersetzen keine fehlenden Missionskontakte oder Trigger.

Die TCE2-Generierung verteilt ATTACK-/DEFEND-Stellungen über nativ erreichbare Bereiche und begrenzt ihre gleichzeitige Besetzung. Bei sechs verfügbaren Bots können zwei verteidigen, zwei Flanken besetzen und zwei die Mission verfolgen; Träger und akute Bombenaktionen haben Vorrang. Mehrere Zugänge werden nur als getrennte gerichtete Korridore übernommen, mit gemeinsam genutzten ROUTE-Ankern (höchstens 48 zusätzliche Anker). Gibt es keinen belegten zweiten Zugang, wird kein zufälliger Umweg erzeugt. Manuell bearbeitete oder importierte Goals werden nicht überschrieben; für den neuen Plan die generierten GT-Goals erneut erzeugen und übernehmen.

Die Missionsziele stammen aus dem tatsächlich laufenden Gametype. GT2 kann je nach Mapscript Flaggen-/Capture-Aufgaben oder Sabotage mit Sprengzielen enthalten; GT5 nutzt seine aktiven Bombenziele. Vom Script entfernte BSP-Objekte werden nicht wieder als Goals erfunden. Ein vorhandenes, aber noch nicht unterstütztes aktives Missionsziel wird mit Entity und Ziel gemeldet und sperrt den fertigen Export.

Generierte taktische Ziele und ein erfolgreicher Ladecheck bestätigen noch nicht, dass Bots die gesamte Mission lösen. Capture, Plant, Defuse, Trigger und Routen im gewählten Gametype live prüfen.

### BSP-Spawns und eigene GT7-Spawns

**Spawns** im Projektbaum fährt den Spawn-Bereich rechts an. Blaue Körpermarker sind die BSP-Spawns und bleiben unveränderlich. Grüne Marker sind erzeugte eigene Spawns; die Liste zeigt Herkunft und Klasse. **Spawns in der Karte anzeigen** steuert diese Ebene.

1. Das aktuelle Wegnetz **Prüfen**.
2. Im Spawn-Bereich **Generieren** drücken. Erreichbare Standflächen werden als zusätzliche Spawnpositionen ausgewählt.
3. Einen grünen Spawn auswählen. Mit der linken Maustaste ziehen oder X/Y/Z und Blickrichtung ändern, dann **Verschieben**. **Löschen** entfernt nur eigene Spawns.
4. **Eigene GT7-Spawns nutzen** aktivieren, um die eigenen Deathmatch-Spawns zu verwenden; deaktivieren für Standardspawns.
5. **GT7-Script speichern** prüft die eigenen Spawns im Spiel, ergänzt das aus der PK3 abgeleitete `<map>_gt7.script`, sichert ältere Dateien und übernimmt es nach `mapscripts` der Testinstallation. Dabei wird GT7 verwendet und die Map neu geladen.

Das Script nutzt `g_mapScriptDirectory "mapscripts"`, `tce_dmspawns 1/0` und die geprüften `tce_dmspawn`-Zeilen. Spawnkoordinaten sind Spielerorigins; Wegpunktkoordinaten im Editor sind Fußpunkte. Beim Ziehen berücksichtigt Studio den Unterschied automatisch. Fehlt ein geeignetes Original-Mapscript oder liegt ein Spawn im Hindernis, meldet die Aktion das Problem und bestätigt keine erfolgreiche Übernahme.

Eigene GT7-Spawns gehören zu TCE2. Im ET-Projekt werden die BSP-Spawns angezeigt; die TCE2-GT7-Schreibaktionen bleiben gesperrt. Auch während eines laufenden Prüfauftrags werden Spawns nicht verschoben. Schlägt das Speichern einer Mausverschiebung fehl, zeigt die Karte wieder die tatsächlich gespeicherte Position.

### Sichtbar mit Bots testen

1. Aktuelles Wegnetz prüfen und **Ins Spiel übernehmen**.
2. Gewünschten Gametype wählen; nötige Goals übernehmen. TCE2-Gametypewechsel erfolgen über einen Neustart, nicht während derselben laufenden Map. Normales ET bleibt hier bei Objective GT2.
3. Die Map im sichtbaren Spiel ansehen. Das Spiel ist jederzeit normal bedienbar: Maus, Tastatur, Menü und Bot-Zuschauen bleiben verfügbar. NavStudio sperrt keine Spielfenster und deaktiviert keine Eingaben.
4. Falls noch keine Bots vorhanden sind, in der Spielkonsole `/bot addbot 1 1 Test_Terrorist` und `/bot addbot 2 1 Test_Specop` ausführen. Danach die Wegpunktanzeige im Live-Editor einschalten, Bots beim Gehen, Kämpfen und Erfüllen der Mapaufgaben beobachten. Engstellen, Räume, Treppen, Leitern, Sprünge, Wasser und Abgründe prüfen. Screenshots aus passenden Blickwinkeln mit `/screenshotJPEG navtest_01` aufnehmen und ansehen. Sie liegen unter `tce2/screenshots` beziehungsweise `omnibot/screenshots` in der Testinstallation.
5. Gefundene Fehler im Projekt korrigieren, erneut prüfen und übernehmen. **Spiel beenden** schließt dieselbe Instanz.

Der Starter verwendet `devmap <map>`, `sv_official 0`, `bind n noclip`, `bind k kill`, Respawnzeit 1, Limbozeiten 1000 und `timelimit 999`. Er führt kein `unbindall` aus. Rendererwerte bleiben `r_maxpolyverts 16384` / `r_maxpolys 4096`; die Ingame-Wegpunktdistanz ist auf 800 begrenzt. Die Studio-Sichtweite ist ein separater Viewerwert.

**Geometrie erzeugt** bedeutet: Vorschlag aus der Map. **Nativ geprüft** bedeutet: der genaue Stand wurde im Spiel mit passendem Körperprofil geprüft. **Im Spiel geladen** bedeutet: die Übernahme wurde bestätigt. Keiner dieser drei Status ersetzt eine **Live-Abnahme mit Bots**. Ein verbundenes Netz allein beweist weder vollständige Flächenabdeckung noch erfolgreiche Missionslogik. Auch bei 300 Karten ist die Prüfung pro Map/Mod/Gametype erforderlich; eine allgemeine Fehlerfreiheitsgarantie wird nicht angezeigt.

### Automatische Türen in TCE2

**Netz generieren** untersucht normale Schiebe- und Drehtüren sowie eindeutig zugeordnete Schalter. Türpunkte enthalten ein gerichtetes Rezept: Aktivierungsort, Blickziel, Türmodell, Teamfreigabe und Durchgangsziel. Bei einem entfernten Schalter läuft der Bot über vorhandene geprüfte Wege dorthin, betätigt ihn und kehrt zur Tür zurück. Er wartet auf die tatsächliche Öffnung und prüft den Durchgang vor dem Weiterlaufen. Eine geschlossene Tür wird nicht einfach aus der Laufzeitkollision entfernt.

Schalterketten über `target_relay` werden nur bei eindeutigem Ziel und passender Freigabe verwendet. Verschlossene Türen, nicht erreichbare Schalter, mehrdeutige Ziele oder zusätzliche Script-/Schießeffekte erhalten einen konkreten Prüfgrund. Der Generator führt zum Prüfen keine Mapaktionen aus und schaltet keine Sperren ab. Bei zeitgesteuerten Türen sind Wiederholungen begrenzt; ein abgewiesener Durchgang wird neu geplant.

Türrezepte bleiben bei Speichern, WAY-Import und erneuter nativer Prüfung erhalten. Wird ein benötigter Punkt gelöscht, eine Aktionsverbindung getrennt oder das DOOR-Flag entfernt, werden zugehörige Verweise entfernt; **Rückgängig** stellt sie wieder her. Neu generierte Rezepte bleiben bis zum echten Bot-Test als live ungeprüft ausgewiesen.

**Automatisch reparieren und prüfen** ergänzt Türrezepte auch im bereits bearbeiteten Netz. Vorhandene Punkte, Namen, Flags und Verbindungen bleiben erhalten. Neue Türpunkte erhalten neue IDs; gelöschte IDs werden nicht wiederverwendet. Bei Drehtüren mit fester Öffnungsrichtung muss der Bedien-/Wartepunkt zusätzlich außerhalb des gesamten Türblattschwenks liegen. Die gewöhnliche Aktion **Prüfen** fügt keine gelöschten Türpunkte wieder ein.

Die Reparatur sucht außerdem begrenzt nach fehlenden lokalen Anschlüssen, wenn der vorhandene Hin- und Rückweg einen großen Umweg macht. Auch diese Vorschläge müssen anschließend die vollständige native Körper-/Bodenprüfung bestehen; Wände, Wasser und Absturzkanten werden nicht übergangen.

### Einstellungen und Verbindungsfehler

Für Pfadänderungen erst Projekt schließen. **Einstellungen** öffnet Spielordner, `ET.exe`/`etl.exe` und zusätzliche Mapordner. Die Spiel-EXE muss direkt im gewählten Installationsordner liegen; dieser benötigt `etmain` und `tce2`. **Speichern** übernimmt die geprüften Pfade. Es wird keine zweite Spielinstallation angelegt.

Wenn die Testinstanz nicht antwortet: Start-/Auftragsstatus abwarten. Bei einem hängenden eigenen Testspiel **Spiel beenden**, danach **Testspiel starten**. NavStudio prüft die eigene Prozessidentität und startet bei unklarem Zustand nicht blind einen zweiten Prozess. Wenn die App-Verbindung unterbrochen ist, die Oberfläche mit **↻** neu laden; nötigenfalls NavStudio neu öffnen und das gespeicherte Projekt laden. Bereits gesendete Schreibaufträge werden nach einer verlorenen Antwort nicht automatisch doppelt ausgeführt.

### Paket und Source-Start

Das fertige Paket benötigt kein separat installiertes Python. Windows verwendet die installierte WebView2-Laufzeit. `Start.cmd` direkt neben der EXE prüft die Paketdateien vor dem Start. Der Paketbau kontrolliert Oberfläche, Hilfe, Projekt-/Spawn-/Goal-Module, Offline-Worker, WebView2-DLLs und den ausgewählten aktuellen Recast-Build und schreibt `navstudio-package.json`.

Für Source-Arbeit Python-Abhängigkeiten aus `requirements-desktop.txt` installieren und `src/tools/navstudio/entry.py` starten. `src/tools/navstudio/Start.cmd` bevorzugt aktuelle Quellen mit vorhandenen Desktop-Abhängigkeiten. Ein EXE-Fallback wird nur verwendet, wenn dessen Paketmanifest zu den aktuellen Quellen passt; eine alte EXE wird nicht stillschweigend geöffnet. Bei einem veralteten Paket das aktuelle Paket bauen/installieren oder den Source-Start verwenden.

```powershell
python -m pip install --target build/navstudio-desktop-deps -r src/tools/navstudio/requirements-desktop.txt
$env:PYTHONPATH = 'build/navstudio-desktop-deps'
python src/tools/navstudio/entry.py --workspace C:/Users/micha/Documents/GitHub/true-combat
```

`--no-browser` ist der ausdrücklich gewählte Dienstmodus für Entwickler/Automation und öffnet keine Oberfläche. Normaler EXE-Start öffnet das Desktop-Fenster. Der lokale Dienst nutzt Port 8766 und die bestehende Spielinstanz Port 27986. Projekte, Generierung, Prüfung und Übernahme benötigen weder Konsolenbefehle noch manuelle Datei-Uploads.

## English

### Start

1. Start `build/navgen-overkill/TCE2-NavStudio.exe`. It opens its own desktop window.
2. Choose **Create new project** or open a saved project from the list. **View current project** joins the project already used by a running batch.
3. Choose a map first. Opening the application does not start a game or select a fixed default map.

Keep the EXE and `_internal` together next to `ET.exe` or `etl.exe` in the existing installation. Data folders are `etmain`, `tce2`, `omni-bot` and, for regular ET, `omnibot`. When moving the app, copy the complete package including `Start.cmd`, `verify-package.ps1` and `navstudio-package.json`; retain the existing game data and projects. **Guide** opens this help inside the application. **English / Deutsch** switches UI language without changing map names or project data.

### Create a project with the wizard

1. Enter a project name, for example **Delta – test network**. Names support up to 100 characters.
2. Choose **TCE2** or **Enemy Territory**. TCE2 lists `obj_` maps and uses `fs_game tce2`. ET lists other maps, uses `fs_game omnibot` and is fixed to **Objective GT2** in Studio. TCE2 GT5 is the bomb mode; gametype numbers are not interchangeable between the mods.
3. Select a map. Search filters its name. Entries identify **all_maps** packages or installed maps. Choose the gametype.
4. Choose **Generate automatically** or **Import existing network**. Generation requires an available validation adapter; follow the displayed note if it is disabled. Import copies existing `.way` and matching `.gm` files into the project without changing their originals.
5. Click **Create project**. Generation opens the map in the existing visible test installation. Imported data is first displayed in the viewer and does not acquire a new validation status merely by being imported.

For TCE2, the wizard can generate **GT2 and GT5 goals** after successful network validation. Regeneration starts a new graph and archives the previous authored draft. Use **Edit** or **Repair automatically** for individual corrections.

### Map library and project files

Put complete map PK3s in `etmain/all_maps`. In this repository this is `C:/Users/micha/Documents/GitHub/true-combat/etmain/all_maps`. Opening a project borrows its package into the test installation's `etmain`. **Close** saves the project, stops its own test game and returns the borrowed package to **all_maps**. Existing base packages remain installed.

The project remains saved after its map is returned. Reopen it to resume and provide the map again. A BSP alone does not supply its required textures, models and mapscripts.

Project folders are `projects/<project-ID>/` next to the app. `project.json` stores name, map and mod; `preview/` geometry and candidates; `native/` validated reports and WAY; `bundles/` companion files; `work/` drafts, spawns and evidence. The actual project path appears below its name. An internal ID keeps equally named projects separate.

### Layout and actions

The workflow toolbar stays at the top. Project, game and display controls are on the left, the map in the centre, properties and spawns on the right. Current unresolved locations appear directly below the map. The complete list grows with the page; it has no inner scroll box and does not reduce the map viewport. **Diagnostics / Advanced** contains technical tools, discarded samples and source evidence.

| Action | Result |
| --- | --- |
| **Save** | Saves the project and current edits; does not change the running graph. |
| **Generate network** | Reads BSP/map models, creates Recast and the graph, validates it in the game and attempts up to two repair passes. |
| **Validate** | Checks the exact current draft/candidate and automatically starts/uses the existing test installation; handles saving and export. |
| **Repair automatically** | Attempts additional paths and checks the result again. |
| **Apply to game** | Installs/reloads the currently validated WAY in the same test installation; edits are saved, exported and checked first. |
| **Start test game** | Opens the project map visibly, starting the same existing installation if needed. |
| **Close game** | Closes the owned test game, including an unresponsive game port. |
| **Close** | Saves/closes the project and returns a borrowed map. |

Watch the job status until it completes or fails. A failed application is not considered loaded. **↻**, **F5** or **Ctrl+R** reloads the UI while retaining the open project and draft.

### Camera and display

| Input | Action |
| --- | --- |
| Hold right mouse + move mouse | Rotate camera. |
| Hold right mouse + W A S D | Move forward, left, backward, right. |
| Hold right mouse + E / Q | Up / down. |
| Shift / Ctrl while moving | Faster / slower. |
| Drag middle mouse | Pan sideways or vertically. |
| Mouse wheel | Move forward / backward. |
| **F** with a selection | Focus the selection. |
| **Eye level** | Place the view at player eye height at the selected waypoint. |
| **Fly camera** | Click the view; WASD moves, Space/C goes up/down, Esc releases the mouse. |

**Select** returns to inspection/edit mode. Display controls toggle BSP, waypoints, edges, standing/crouching navmesh and height slices. **Selected only** reduces edge clutter. **X-ray** is a display aid: a line visible through a wall does not prove that bots can traverse it. The viewer camera is not a player-physics test.

### Edit waypoints with the mouse

1. Click **Edit** above the map or **Waypoint network** in the project tree to open the graph as a draft.
2. Choose **Move**, click a post and drag it on the floor with the left mouse button. Release to apply. Esc or release without a valid floor cancels the gesture.
3. With **+ Point**, click the floor. A new point initially has no automatic connection.
4. With **Connect**, click the source then the target. **Both directions** creates both links; unchecked creates the selected direction only.
5. **Disconnect** accepts an edge or two points. **Delete** removes the selection. **Ctrl+Z** undoes; **Ctrl+Shift+Z / Ctrl+Y** redoes.
6. **Save** retains the draft, **Validate** checks it, and **Apply to game** loads that exact version in the test game.

The selected Source-ID, radius and flags appear on the right. A known native waypoint index and WAY UID are shown separately: the index is the position in the current native file, the Source-ID identifies the project node, and the UID is its saved Omni-bot identity. An unknown native index is never guessed. **Exact coordinates / edge IDs** supports numeric editing. Apply radius/flags and coordinate changes with their respective buttons. Selecting a spawn disables the properties of a previously selected waypoint.

**Waypoint name** and **Facing · X Y Z** also use **Apply radius / flags**. Facing is a direction vector, such as `1 0 0`; `0 0 0` clears it. Native jump, ladder and door recipes determine their own facing, so that field is disabled for action nodes. Names, ordinary facing and imported positive WAY UIDs are retained in projects, Undo and the native metadata contract. The in-game view shows selected names, radius, flags and action properties; red beams mark arrival tolerance and blue arrows show facing.

### Radius, flags, jumps and ladders

Radius is the bot's arrival tolerance, not its body width or waypoint spacing. Ordinary waypoints support the native DMS range 10–300; imported values such as 150 remain unchanged in the draft, saved project and validation candidate. Start at 35; open space may benefit from 60–100 and tight transitions from about 10–20. Larger requests must pass the same full native area/body checks. Jump, ladder, drop and door recipes retain their existing action protection and precise radii, at most 100. Overlapping tolerances are not automatically errors. Native checks may geometrically limit a requested radius; properties show the effective value and reason.

| Flag | Use |
| --- | --- |
| **CROUCH** | Low passage requiring crouching. |
| **JUMP / JUMPGAP / JUMPLOW** | Jump or specialised jump transition; source, target and directed edge must match the motion. |
| **CLIMB** | Ladder routing with a matching entry, guides, exit and return path. |
| **SPRINT / SNEAK / PRONE** | Movement behaviour where supported by the mod and route. |
| **TEAM1 / TEAM2 / TEAMONLY** | Team-related routes; TCE2's first teams are Terrorists and Specops. |
| Other flags | All registered Omni-bot bits remain combinable. ET weapon/class/script flags do not invent missing TCE2 abilities. |

Generation searches for necessary jump, ladder and crouch transitions using geometry and game feedback. A flag alone does not build a usable transition. Generated action points and links are editable and retained on export/application; invalid motion is rejected with a concrete location. Observe actual bots climbing and jumping afterwards.

A one-way link is checked only in its existing direction. It does not require a reverse link, but does not by itself prove a valid jump or drop. Controlled drops use actual TCE2 falling physics, landing and damage; truck height is not rejected by a fixed limit. Ledge climbing is a separate TCE2 movement using jump input and upward aim. The generator distinguishes it from actual ladders and stores a directed target recipe; CLIMB alone is insufficient. Validation, saving and applying preserve verified action recipes.

A real spawn platform may have a validated one-way drop into the shared map. Studio binds the player spawn to the BSP and running engine and validates the complete drop and exit chain. An ordinary isolated group does not receive this exception. CAMP, attack, defend and mission destinations stay in the mutually reachable core; a mission contact outside it remains an explicit unresolved task.

For a jump from rest, select its launch node, choose the connected landing node under **Standing jump target**, then click **Apply radius / flags**. Only outgoing neighbours are listed. The launch receives a precise recipe with a small action radius; validation checks jump height, obstacles and landing. **No standing jump** removes that recipe. Deleting its landing node removes the target reference too; Undo restores both. This special action radius is not a recommendation for ordinary waypoints.

For TCE2 ledge climbing, select the launch point, connect its directed route to the higher landing point, then select that target under **TCE2 ledge-climb target**. **Apply radius / flags** saves the recipe and sets the required standing/CLIMB properties. **Validate** checks actual movement. **No ledge climb** removes the recipe. Ordinary ladder CLIMB flags survive radius edits. Standing jumps and ledge climbs use distinct launch recipes.

### Find and resolve current errors

**Unresolved locations** below the map refers to the current version. Without a current report, a dash is shown instead of an apparently validated zero. Edits hide older errors until that exact draft is validated again. **Move camera here** focuses the problem. Do not keep working from an old ID that no longer exists in the current draft.

| Message | Next step |
| --- | --- |
| **Validation pending / Changes not validated** | **Validate**. |
| **No exit / No entrance / Missing return route** | Focus the location, inspect the route, **Repair automatically** or add appropriate directed links, then validate. |
| **Source changed** | Save the project, check its map/source and regenerate; older authored work remains backed up. |
| **Native validation failed** | Follow the concrete message. The failed version is not replaced by an older successful report. |
| **Application not confirmed** | Check the game/job status; success is not assumed. |

Discarded floor/roof/collision samples in **Diagnostics / Advanced** are separate from actual unresolved edges. Their number is not a count of broken active waypoints. Geometry/runtime limitations remain visible there.

**Door diagnostics** in the current report show locked doors, missing or ambiguous button bindings, and unconfirmed sweep areas or opening times. Both directions of the same door are grouped. **Move camera here** focuses the documented door waypoint; the button is disabled when its position is absent. Stale reports hide older cases. Selecting a door waypoint or its connection shows model, activator, entry/exit, use waypoint and team permission in the inspector. These are geometry/recipe checks; successful bot activation still requires separate live testing.

Rotating doors may require a separate safe activation point. The local staged recipe keeps **USE → ENTRY → EXIT**: activate beside the sweep, wait until the leaf is fully open, align with the doorway, then cross. Its route begins at USE; ENTRY cannot be approached directly from another ordinary waypoint. A remote switch remains a separate activation mode. These recipes retain the actual activation distance and current door identity when edited or revalidated.

### Goals and mapscripts

**Saved goals** draws the actual `_MG` table from the matching GM file in the BSP viewer. GT2/GT5 select their own files; a common legacy pair is used only when both GT files are absent and both legacy files exist. Green markers show goals/radii and blue arrows their AimVectors. Click a marker to inspect saved properties such as Stance, Timeout and Use, and the exact file/SHA256. **Omni-bot labels / properties** toggles map text. Goal markers and radii use exactly the saved GM position, matching the native Omni-bot renderer. No body offset is guessed: in-game creation uses the bottom of the player's bounds, while explicit position changes and mission targets may use other world positions. **Active** and scripted changes are runtime data and are never guessed from a file. Save live edits with **Save Goals** to refresh the Studio view.

TCE2 shares `<map>.way` and uses separate gametype pairs: `<map>_gt2.gm` + `<map>_gt2_goals.gm`, and `<map>_gt5.gm` + `<map>_gt5_goals.gm`. Regular ET uses `<map>.gm` + `<map>_goals.gm` without a GT suffix and stays at Objective GT2. Its project therefore shows **ET goals** instead of two TCE2 gametype pairs. **GM is GameMonkey**, distinct from Lua and from the engine's `.script` mapscript.

1. **Generate GT2 + GT5 goals** creates TCE2 targets from current validated navigation and map objectives. Imported or manually edited files are protected from silent overwrite.
2. **Goals · GT2 / GT5** opens the matching data. The dialog exposes saved goals, `.gm` map logic and the PK3 mapscript. The selected gametype chooses a script such as `maps/obj_delta_gt5.script`, with a common legacy fallback where applicable.
3. Use **Save script** for text edits. Saving does not execute GM. **Export ZIP** packages the existing direct companion files.
   Unsaved script text prevents closing the app window. Save it first or discard it with **Reload** in the dialog.
4. Apply the network first with **Apply to game**. Choose test gametype 2 or 5 on the left, then **Apply GT… goals to game**. This backs up the old pair, deploys the project pair, restarts the same installation in that gametype and verifies loaded names/map logic. The WAY must match current validation evidence.

The **Omni-bot live editor** controls the running game. Selection, position and aim come from the in-game player, not the Studio camera. Show goals, select/edit them and use **Save Goals**; on confirmed saves, Studio copies the new runtime file into the project and refreshes its view. Save pending Studio waypoint edits first. A live WAY save invalidates the previous validation; validate again. Unsaved script-dialog text is never overwritten. `waypoint_load` reloads routes only and does not activate changed `.gm` logic.

Studio's goal layer shows saved data. For SWITCH, `Facing` is a blue direction of length `Facing × 32`. A nonzero `AimPosition` takes precedence, matching the native render script, and is an absolute yellow target. The start is 64 units above the goal position, lowered by 24 for crouch or 50 for prone. Drawing changes no saved values and does not infer current runtime activation or dynamic targets.


To create a custom tactical goal in the live editor:

In a local TCE2 test game, **Voice menu → 8. TCE2 Omni-bot DMS** opens the editor (**O** in the letter-based voice menu). It is supplied only by the optional `zz_tce2_navstudio_editor.pk3` in the test installation; public mod packages have no editor menu entry. DMS pages support mouse clicks and number keys; **V** goes back, **=** opens the next page and **Z/Escape** immediately returns input to the game. **N** opens name fields and custom numeric values. Studio's live editor exposes the same native catalog, including SWITCH, Timeout in milliseconds, Use, Stance, roles, trigger boxes, waypoint names and every registered flag. ET equipment goals and ET test classes are offered only in ET mode. Use **Goal Help** for the properties supported by the selected goal type.

1. Open the test game and place its player at the intended location. Choose **Show goals**.
2. Under **Create named goal**, choose a type such as `CAMP`, `Snipe`, `Attack` or `Defend`, enter a name using letters/digits/underscore, and click **Create goal**.
3. Select an existing goal using **Edit nearest** or **Edit aimed goal**. **Move to player** moves it to the player position; **Move goal: start / end** toggles movement in the game.
4. Set radius, group, facing and camp times with the available properties. Finish editing, then click **Save Goals** and wait for confirmation.
5. Observe bots using the goal in the selected gametype. Tactical goals do not replace missing mission contacts or triggers.

TCE2 generation distributes ATTACK/DEFEND stations across natively reachable areas and limits concurrent occupancy. Six eligible bots can split into two defenders, two flankers and two mission actors; carriers and urgent bomb actions take precedence. Multiple approaches require distinct directed corridors and reuse a small ROUTE anchor network (at most 48 additional anchors). A single proven approach does not produce a random detour. Manually edited or imported goals are not overwritten; regenerate and apply generated GT goals to use the new plan.

Tactical goal generation and a successful load check do not prove mission completion. Test capture, plant, defuse, triggers and routes in a live match with the chosen gametype.

### BSP spawns and custom GT7 spawns

**Spawns** in the project tree scrolls to the spawn editor. Blue markers are immutable BSP spawns. Green markers are generated custom spawns; the list shows source and class. **Show spawns in map** controls this layer.

1. **Validate** the current network.
2. Click **Generate** in the spawn panel to select reachable standing surfaces.
3. Select a green spawn. Drag it with the left mouse or change X/Y/Z and facing, then **Move**. **Delete** affects custom spawns only.
4. Enable **Use custom GT7 spawns** for the generated deathmatch spawns; disable it for standard spawns.
5. **Save GT7 script** validates the custom positions in the game, extends the PK3-derived `<map>_gt7.script`, backs up older files and deploys it to the test installation's `mapscripts`. The action uses GT7 and reloads the map.

The script uses `g_mapScriptDirectory "mapscripts"`, `tce_dmspawns 1/0` and validated `tce_dmspawn` lines. Spawn coordinates are player origins; editor waypoints are foot positions. Studio maintains the offset while dragging. Missing source scripts or obstructed positions produce concrete failures, never a false success confirmation.

Custom GT7 spawns belong to TCE2. ET projects display BSP spawns while the TCE2 GT7 write actions remain disabled. Spawn movement is also disabled during validation jobs. If saving a drag fails, the view returns to the actual saved position.

### Visible bot testing

1. Validate the network and **Apply to game**.
2. Choose the gametype and apply required goals. TCE2 gametype changes require a restart. Regular ET stays at Objective GT2 here.
3. Inspect the visible game. Mouse, keyboard, menus and bot spectating are always available. NavStudio does not lock game windows or disable game input.
4. If no bots are present, use `/bot addbot 1 1 Test_Terrorist` and `/bot addbot 2 1 Test_Specop` in the game console. Then enable waypoints in the live editor and observe bots walking, fighting and performing objectives. Check tight entrances, rooms, floors, stairs, ladders, jumps, water and drops. Capture appropriate screenshots with `/screenshotJPEG navtest_01` and inspect them. They are stored under the test installation's `tce2/screenshots` or `omnibot/screenshots`.
5. Fix findings in the project, validate/apply again, and close that same game using **Close game**.

Startup uses `devmap <map>`, `sv_official 0`, `bind n noclip`, `bind k kill`, respawn 1, limbo 1000 and `timelimit 999`; it never runs `unbindall`. Renderer values stay `r_maxpolyverts 16384` / `r_maxpolys 4096`; in-game waypoint distance is 800. Studio's draw distance is a separate viewer setting.

**Geometry generated** means a map-derived candidate. **Natively validated** means that exact version passed the game's checks with the matching body profile. **Loaded in game** means its application was confirmed. These do not replace **live bot acceptance**. A connected graph does not prove complete area coverage or working mission logic. Even across 300 maps, acceptance belongs to each map/mod/gametype; the program does not claim universal perfection.

### Automatic TCE2 doors

**Generate network** discovers ordinary sliding/rotating doors and unambiguous switch bindings. Directed recipes retain the activation position, aim, brush identity, team permission and passage target. For a remote switch, bots use existing validated graph paths to reach the switch and return to the entry. They wait for the actual opening and check the passage before moving through it; runtime collision is never ignored.

Bounded `target_relay` chains require an unambiguous target and valid permissions. Locked doors, inaccessible switches, ambiguous targets and additional script/shoot effects remain explicit diagnostics. Generation does not execute map actions or remove locks. Closing timers allow bounded retries followed by replanning.

Saving, WAY import and native revalidation preserve the recipe. Deleting a referenced point, removing its action edge or clearing DOOR clears dependent references; undo restores them. Generated actions remain live-unverified until actual bot testing.

### Settings and connection failures

Close the project before changing paths. **Settings** selects the existing installation, its `ET.exe`/`etl.exe` and additional map directories. The EXE must be directly in that installation, which needs `etmain` and `tce2`. **Save** validates/stores paths without creating another game installation.

If the game does not respond, wait for startup/job status. For a stuck owned game choose **Close game**, then **Start test game**. Studio checks process identity and never blindly starts a second instance. For a lost app connection use **↻**, or reopen NavStudio and load the saved project. A write request with a lost reply is not automatically sent twice.

### Package and source usage

The packaged app includes Python and uses Windows WebView2. `Start.cmd` beside the EXE validates its files before startup. The builder checks UI/help, project/spawn/goal modules, offline workers, WebView2 DLLs and the selected current Recast executable, then writes `navstudio-package.json`.

For source work install `requirements-desktop.txt`, then launch `src/tools/navstudio/entry.py`. `src/tools/navstudio/Start.cmd` prefers current source with available desktop dependencies. It only falls back to an EXE whose manifest matches the checkout; it does not silently open an old executable. Rebuild/install a stale package or use source startup. The PowerShell example in the German section installs dependencies locally under `build/navstudio-desktop-deps`.

`--no-browser` is explicit service-only mode for developers/automation; normal EXE startup opens the desktop GUI. The local service uses port 8766 and the existing game port 27986. Project setup, generation, validation and application require no console commands or manual file upload.
