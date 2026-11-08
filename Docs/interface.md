# Menus, HUD and backgrounds

## Appearance

Open **Options -> Appearance** for palette, transparency and custom RGB values.
Palettes include Gold, Arctic, Crimson, Emerald, Black/Grey, Grey/Black, Slate,
Copper, Violet and Custom. Transparency affects menu surfaces rather than text
or the gameplay HUD.

Menu size changes supported menu surfaces. The startup main menu retains its
full-size layout; background artwork and the connection screen stay full-frame.
Very small user scales reduce physical text size; increase scale on low-resolution
displays.

## Independent widget sizes

**View -> Widget Size** provides independent Compass, Fireteam and Commandmap
controls. The [Cvar reference](cvars.md) lists defaults and ranges.

Voice menus keep the compact original left-hand location regardless of menu
size. Kill notifications retain their bottom-left anchor as the HUD scales.
Text-entry focus is an underline rather than a filled highlight over typed text.

## Match information

- Fireteam overlay: members, health, role/weapon and location.
- Command map and compass: oriented player markers and tactical information.
- Scoreboard and weapon statistics: TCE-specific player data.
- Ingame menu: persistent Scoreboard and Minimize actions.
- Windows: minimize title-bar button for a supported windowed ET host.

## Background selection

**Appearance -> Backgrounds** provides:

1. **Season**: local calendar/time selection, with a country rule or explicit
   winter months. Germany uses December, January and February.
2. **Live weather (launcher)**: information supplied by the optional launcher
   for an explicitly supplied city/postcode.
3. **Time of day only**: daylight, dusk or night without seasonal snow.

Seasonal artwork is not an observation of current snow. The weather launcher
uses Open-Meteo/GeoNames data; without fresh data it falls back to time of day.
Selection stays through UI reloads/map changes and is re-evaluated at process start.

From a source checkout:

```sh
python tools/launcher/start_game.py --game /games/et/et.x86 --location Berlin --country DE -- +set fs_game tce2
```

Use `--print-only` to inspect the command and weather result. The
[launcher reference](https://github.com/jay1110/true-combat/blob/final/tools/launcher/README.md)
describes caching and location-data handling.
