/* Local UI language; never translate map IDs, paths, user input or log evidence. */
(function () {
  'use strict';
  var pairs = `ET: originaler Omni-bot-Mod. Ansicht und Spieltest verfügbar; native Generierung benötigt einen ET-Prüfadapter.|ET: original Omni-bot mod. Viewer and game testing available; native generation requires an ET validation adapter.
TCE2: obj_-Maps mit TCE2-Prüfung.|TCE2: obj_ maps with TCE2 validation.
Keine Karten für diesen Spielmodus gefunden.|No maps found for this game mode.
Lokalen Entwurf anlegen; vorhandene WAY bleibt erhalten.|Create a local draft; existing WAY remains unchanged.
Keine abgelehnten Einträge im geladenen Datensatz. Das allein belegt keine vollständige Mapabdeckung.|No rejected entries in this dataset. This alone does not prove complete map coverage.
Links: auswählen / Werkzeug · Rechts ziehen: drehen · Mitte ziehen: verschieben · Mausrad: vor/zurück · Auswahl auch durch Wände|Left: select / tool · Right drag: rotate · Middle drag: pan · Wheel: forward/back · Selection through walls supported
LOKALES WERKZEUG|LOCAL TOOL
Verbinde …|Connecting …
Anleitung ↗|Guide ↗
⚙ Einstellungen|⚙ Settings
01 / MAP & GEOMETRIE|01 / MAP & GEOMETRY
Map auswählen|Select map
Lade lokalen Projektstand …|Loading local project …
Map im Testspiel öffnen|Open map in test game
Spielstatus wird gelesen …|Reading game status …
Testspiel beenden|Close test game
02 / NAVIGATION ERZEUGEN|02 / GENERATE NAVIGATION
Toolgestützt erzeugen & prüfen|Generate & validate
Netz, Goals & Spawns automatisch erstellen|Automatically create graph, goals & spawns
Wegpunkte, Goals & Mapscripts als ZIP exportieren|Export waypoints, goals & mapscripts as ZIP
Wegpunkte, Goals und Mapscripts sind bereit.|Waypoints, goals and mapscripts are ready.
Zuerst ein Map-Projekt öffnen.|Open a map project first.
Zuerst Netz, Goals und Spawns automatisch erstellen.|Automatically create graph, goals and spawns first.
ZIP wird vorbereitet …|Preparing ZIP …
Auftrag läuft. ZIP-Export anschließend verfügbar.|A task is running. ZIP export will be available afterwards.
Kein unterstützter Gametype mit Mapscript vorhanden.|No supported gametype has a mapscript.
Öffnet die Map, erzeugt das Netz und prüft es nativ. Bis zu zwei gezielte Nachbesserungen; keine automatische Installation.|Opens the map, generates the graph and validates it in the game. Up to two repair passes; no automatic installation.
Nur Recast & Wegnetz erzeugen|Generate Recast & waypoint graph only
Nativ prüfen|Validate in game
Pfade nachbessern|Repair paths
Mapmodell-Zustand erfassen|Capture map model state
Bereit|Ready
03 / EBENEN|03 / LAYERS
Darstellung|View
Ingame-Wegpunkte|In-game waypoints
Recast & Prüfung|Recast & validation
Magenta Pfosten = Wegpunkte · Weiß = gerichtete Verbindungen. Ansicht sofort sichtbar, kein Spielstart nötig.|Magenta posts = waypoints · White = directed links. View available without starting the game.
BSP-Darstellung|BSP view
Navmesh · Stehen|Navmesh · Standing
Navmesh · Ducken|Navmesh · Crouching
Exportierte Kollision|Exported collision
Wegpunkte|Waypoints
Gespeicherte Goals|Saved goals
Omni-bot-Texte / Eigenschaften|Omni-bot labels / properties
Goals werden für den gewählten Gametype aus der GM-Datei gelesen.|Goals are read from the selected gametype's GM file.
Gespeichertes Goal · Active und dynamische Spielzustände sind nur im laufenden Spiel belegt.|Saved goal · Active and dynamic game state are only confirmed by the running game.
Spielerorigin · GM Position|Player origin · GM Position
Fußpunkt · Darstellung|Foot position · display
GM-Position · unverändert|GM position · unchanged
Wegpunktname|Waypoint name
Radius · gewöhnlich 10–300|Radius · ordinary 10–300
Nativer Wegpunktindex|Native waypoint index
Blickrichtung · X Y Z|Facing · X Y Z
Akzeptierte Prüfungen|Accepted checks
Abgelehnte Prüfungen|Rejected checks
Durch Wände anzeigen|Show through walls
Gerichtete Verbindungen|Directed links
Alle Verbindungen|All links
Nur Auswahl · übersichtlich|Selection only
Ausblenden|Hide
Sichtweite der Wegpunkte|Waypoint view distance
600 Units · direktes Umfeld|600 units · Nearby area
Unbegrenzt|Unlimited
Z von|Z from
bis|to
Nur aktuelle Kamerahöhe ±96 Units|Current camera height ±96 units only
Navmesh ist ein Geometriekandidat. Erst die native Prüfung bewertet den TCE2-Spielerhull.|The navmesh is a geometry candidate. Native validation checks the actual TCE2 player hull.
04 / INSTALLIEREN|04 / INSTALL
Stand prüfen|Inspect current graph
In vorhandener Testinstanz nutzen|Use in existing test instance
Geprüfte WAY in TCE2 installieren …|Install validated WAY in TCE2 …
Installation setzt einen aktuellen, erfolgreich auditierten Stand voraus.|Installation requires a current, successfully audited graph.
RAUMANSICHT|3D VIEW
Auswählen|Select
Augenhöhe|Eye level
Kamera fliegen|Fly camera
Bearbeiten|Edit
↖ Verschieben|↖ Move
+ Punkt|+ Point
↔ Verbinden|↔ Connect
✂ Trennen|✂ Disconnect
Beide Richtungen|Both directions
Löschen|Delete
Bearbeiten öffnet einen lokalen Entwurf. Rechts ziehen: Kamera drehen · Mausrad: vor/zurück.|Edit opens a local draft. Right-drag: rotate camera · Mouse wheel: move forward/back.
Map wird geladen|Loading map
Lokale BSP-Geometrie …|Local BSP geometry …
Klick: Punkt oder Kante auswählen · Auswahl auch durch Wände möglich|Click to select a point or edge · Selection through walls is available
Verbindungen|Links
Abgelehnt|Rejected
Abgelehnt im Netz|Rejected in graph
Bodenproben ohne Routenbeleg|Ground samples without a verified route
Proben ·|samples ·
Bereiche|areas
Einzelne Bodenproben anzeigen|Show individual ground samples
Verworfene Geometrieproben ·|Discarded geometry samples ·
Bodenproben ohne bestätigte Route. Darunter können Dächer, Mapränder und nicht spielbare Flächen sein; vollständige Mapabdeckung bleibt ungeprüft.|Ground samples without a validated route. These may include roofs, map borders and inaccessible surfaces; full map coverage remains unverified.
Historischer Generierungsbericht. Er belegt nicht den Anschlusszustand des aktuellen Entwurfs.|Historical generation report. It does not validate connectivity of the current draft.
Genauer Standort fehlt im Generierungsbericht; Mapabdeckung bleibt offen.|Exact location is missing from the generation report; map coverage remains unverified.
Keine unverbundenen Bodenproben in diesem Generierungsbericht.|No unconnected ground samples in this generation report.
Keine verworfenen Geometrieproben in diesem Generierungsbericht.|No discarded geometry samples in this generation report.
Der Scanner prüft auch Hindernisse, Decken und direkte Wege durch Wände. Verworfene Proben bleiben im Generierungsbericht erhalten. Fehler im aktuellen Wegnetz stehen unter „Ablehnungsgründe“.|The scanner also tests obstacles, ceilings and direct paths through walls. Discarded samples stay in the generation report. Errors in the current graph appear under “Rejection reasons”.
Navmesh-Polygone|Navmesh polygons
Prüfstand|Validation status
WEGPUNKTE BEARBEITEN|EDIT WAYPOINTS
Lokalen Entwurf anlegen / öffnen|Create / open local draft
Änderungen werden als Entwurf gespeichert und erst nach neuer nativer Prüfung installiert.|Changes are saved as a draft and installed only after fresh native validation.
Entwurf anzeigen|Show draft
Punkt im Bild auswählen. Zum Verschieben mit linker Maustaste ziehen.|Select a point in the view. Drag with the left mouse button to move it.
Standard · 0|Default · 0
Ducken · 64|Crouch · 64
Radius / Flags übernehmen|Apply radius / flags
Stand-Sprungziel|Standing jump target
Kein Stand-Sprung|No standing jump
TCE2-Freikletterziel|TCE2 ledge-climb target
Kein Freiklettern|No ledge climb
Freiklettern an einer Kante: Leertaste/Sprung und Blick nach oben. Keine Leiter. Ein gerichtetes Zielrezept wird nativ geprüft.|Ledge climb: press Space/jump and look upward. Not a ladder. Native validation checks the directed target recipe.
Nur verbundene Ziele in Sprungrichtung. Der Startpunkt erhält ein genaues Stand-Sprungrezept; Bewegung und Landung werden nativ geprüft.|Only connected targets in the jump direction. The launch node receives a precise standing jump recipe; movement and landing are validated in game.
Ausgewählten Punkt löschen|Delete selected point
Ring am Fuß zeigt den Entwurfsradius. Native Prüfung kann ihn geometrisch begrenzen. Alte gewöhnliche Radien unter 10 werden sichtbar auf 35 umgestellt. Leiter-/Sprungguides sind gesperrt und werden beim Prüfen neu erzeugt.|The ring at the foot shows the draft radius. Native validation may constrain it. Old ordinary radii below 10 are reset to 35. Ladder/jump guides are locked and regenerated during validation.
Genaue Koordinaten / Verbindungs-IDs|Exact coordinates / link IDs
Z · Fuß|Z · Feet
Fußpunkt im BSP wählen|Pick floor point in BSP
Kamerafuß übernehmen|Use camera feet
Koordinaten übernehmen|Apply coordinates
Punkt an Koordinaten hinzufügen|Add point at coordinates
Von-ID|From ID
Nach-ID|To ID
Auswahl als Von|Selection as source
Auswahl als Nach|Selection as target
Beide Richtungen ↔|Both directions ↔
Verbinden|Connect
Verbindung löschen|Delete link
Entwurf speichern|Save draft
Als nativen Prüfkandidaten exportieren|Export candidate for native validation
JSON herunterladen|Download JSON
Sichern & archivieren|Save & archive
Danach links|Then use
. Zum Vergleich „Entwurf anzeigen“ ausschalten. Speichern/Export allein ersetzt keine WAY-Datei.|on the left. Disable “Show draft” to compare. Saving/exporting alone does not replace a WAY file.
INSPEKTOR|INSPECTOR
Wegpunkt-ID direkt anfahren|Go to waypoint ID
Anfahren|Go to
Einen Punkt auswählen|Select a point
Prüfergebnis, Koordinaten und Nachbarverbindungen werden hier angezeigt.|Validation results, coordinates and neighboring links appear here.
Auswahl mit Kamera anfahren|Move camera to selection
LOKALE VERBINDUNGEN|LOCAL LINKS
Noch keine lokalen Verbindungsprüfungen geladen.|No local link checks loaded yet.
ABLEHNUNGSGRÜNDE|REJECTION REASONS
Noch kein nativer Prüfbericht geladen.|No native validation report loaded yet.
DATENQUELLEN|DATA SOURCES
Die BSP-Ansicht zeigt Renderflächen. Exportierte Kollision enthält den erfassten Zustand der Mapmodelle. Bewegliche Modelle können ihren Zustand später ändern.|The BSP view shows render surfaces. Exported collision captures map model state. Moving models may change later.
Auftragsprotokoll|Job log
Noch keine Aufträge.|No jobs yet.
LOKALE INSTANZ|LOCAL INSTANCE
Einstellungen|Settings
Die bestehende Spielinstallation wird direkt verwendet. Es wird keine zweite Installation angelegt. Vor dem Ändern Testspiel und laufende Aufträge beenden.|The existing game installation is used directly. No second installation is created. Close the test game and finish running jobs before changing paths.
Ordner der Testinstanz (absoluter Pfad)|Test instance folder (absolute path)
Spiel-EXE direkt in diesem Ordner|Game executable directly in this folder
Zusätzliche Mapordner (ein absoluter Pfad je Zeile)|Additional map folders (one absolute path per line)
Neu laden|Reload
Speichern|Save
Schließen|Close
TCE2-NAVIGATION INSTALLIEREN|INSTALL TCE2 NAVIGATION
Geprüften Stand übernehmen?|Install validated graph?
Die erzeugte WAY-Datei für|The generated WAY file for
wird in TCE2 installiert. Der Server prüft vorher erneut den Auditstand.|will be installed in TCE2. The server checks the audit again first.
Zur Bestätigung INSTALL eingeben|Type INSTALL to confirm
Abbrechen|Cancel
Installieren|Install
Ansicht aktualisieren|Refresh view
Studio bereit|Studio ready
Verbindung unterbrochen|Connection lost
Abgeschlossen|Complete
Fehlgeschlagen|Failed
Wartet|Queued
Native Prüfung veraltet|Native validation outdated
Nativ geprüft · Live-Abnahme offen|Natively validated · Live testing pending
Kandidat · native Prüfung offen|Candidate · Native validation pending
Bearbeitungsentwurf · nicht geprüft|Draft · Not validated
BSP geladen · Navigation fehlt|BSP loaded · Navigation missing
wartet|queued
läuft|running
abgeschlossen|complete
fehlgeschlagen|failed
Neues Projekt|New project
Projekt öffnen|Open project
Speichern|Save
Schließen|Close
Netz generieren|Generate network
Prüfen|Validate
Automatisch reparieren|Repair automatically
Ins Spiel übernehmen|Apply to game
Testspiel starten|Start test game
Spiel beenden|Close game
Das Testspiel ist jederzeit normal bedienbar.|The test game always accepts normal player input.
Map laden startet bei Bedarf dieselbe sichtbare Testinstanz. Das Testspiel ist jederzeit normal bedienbar. Die BSP-Ansicht funktioniert weiterhin offline.|Loading a map starts the same visible test instance when needed. The game always accepts normal player input. The BSP viewer also works offline.
PROJEKT|PROJECT
Kein Projekt geöffnet|No project open
Wegnetz|Waypoint network
Spawns|Spawns
Map-Script|Map script
Omni-bot Live-Editor|Omni-bot live editor
⚑ Goals GT2 + GT5 generieren|⚑ Generate GT2 + GT5 goals
SPIEL UND GAMETYPE|GAME AND GAMETYPE
ARBEITSSTATUS|WORK STATUS
DARSTELLUNG|DISPLAY
PUNKT-EIGENSCHAFTEN|POINT PROPERTIES
SPAWNS|SPAWNS
Spawns in der Karte anzeigen|Show spawns in map
BSP = Blau · eigene Spawns = Grün. Eigene Spawns mit der Maus auswählen und ziehen.|BSP = blue · custom spawns = green. Select and drag custom spawns with the mouse.
Spawn auswählen|Select spawn
Verschieben|Move
Blickrichtung|Facing angle
Eigene GT7-Spawns nutzen|Use custom GT7 spawns
Generieren|Generate
GT7-Script speichern|Save GT7 script
Eine Map. Ein Projekt.|One map. One project.
Wegnetz, Goals und Spawns werden zusammen gespeichert. Wähle ein vorhandenes Projekt oder lege ein neues an.|Waypoint network, goals and spawns are saved together. Open an existing project or create a new one.
Neues Projekt erstellen|Create new project
Spielpfad / Einstellungen|Game path / settings
Vorhandene Projekte|Existing projects
Noch keine Projekte. Links ein neues Projekt erstellen.|No projects yet. Create a new project on the left.
Neues Map-Projekt|New map project
Name|Name
Spiel|Game
Map|Map
Navigation|Navigation
Projektname|Project name
Jedes Projekt erhält einen eigenen Ordner. Andere Karten und ihre Prüfergebnisse bleiben getrennt.|Each project has its own folder. Other maps and their validation results stay separate.
Für welches Spiel?|Which game?
obj_-Maps, Gametype 2 / 5 / 7|obj_ maps, gametype 2 / 5 / 7
ET-Maps mit originalem Omni-bot-Mod|ET maps using the original Omni-bot mod
Map aus all_maps oder der Installation|Map from all_maps or the installation
Gametype|Gametype
Automatisch generieren|Generate automatically
BSP + Recast, Sprünge, Leitern, Duckstellen und native Prüfung|BSP + Recast, jumps, ladders, crouch passages and native validation
Vorhandenes Netz importieren|Import existing network
.way und passende .gm-Dateien in dieses Projekt übernehmen|Import .way and matching .gm files into this project
Goals für GT2 und GT5 erstellen|Create GT2 and GT5 goals
Die Generierung öffnet die Map in der bestehenden sichtbaren Testinstanz. Restfehler erscheinen direkt unter der Kartenansicht.|Generation opens the map in the existing visible test instance. Remaining errors appear directly below the map viewport.
Zurück|Back
Weiter|Next
Projekt erstellen|Create project
Langsam|Slow
Normal|Normal
Schnell|Fast
Kamera|Camera
Änderungen noch nicht geprüft. Alte Fehler ausgeblendet; „Prüfen“ prüft den aktuellen Entwurf.|Changes have not been validated. Old errors are hidden; Validate checks the current draft.
Noch kein Prüfbericht: „Prüfen“ untersucht das aktuelle Netz.|No validation report yet: Validate checks the current network.
Aktuelle Prüfung fehlgeschlagen|Current validation failed
Diagnose / Erweitert|Diagnostics / Advanced
Prüfproben, technische Werkzeuge und Quelldateien. Offene Verbindungen stehen direkt unter der Karte.|Validation samples, technical tools and source files. Unresolved connections appear directly below the map.
Abgelehnte Prüfproben|Rejected validation samples
Offene Stellen|Unresolved locations
Laufendes Projekt anzeigen|View current project
Goals für GT2 ins Spiel übernehmen|Apply GT2 goals to game
Goals für GT5 ins Spiel übernehmen|Apply GT5 goals to game
Goals für GT7 ins Spiel übernehmen|Apply GT7 goals to game
Goals übernehmen startet die Map mit dem ausgewählten Gametype neu. Vorher das geprüfte Wegnetz ins Spiel übernehmen.|Applying goals restarts the map with the selected gametype. Apply the validated waypoint network to the game first.
ET-Goals|ET goals
Türdiagnosen|Door diagnostics
Tür|Door
Türaktion|Door action
Türrezept|Door recipe
Türmodell|Door model
Tür-Entity|Door entity
Aktivierung|Activation
Direkt an der Tür|Directly at the door
Über zugeordneten Schalter|Through the assigned button
Aktivatormodell|Activator model
Aktivator-Entity|Activator entity
Eintrittspunkt|Entry waypoint
Anfahrtspunkt|Approach waypoint
Austrittspunkt|Exit waypoint
Bedienpunkt|Use waypoint
Aktivierungsziel|Activation aim
Teamfreigabe|Allowed teams
Beide Teams|Both teams
Türprüfstand|Door validation status
Schalter / Tür bedienen|Use button / door
Tür betreten|Enter doorway
Tür verlassen|Exit doorway
Native Geometrie geprüft; Botbedienung separat testen.|Native geometry validated; test bot activation separately.
Entwurfsrezept – aktuelle Türprüfung fehlt.|Draft recipe; current door validation is missing.
Durchgang geometrisch geprüft; Botbedienung offen.|Passage geometry validated; bot activation remains untested.
Tür verriegelt oder Schlüssel erforderlich.|Door is locked or requires a key.
Öffnungszeit und Rückweg noch nicht belegt.|Opening time and return route are not yet validated.
Türschwenkbereich noch nicht sicher belegt.|The door sweep area is not yet validated as safe.
Schalterzuordnung fehlt oder ist nicht eindeutig.|Button binding is missing or ambiguous.
Schalter bei geschlossener Tür nicht erreichbar.|Button cannot be reached while the door is closed.
Aktivator nicht sicher anvisierbar.|Activator cannot be aimed at safely.
Türmodell oder Ausgangszustand nicht bestätigt.|Door model or initial state is unconfirmed.
Teamfreigabe nicht bestätigt.|Team permission is unconfirmed.
Kein vollständig bestätigter Türdurchgang.|No fully validated door passage.
Türprüfung veraltet – aktuelle Navigation prüfen.|Door validation is stale; validate the current navigation.
Noch keine aktuelle native Türprüfung.|No current native door validation yet.
Aktueller nativer Prüfstand. Botbedienung separat testen.|Current native validation. Test bot activation separately.
Türsuche durch Prüfbudget begrenzt; weitere Durchgänge können fehlen.|Door search reached its validation budget; further passages may be missing.
Keine genaue Position im Prüfbericht.|No exact position in the validation report.
Türbereich ansehen|View door area
Prüfdetails|Validation details
Keine Türfälle im aktuellen Prüfbericht.|No door cases in the current validation report.
ET-Goals ins Spiel übernehmen|Apply ET goals to game
Normales ET verwendet hier Objective GT2. Goals und Maplogik bilden ein gemeinsames .gm-Paar ohne Gametype-Suffix.|Regular ET uses Objective GT2 here. Goals and map logic use one common .gm pair without a gametype suffix.
Goals übernehmen startet dieselbe ET-Map als Objective GT2 neu. Vorher das geprüfte Wegnetz ins Spiel übernehmen.|Applying goals restarts the same ET map as Objective GT2. Apply the validated waypoint network first.
Abstieg nicht bestätigt: widersprüchliches Landeziel oder ausgeschöpftes natives Aktionsbudget. Die Einbahnverbindung allein beweist keine ausführbare Bewegung.|Drop not confirmed: conflicting landing target or native action budget exhausted. A one-way link alone does not prove executable movement.
Sprungstart mit tatsächlichem Bodenkontakt der Spielerhülle geprüft; auch eine Position knapp an der Kante bleibt gültig.|Jump launch verified with actual player-hull ground contact; a position near the lip can remain valid.
TCE2-Freiklettern: Absprung und Blick nach oben führen an eine höhere Kante. Kein gewöhnlicher Leiterpunkt; Botbedienung separat testen.|TCE2 ledge climb: jump and look upward to reach a higher ledge. This is not an ordinary ladder waypoint; test actual bot traversal separately.
Native Anfahrt zum TCE2-Freikletterpunkt mit realer Spielerhülle geprüft.|Approach to the TCE2 ledge-climb launch verified with the actual player hull.
Geprüfter gewöhnlicher Rückweg vom Freikletterstart; der Aufstieg bleibt gerichtet.|Verified ordinary retreat from the ledge-climb launch; the ascent remains directed.
Gerichteter TCE2-Kantenaufstieg mit Sprungimpuls und Blick nach oben nativ geprüft; die Landung liegt auf begehbarem Boden.|Directed TCE2 ledge climb with jump input and upward aim verified natively; landing is on walkable ground.
Freiklettern nicht ausführbar: Höhe, Körperfreiraum, Kantenkontakt oder Landung passen nicht zur TCE2-Bewegung.|Ledge climb cannot execute: height, body clearance, ledge contact or landing do not match TCE2 movement.
Freiklettern nicht bestätigt: widersprüchliches Zielrezept oder erschöpftes natives Aktionsbudget.|Ledge climb not confirmed: conflicting target recipe or exhausted native action budget.
`;
  var dictionary = Object.fromEntries(pairs.split('\n').map(function (line) { return line.split('|'); }));
  var language = localStorage.getItem('navstudio-language') === 'en' ? 'en' : 'de';
  var originals = new WeakMap();
  function translate(value) {
    var clean = value.trim();
    if (dictionary[clean]) return value.replace(clean, dictionary[clean]);
    if (/^(Bereich \d+|Ohne Gruppierung) · \d+ Bodenproben · Kamera hin$/.test(clean)) return value.replace('Bereich','Area').replace('Ohne Gruppierung','Ungrouped').replace('Bodenproben','ground samples').replace('Kamera hin','Focus camera');
    if (/^Bodenprobe \d+ · Kamera hin$/.test(clean)) return value.replace('Bodenprobe','Ground sample').replace('Kamera hin','Focus camera');
    if (clean.startsWith('Studio bereit ·')) return value.replace('Studio bereit', 'Studio ready');
    if (/^GT[257] · nicht vorhanden$/.test(clean)) return value.replace('nicht vorhanden','unavailable');
    if (/^⚑ Goals GT[25](?: \+ GT[25])? generieren$/.test(clean)) return '⚑ Generate '+clean.slice(8,-11)+' goals';
    if (clean.startsWith('Verfügbar: ')) return value.replace('Verfügbar: ','Available: ').replaceAll('kein passendes Mapscript','no matching mapscript');
    if (clean.startsWith('ZIP zum Speichern bereit: ')) return value.replace('ZIP zum Speichern bereit: ','ZIP ready to save: ');
    if (/^Spiel (läuft|geschlossen|startet)/.test(clean)) return value.replace('Spiel läuft', 'Game running').replace('Spiel geschlossen', 'Game closed').replace('Spiel startet / wartet auf Map', 'Game starting / waiting for map').replace('Spielfenster vorhanden', 'Game window present');
    return value;
  }
  function render() {
    observer.disconnect();
    var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT), node;
    while ((node = walker.nextNode())) {
      if (!node.parentElement || node.parentElement.closest('script,style,pre,textarea,#language-toggle')) continue;
      var old = originals.get(node);
      if (!old || (node.nodeValue !== old.source && node.nodeValue !== old.output)) old = {source: node.nodeValue};
      old.output = language === 'en' ? translate(old.source) : old.source;
      node.nodeValue = old.output; originals.set(node, old);
    }
    document.documentElement.lang = language;
    document.getElementById('language-toggle').textContent = language === 'de' ? 'English' : 'Deutsch';
    observer.observe(document.body, {subtree:true, childList:true, characterData:true});
  }
  var observer = new MutationObserver(render);
  document.getElementById('language-toggle').addEventListener('click', function () {
    language = language === 'de' ? 'en' : 'de'; localStorage.setItem('navstudio-language', language); render();
  });
  render();
}());
