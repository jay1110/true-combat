# Canonical mod resources

Edit `pak/` directly for public Game resources. Edit `navstudio/editor-pak/`
for the optional Editor overlay, including its complete Omni-bot DMS menus.
These are the installable sources. Release packaging and the UI collector read
their current bytes; they do not restyle, import, merge or replace them.
Native UI/CGame modules are added from selected build manifests during packaging.
QGame stays outside the PK3 in the release ZIP's `server/` directory.

The delta contains new or changed resources, including all twelve project
compass images. Installed original TCE 0.49b pak0..4 supplies unchanged resources.
`base-game-reference.json` records their hashes and package precedence;
byte-identical stock copies are rejected. `base-ui-reference.json` provides only
stock menu names and open edges for closure checks. Both are version-bound
metadata and contain no original asset bytes.

`pak_original_tce/` remains a local reference. Validation, CI and release
packaging do not read it or copy its contents. Historical `src/assets/`,
`ui-modern/` authoring copies and `navstudio/editor-ui/` catalog files are not
fallback sources for the canonical resources.

Validate Game and Editor without changing any resource or metadata:

```powershell
python tools/assets/prepare_pak.py
python tools/assets/prepare_pak.py --check
```

Both commands validate the current canonical trees: safe paths, public/editor
separation, no native binaries or nested packages, authenticated stock exclusion,
and complete loaded/reachable Editor goals and voice menus. The original
standalone `unbindall` reset is rejected rather than silently rewriting configs.
Manually edited resources are accepted when they satisfy these checks.

To refresh the separate inventory after intentional edits:

```powershell
python tools/assets/prepare_pak.py --write-provenance
```

This writes only `tools/assets/pak-sources.json`, with canonical paths, SHA-256
fingerprints, stock-metadata identities and Editor closure results. It leaves
every installable file and its modification time unchanged. The inventory is
provenance, not a generator recipe or a replacement source; validation does not
reject a valid hand edit just because an older inventory has a different hash.
The obsolete destructive `--replace` command fails with an explanation.

The old stock/theme import and automatic DMS merge have been removed. To add or
change an Editor menu, edit its final overlay file and update all applicable
registry loaders and navigation targets; the closure check verifies the result.

```powershell
python -m unittest discover -s tools/assets -p "test_*.py"
```

The frozen background TGA files, shader and catalog are validated independently
of the original PNGs with `python tools/ui_modernize/background_assets.py --check`.
To regenerate from externally retained PNGs, pass an explicit `--source` directory.
Output defaults to ignored `build/ui-backgrounds/`; `--out` selects another root.
No PNG source directory is required in a clean checkout or a normal release build.
