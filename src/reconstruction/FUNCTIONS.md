# Funktionscheckliste TC:E

Generiert aus `functions.json`; Status dort bzw. mit dem Tracker aendern.

`[x]` = rekonstruiert und verhaltensgeprueft. Einbindung separat beachten.
`[ ]` = offen/in Arbeit oder begruendet ausgeschlossen. Keine automatische
Uebertragung des Status zwischen Modulen oder Plattformen.

Umfang: alle intern von Ghidra erfassten Funktionen der sechs Module,
ergaenzt um alle definierten ELF-Funktionssymbole. Gleiche Einstiegspunkte
mit Aliasnamen bilden einen Eintrag. Externe Bibliotheksimporte haben
keinen Funktionskoerper in diesen Binaries und werden nicht portiert.
Weitere durch Analyse entdeckte Funktionen werden beim Sync hinzugefuegt.

| Modul | Erfasst | Offen | In Arbeit | Verifiziert | Ausgeschlossen |
|---|---:|---:|---:|---:|---:|
| [cgame.mp.i386.so](checklists/cgame.mp.i386.so.md) | 1852 | 1852 | 0 | 0 | 0 |
| [cgame_mp_x86.dll](checklists/cgame_mp_x86.dll.md) | 1405 | 1391 | 0 | 14 | 0 |
| [qagame.mp.i386.so](checklists/qagame.mp.i386.so.md) | 3754 | 3754 | 0 | 0 | 0 |
| [qagame_mp_x86.dll](checklists/qagame_mp_x86.dll.md) | 1681 | 1681 | 0 | 0 | 0 |
| [ui.mp.i386.so](checklists/ui.mp.i386.so.md) | 1258 | 1258 | 0 | 0 | 0 |
| [ui_mp_x86.dll](checklists/ui_mp_x86.dll.md) | 603 | 603 | 0 | 0 | 0 |

## Aktive Arbeit

Keine aktive Funktion. Naechsten Schritt in [WORKLOG.md](WORKLOG.md) lesen.

Die Zaehler messen Eintraege, nicht den Fertigstellungsgrad des Mods.
Belege, Tests und Integrationsstatus stehen in `functions.json`.
Gesamtstand: [STATUS.md](STATUS.md). Arbeitsjournal: [WORKLOG.md](WORKLOG.md).
