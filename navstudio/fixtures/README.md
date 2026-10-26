# obj_navstudio_doors

Ein kleiner isolierter Aktivierungs-Testlevel. Keine Missionskarte, keine
Produktions-Moddaten und kein Bestandteil der normalen TCE2-PK3.

Aktueller positiver Kandidat, ohne Deployment:
`python navstudio/build_doors_fixture.py --output build/navstudio-doorfixture/candidates/obj_navstudio_doors.positive.pk3`.
Der Builder öffnet kein Spiel. Der Standard-Ausgabepfad ohne `--output` ist
die Mapbibliothek; ihn während eines laufenden Fixtureprojekts nicht verwenden.
Vorhandenes Q3Map2 2.5.17n liegt hier als
`C:/q3map_2.5.16_win32_x86/q3map3.exe`; es wird nichts installiert.
Ausgabe: `etmain/all_maps/obj_navstudio_doors.pk3`. Compiler-Zwischendaten,
Logs und genaue Entity-/Modelldaten liegen unter `build/navstudio-doorfixture`.
Ein fremdes vorhandenes PK3 wird nicht überschrieben. Ein verifizierter eigener
Vorstand wird vor einer Änderung im Evidence-Unterordner gesichert.

Der Hauptflur läuft X0–1536, Y−128–128; alle begehbaren Böden liegen auf Z0,
Decke Z192. Player-origin Z30–32 ist für das TCE2-Körperprofil geeignet.
Rot = linker Startbereich, Grün = mittlerer Bereich/Schalter, Blau = rechter
Zielbereich. Acht Deathmatch- und beide Team-Spawns liegen ausschließlich
links, X96/160/224, Y−64/0/64, Z32. GT7-Script aktiviert BSP-Deathmatchspawns.

| Fall | Ort / sichere Boden-Anfahrt | Verhalten |
| --- | --- | --- |
| Direkte Schiebetür | Mittelpunkt512,0,64; Eintritte480,0,30 und544,0,30 | Targetname `navstudio_sliding`, key0, Start geschlossen. |
| Direkte Drehtür | Scharnier768,−64,64; Eintritte736,0,30 und800,0,30 | `navstudio_rotating`, key0, angles0 0 0, degrees90. |
| Schalterpflichtige Drehtür | Scharnier1184,−64,64; Eintritte1152,0,30 und1216,0,30 | `navstudio_remote`, key0/bit1; direktes Aktivieren gesperrt, Relay erlaubt. |
| Seitlicher Schalter | Mittelpunkt1034,264,56; Anfahrt1000,264,30 | Explizites **USE**, spawnflags0 ohne TOUCH. `navstudio_button` → `navstudio_relay` → `navstudio_remote`; genau eine Kette, keine Teambegrenzung. |
| Naher Schalter links | Mittelpunkt1120,122,56; Anfahrt1120,80,30 | Explizites USE → dieselbe Relaybindung; kurze sichere Rückkehr zur linken Türseite. |
| Naher Schalter rechts | Mittelpunkt1248,122,56; Anfahrt1248,80,30 | Explizites USE → dieselbe Relaybindung; kurze sichere Rückkehr zur rechten Türseite. |
| Verschlossene Sackgasse | Mittelpunkt1408,−136,64; Vorderseite1408,−104,30 | `navstudio_locked`, key−1; kein Schalter, kein Spawn/Ziel dahinter. |

Alle Türen schließen nach vier Sekunden. Der Schalter steht außerhalb des
geraden Flurs. Ein langer gewöhnlicher U-Weg verläuft über928,264 →944,416
→1408,416 →1424,224 und verbindet die beiden Seiten der Remote-Tür, sodass
beide Eintritte den Schalter ohne Benutzung ihrer eigenen bedingten Kante
erreichen können. Die Tür bleibt die kurze Route. Zielmarkierung:
`navstudio_goal_right` bei1488,0,32.

Die Drehtüren benutzen **degrees90** zum Öffnen und **angles0 0 0** als
Startstellung. `angle90` würde die Startstellung drehen. Scharniere entstehen
aus echten Origin-Brushes. Das Script setzt ausschließlich Respawnzeit,
Zeitlimit und Deathmatch-Spawnmodus; es öffnet oder entsperrt keine Tür.

Der entfernte Schalter und der Viersekunden-Timer bleiben als historischer
Negativfall erhalten: eine späte Rückkehr muss sicher den Umweg nutzen.
Zwei hinten angehängte nahe Schalter ermöglichen positive USE/Warten/SAFEEXIT-
Prüfungen beider Richtungen, ohne Öffnungszeit oder Geschwindigkeit zu erhöhen.
Die bisherigen Tür-, Schalter- und Relaymodelle/Entityreihenfolge sind unverändert.
`door-baseline-a13.json` hält dafür die ursprünglichen kompilierten Attribute fest.
Der alte installierte/ausgeliehene a13-PK3 wird durch Kandidatenbau nicht ersetzt.

BSP/VIS/LIGHT-Prüfung und Dateihashes bestätigen den gebauten Level. Aktivieren,
Schalterbedienung, beidseitige Botpassagen und Wiederholung nach Autoclose
müssen danach in der bestehenden sichtbaren TCE2-Instanz geprüft werden.
