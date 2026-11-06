# TCE2 Modern UI authoring sources

TCE2 1.0.3 ships the modern UI inside the single `tce2-1.0.3.pk3`. There is no
separate theme PK3 to install. New and changed runtime resources are visible and
editable under the repository-root `pak` directory; unchanged resources come
from an existing original TCE installation.

The original UI remains byte-preserved in `pak_original_tce/ui`, `ui_obj`,
`ui_robj` and `ui_tdm`. This reference is local only and is never published or
included in source ZIPs. This `ui-modern` directory contains the presentation
sources and deterministic artwork/font inputs used to prepare that active tree.
`src/assets` contains the earlier production overrides, including all twelve
standard-map compass images. Release builds consume `pak`, not a hand-picked
subset of those authoring directories.

## One package and separate server files

Use the [manual workflow](../tools/release/README.md) or the documented local
bundle command. The public ZIP contains one mod PK3, UI/CGame binaries inside
that PK3, and the loose QGame modules plus matching bot/Lua runtime under `server`.
Editor resources come only from `navstudio/editor-pak` and the explicitly enabled
Editor build. Do not mix those client/server modules with a public Game build.

For original ET, use the ZIP installer with `--classic-client` to extract the
selected native UI/CGame pair from this same PK3 when updating an existing local
installation. Its initial menu can otherwise load an older loose UI DLL before
connecting to a pure server. Close the game first; the installer preserves
replaced files outside the mod search path. Retire older theme, server-list-fix
and client-override PK3s so that they cannot override the unified package.

## Editing and deliberate regeneration

Edit `pak` for the next production package. A normal build never regenerates or
overwrites those edits. If changing the UI authoring generators instead, run the
explicit migration tool described in [tools/assets/README.md](../tools/assets/README.md).
`prepare_pak.py --check` only compares the authoring derivation; `--replace`
deliberately replaces the active tree and its separate Editor overlay.

The source comparison still protects original actions, cvars, bindings, lists,
feeders, ownerdraws and conditions. Authorized new appearance/background settings
have narrow explicit contracts. Ingame Main, Options and Referee navigation stay
in their header; the scene remains visible underneath. Appearance provides
palettes, custom colors, opacity, menu/HUD size and background settings.

The 852x480 design canvas is fitted to the actual screen. Modern text prefers
about eight pixels of cap height before fitting; finite field, row and paragraph
bounds take precedence. UI and HUD scales are separate; connecting and the menu
background remain fullscreen. The scoreboard keeps player paging and GT5 stat
visibility. Very long feeder values retain their existing character limits.

## Fonts and backgrounds

Barlow TTF sources support FreeType-enabled ETLegacy; six committed 20,548-byte
DAT files and bottom-up atlases also support original ET 2.60b. The classic bake
uses 72 DPI, grayscale hinting and light outline emboldening for small resolutions.
Theme TGAs have power-of-two dimensions and native bottom-up rows. No engine
extension is required. Font provenance and SIL licenses are included with the
assets; authoring-only FreeType versions are pinned in
`tools/ui_modernize/requirements-fonts.txt`.

`map_pics` contains the sixteen supplied background PNGs. The portable generated
images, shaders and catalog are included in `pak`. Runtime selection changes the
picture for each normal game start while keeping it stable through menu reloads
and map changes. See [the background settings and optional weather helper](../tools/launcher/README.md).

## Verification

```sh
python -m unittest discover -s tools/ui_modernize -p "test_*.py"
python -m unittest discover -s tools/assets -p "test_*.py"
python tools/ui_modernize/generate_assets.py --check
python tools/ui_modernize/background_assets.py --check
```

Native text, viewport and background regression tools are under `tools/modernize`.
Runtime evidence distinguishes original ET 2.60b from ETLegacy. A screenshot matrix
is not a claim that every menu was physically mouse-clicked or that all engines,
platforms, languages and renderers have been tested.
