# Lokaler Testserver

Die Kopien von `serverconfig.cfg` und `objmaps.cfg` entsprechen den reparierten
lokalen Dateien in `tce2/`. Die BAT hier startet aus dem Repository-Stamm;
die lokale Root-`tceded.bat` verwendet stattdessen ihr eigenes Verzeichnis.
Spielassets und Engine werden weiterhin lokal benoetigt.

Port: 27960. Objective-Konfiguration: g_gametype 5. Das SDK enthaelt noch nicht
die vollstaendige TC:E-Rundenlogik. Ein erfolgreicher Serverstart belegt keine
fertige Client-Verbindung oder Spielbarkeit.

`sv_punkbuster` ist schreibgeschuetzt; die alte Konfigurationszeile hatte keine
Wirkung. Der effektive deaktivierte Zustand bleibt erhalten. Auch der bisherige
Portwechsel auf 8999 in der Konfiguration wurde erst nach Netzwerkstart versucht
und war nicht wirksam. Den gewuenschten Port im Launcher einstellen.

Die Engine schreibt durch `+set logfile 2` eine `etconsole.log` in ihren
aktiven `fs_homepath/tce2`-Ordner. Der isolierte Test schreibt seine Logs getrennt:

```powershell
python src/tools/test_server_start.py
```

Der Test uebernimmt die Root-Launcher-Argumente, nutzt Loopback/Port 27969 und
`build/server-smoke/obj_delta` als eigenes Home-Verzeichnis. Er kontrolliert
UDP-getstatus (einschliesslich gamename=tce2) und Startdiagnosen und beendet
seinen eigenen Serverprozess. Mit `--client` prueft er zusaetzlich den lokalen
Verbindungsaufbau und unveraenderte Client-DLL-Hashes. `build.ps1` erzeugt dazu
auch `tce2/zz_tce2_vm.pk3`; das Paket muss zum jeweiligen Build passen.
Er ist absichtlich kein automatischer Bestandteil jedes DLL-Builds.
