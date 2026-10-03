# Arbeitsauftrag: TC:E aus den Originalmodulen rekonstruieren

## Ziel und Dateien

Ziel ist ein aus diesem SDK baubarer, funktionsfaehiger TC:E-Mod fuer Enemy
Territory, zunaechst Windows x86. Ein kompilierendes ET-SDK erfuellt das Ziel nicht.
Originalmodule/Assets liegen lokal in `../tcetest/`, ET-Referenzen in `../etmain/`.
In GhidraMCP sind `cgame`, `qagame` und `ui` als Windows-DLL und Linux-i386-SO
geoeffnet. Linux-Symbole zur Namensfindung nutzen; Windows-Verhalten gesondert
pruefen. Keine Binaerdateien oder Spielassets in die Quellen einbetten.

## Bei jeder Fortsetzung zuerst lesen

1. `reconstruction/STATUS.md`: nachgewiesener Gesamtstand und Grenzen.
2. `reconstruction/WORKLOG.md`: letzte Arbeit, aktive Funktion, naechster Schritt.
3. `reconstruction/FUNCTIONS.md`: Zusammenfassung und Links zu Modulchecklisten.
4. `reconstruction/functions.json`: massgebliche maschinenlesbare Statusliste.

## Verbindlicher Ablauf pro Funktion

1. Eine Funktion aus der Arbeitswarteschlange waehlen. In `functions.json`
   den Status auf `in_progress` setzen, dann Checklisten generieren.
2. Modul, Adresse, Linux-Symbol, Aufrufer, globale Daten, Strukturen und
   SDK-Gegenstueck untersuchen. Unklare Typen/Offsets als unklar kennzeichnen.
3. Belege unter `reconstruction/evidence/` speichern. Original-DLL/SO-Hashes
   beachten. Kein Nachweis allein durch Namensgleichheit oder Kompilierbarkeit.
4. Lesbaren C-Code in `game/`, `cgame/` bzw. `ui/` schreiben. Abhaengigkeiten
   rekonstruieren; keine erfundenen Stubs als fertige Implementierung ausgeben.
5. Betroffene Module bauen; Verhalten moeglichst mit Originalfunktion vergleichen.
   Grenzen der Tests festhalten. Rekonstruktion und Einbindung getrennt behandeln.
6. Erst nach Implementierung, erfolgreichem Build und dokumentierter
   Verhaltenspruefung `verified` setzen. `source`, `evidence`, `validation`
   und `integration` ausfuellen. Ein Haken bedeutet verifizierte Funktion,
   nicht spielbarer Mod. Bestehende SDK-Funktionen ebenfalls erst abgleichen.
7. `python src/tools/reconstruction_tracker.py sync` aus dem Repository-Stamm
   ausfuehren: abgeleitete Checklisten aktualisieren. Danach `STATUS.md` und
   `WORKLOG.md` sofort nachziehen; offene Risiken/naechste Funktion notieren.

Keine gemeinsame Funktion automatisch in allen Modulen abhaken. Jedes
Binaermodul hat seinen eigenen Status. Aliasnamen am selben Einstiegspunkt
duerfen denselben Funktionsdatensatz verwenden. Thunks und Laufzeitfunktionen
in der Liste belassen; einen Ausschluss erst mit Beleg begruenden.
`functions.json` nie aus einer neu erzeugten Inventur blind ueberschreiben:
das Tracker-Skript fuegt neue Adressen hinzu und erhaelt vorhandene Arbeit.
Die Markdown-Checklisten werden generiert und nicht von Hand bearbeitet.

## Ghidra und Referenzen

- Programmliste und Metadaten vor Adressannahmen lesen; ELF-Dateien wurden
  bisher mit Imagebase 0x10000 geladen. `st_value` ist nicht die Ghidra-Adresse.
- Interne Funktionsinventare fuer alle sechs Module unter
  `reconstruction/inventory/ghidra/` speichern, inklusive Erfassungsdatum und
  Metadaten. MCP-Seiten vollstaendig durchlaufen; Ressourcen koennen abgeschnitten
  sein. Anzahl interner Funktionen und externer Importe nicht verwechseln.
- Ghidra-Namen nur bei belegter Zuordnung aendern und das Programm speichern.
- `../build/reference/` enthaelt benannte Decompilerreferenzen aus einem
  separaten Ghidra-Projekt. Sie sind Analysehilfe, kein baubarer C-Code.
- Neue Erkenntnisse lokal unter `src` festhalten, nicht nur im Chat/Ghidra.

## Build und Git

Alle Rekonstruktionsarbeiten erfolgen auf Branch `dev`. `master` bleibt
unveraendert: dort keine Commits, Merges, Resets oder Pushes durchfuehren.
Vor Git-Aenderungen den aktuellen Branch pruefen. Branchwechsel uebertragen
uncommittete Aenderungen; deshalb nicht mit laufender Arbeit zu `master` wechseln.

`src/build.ps1` baut mit CMake/VS2022 Win32 und fuehrt die Originalvergleiche aus.
Nach jedem erfolgreichen Build der drei Module die DLLs in `../tce2/` ablegen:
das ist der vom Nutzer festgelegte neue `fs_game`. `src/build.ps1` erledigt dies
nach den Tests und prueft SHA-256. Auch nach manuellen CMake-Builds diesen
Deploy-Schritt ausfuehren. `tce2/build-info.json` dokumentiert den Teststand.
Der Nutzer hat das wiederholte Ersetzen dieser drei Test-DLLs ausdruecklich
beauftragt. Keine erneute Freigabe einholen; `tcetest/` bleibt Referenz.
Ohne lokale Referenzbinaries: `cmake -S src -B build/sdk-win32 -A Win32 -DBUILD_TESTING=OFF`.
Originale in `tcetest/` niemals mit Teilrekonstruktionen ueberschreiben.
Keine historischen VS2003-Projekte fuer den neuen Build verwenden.

Nur `src/` und die Root-`.gitignore` gehoeren in neue Commits. Binaries, Assets,
Builds und Ghidra-Projekte bleiben lokal. `gitignore` entfernt bereits verfolgte
Dateien nicht aus dem Index. Lokale Referenzdateien nicht loeschen. Nicht pushen,
History umschreiben oder einen fertigen Mod behaupten, ohne dass dies beauftragt
bzw. durch End-to-End-Pruefungen belegt ist.
