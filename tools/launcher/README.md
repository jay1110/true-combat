# Optional weather-aware background launcher

The existing game executable remains the runtime. Python3 is needed only for
this optional launcher; the mod uses the original ET2.60b engine ABI.

Windows:

```powershell
python launcher/start_game.py --game "C:/Games/ET/et.exe" --location "Berlin" --country DE -- +set fs_game tce2
```

Linux:

```sh
python3 launcher/start_game.py --game /games/et/et.x86 --location Berlin --country DE -- +set fs_game tce2
```

`start_game.ps1` and `start_game.sh` forward the same arguments. `--print-only`
reports the result without opening the game. `--no-network` permits only a
matching fresh cache. The cache lives in the user's LocalAppData/cache directory,
outside the game and source folders. No location is inferred from an IP address.

The default selection needs no launcher or network: Appearance > Backgrounds
offers Season, Live weather and Time of day only. Season defaults to DE and
December/January/February. Southern winter countries use June/July/August.
Country defaults are artwork rules, not a snow report: set the month field to
none for a region without snowy winter, or enter your own comma list such as
12,1,2. Unknown/tropical countries do not receive automatic winter pictures.
Settings take effect on the next game process start.

For the optional Live weather mode, the explicit city or postal code is sent over HTTPS to Open-Meteo's geocoding
service. Current weather, nearby hourly snow depth, snowfall during the next
three hours and sunrise/sunset determine the picture group. Cold temperature
alone never selects a winter picture.
Results are reused for30minutes and accepted for at most3hours during an outage.
Missing/stale data in Live weather mode select a picture by the computer's local
clock and do not claim snowfall. A direct normal game launch uses Season by
default, independently of weather availability.
The game keeps its picture throughout UI reloads/map changes and chooses another
appropriate picture on the next process start.

Weather and geocoding: [Open-Meteo documentation](https://open-meteo.com/en/docs),
[geocoding documentation](https://open-meteo.com/en/docs/geocoding-api).
Weather data attribution: Open-Meteo; location data: GeoNames. Pictures are the
user-provided artwork retained as game-ready images in `pak/tce2_ui/backgrounds`.
The original PNG collection is not required. Time-of-day fallback windows are sunrise07:00,
sunset19:00; a fresh configured location supplies real solar times instead.
