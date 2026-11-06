# Editable mod delta resources

`pak/` is the canonical editable tree of new or changed mod resources. It contains
no byte-identical stock TCE assets; the original installed game supplies those.
The release packager reads it directly and does not regenerate menus or overwrite
hand edits. Native UI/CGame
modules are added from the selected build manifests during final packaging.
QGame remains outside the PK3 in the release ZIP's `server/` directory.

`pak_original_tce/` is a local, read-only reference and is never published or
bundled. It is not pure stock: it also contains project changes and user compass
images. Equality with this local folder alone is not a reason to omit a file.
`base-game-reference.json` records hashes/sizes from the real TCE 0.49b pak0..4,
with engine-compatible last-package precedence. Only equality with this stock
index removes an asset from the delta. All twelve project compass images remain.
`base-ui-reference.json` records only stock menu hashes, names and open edges;
CI can validate Editor closure without original menu bytes or a local original
installation. Both manifests are version-bound metadata, not original payloads.
`map_pics/` holds the supplied background PNG source images.

To deliberately prepare a fresh delta from local references and source overlays:

```powershell
python tools/assets/prepare_pak.py
```

Preparation combines the local reference, all `src/assets/` and the validated
`collect('game')` result, then writes only the non-stock result. This retains the
project compass maps, modern fonts, widget aliases and the Game/editor boundary.
Nested archives, native binaries, the fingerprint-identified obsolete
ServerListFix readme and the original standalone
`unbindall` reset are excluded. Known Finder/Explorer metadata (`.DS_Store`,
`Thumbs.db`, and files under `__MACOSX`) is recorded as excluded; arbitrary hidden
resources are not silently discarded. Original input bytes are never changed and copies
are independent files, so later edits cannot modify the originals through links.

The declared native voice/goal catalog in `navstudio/editor-ui/editor-files.txt`
is merged with modern Editor differences. The native root receives the distinct
name `tce_omnibot_dms` and a single entry in the compact modern root; native
backlinks follow that name without rewriting command strings. Registry loaders
are merged without duplicating main menus. All native pages and goal editors
must be loaded and reachable. Only non-stock differences from the effective Game tree are written to
`navstudio/editor-pak/`, in the same Editor PK3 rather than another addon PK3.
This explicit overlay belongs solely to the separate developer build.
Provenance, stock identity, exclusions and SHA-256 fingerprints are
written to `tools/assets/pak-sources.json`, outside installable resources.

```powershell
python tools/assets/prepare_pak.py --check
python -m unittest discover -s tools/assets -p "test_*.py"
```

`--check` reports missing, extra or changed paths and does not write anything.
After manually editing `pak/`, differences from the generators are expected;
release packaging still uses the edited bytes. A normal preparation refuses to
replace changed files. Use `--replace` only when deliberately discarding those
edits and recreating the delta and Editor overlay from their source inputs.
