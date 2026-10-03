# Arbeitsjournal

## 2026-10-03 — Waffeninitialisierung und Testordner

- Nutzer legt `tce2/` als neues `fs_game` fest. Nach jedem erfolgreichen
  Dreimodul-Build dorthin deployen; `build.ps1` kopiert und prueft SHA-256.
- BG_InitializeWeaponDef (Windows 0x30006a50) und Tabelle 0x30097db0 portiert.
  56.606 synthetische Vergleiche plus 38 echte Specs bestehen, alle 460 Bytes.
- Unbekannte Strukturfelder bleiben als Offsetnamen bezeichnet; x87-Rundung
  bei Schalldaempfung explizit nachvollzogen. Gesamtstand: 13 verifiziert.
- Release-DLLs gebaut, Tests bestanden und nach `tce2/` kopiert.
- BG_ParseGearDef (0x30007b90) abgeschlossen: 95 Faelle mit sechs Gear-Dateien,
  komplettem Strukturvergleich, Fallback-Reihenfolge und Freigaben. Gesamt: 14.
- Erneuter Release-Build und automatische Ablage aller DLLs in `tce2/` bestanden.
- Keine aktive Funktion. Naechster Schritt: CG_LoadGearDef und Server-Gear-Lader
  untersuchen; danach ABI-Abgleich und Einbindung.

## 2026-10-03 — Entwicklungsbranch

- Branch `dev` von `52122ec81b8d615731a63a348f944a5a77f605c9` erstellt und aktiviert.
- Alle vorbereiteten Aenderungen bleiben auf `dev` zur Weiterarbeit vorgemerkt.
- `master` zeigt unveraendert auf denselben Commit; kein Commit, Merge oder Push.
- Nutzeranweisung: ausschliesslich auf `dev` weiterarbeiten, `master` in Ruhe lassen.

## 2026-10-03 — dauerhafte Funktionsverfolgung

- Nutzer hat alle drei Linux-i386-Module im GhidraMCP-Projekt geoeffnet.
- Alle sechs internen Ghidra-Funktionsinventare mit Metadaten gesichert und mit
  definierten ELF-Symbolen zusammengefuehrt: 10.553 Einstiegspunkte.
- Stabile IDs: `program:adresse`. Aliasnamen teilen einen Einstiegspunkt;
  Module und Plattformen haben getrennte Nachweise und getrennte Haken.
- Bereits verifizierte Windows-Funktionen aus dem ersten Arbeitsstand werden
  mit Quellpfad, Belegen und Testreferenz uebernommen. Integration bleibt offen.
- `.gitignore` auf `src/` plus Root-`.gitignore` begrenzt. Bereits verfolgte
  Nicht-Quellen (326 Dateien) wurden nur aus dem Index entfernt und alle lokal
  erhalten. Quellen und `.gitignore` sind fuer einen spaeteren Commit vorgemerkt.
  Bestehende Git-Historie wird nicht umgeschrieben; kein Commit oder Push.
- Isolierte Kopie nur von `src/` ohne Spielordner erfolgreich mit
  `-DBUILD_TESTING=OFF` gebaut. Funktionsstatus bleibt bei erneutem Sync identisch.
- CTest prueft kuenftig neben den Binaervergleichen auch Vollstaendigkeit,
  Belegpfade und Aktualitaet der Funktionschecklisten.
- Lokale Root-`AGENTS.md` verweist auf die versionierte `src/AGENTS.md` und
  bleibt entsprechend der Ausschlussregel selbst ausserhalb des Git-Index.

## Abgeschlossenes Arbeitspaket

`cgame_mp_x86.dll:30006650` — BG_WeaponIsAvailable.
Abhaengigkeit: `cgame_mp_x86.dll:30003900` — TC:E-Version von BG_WeaponInWolfMP.
Die Zuordnung wurde mit Windows-Decompilat und Linux-Symbolen bestaetigt.
C-Port und Originalvergleich abgeschlossen: 408.170 Verfuegbarkeitsfaelle und
85 Waffen-ID-Pruefungen ergaenzen die bisherigen Tests. Release-Build aller drei
Module und 492.685 gemeinsame Vergleiche plus 54 Parserfaelle bestanden.
Beide Windows-Einstiegspunkte sind abgehakt; Integration weiterhin offen.
Der Linux-Decompiler hat bei BG_WeaponInWolfMP eine unaufgeloeste Sprungtabelle:
dessen eigener Funktionseintrag bleibt deshalb offen.

Historischer Arbeitsschritt abgeschlossen; aktueller Stand steht oben.

## Folgende Warteschlange

1. CG_LoadGearDef/Server-Gear-Lader und deren Abhaengigkeiten.
2. Gemeinsame ABI-Strukturen und Waffen-/Spielerzustand vor Integration abgleichen.

Wichtig: Erst fertige, belegte Funktionen abhaken. Der ET-SDK-Spielablauf ist
weiterhin nicht vollstaendig auf TC:E umgestellt.
