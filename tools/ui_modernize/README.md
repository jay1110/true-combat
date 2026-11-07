# TCE2 UI authoring and validation

`pak/` is the sole editable source of production menus, fonts, shaders and
artwork. The optional Editor overlays its final files from
`navstudio/editor-pak/`. Release packaging reads these bytes directly; no
original-menu import or theme regeneration runs during a build.

The modern UI ships inside the single `tce2-1.0.3.pk3`, together with UI/CGame
modules. QGame and the matching bot runtime stay loose under `server/`.
See [release packaging](../release/README.md) and
[canonical resource checks](../assets/README.md).

## Retained design sources

`source/` contains the industrial background PNG and its provenance, the
historical original-menu inventory, and the presentation coverage report.
These are tooling inputs and historical metadata, not another installable
resource tree. The local `pak_original_tce/` reference remains ignored and
unpublished; historical source-comparison tests can use it, but production
builds and canonical resource validation do not require it.

The retired root `ui-modern/` mirror, `map_pics/` PNG collection and `tce/`
legacy launcher are removed. All sixteen processed map backgrounds remain in
`pak/tce2_ui/backgrounds/`, with their source fingerprints, selection phases,
shaders and native format checks. To process a new external PNG collection,
supply `background_assets.py --source <directory>` explicitly. See
[background selection and weather settings](../launcher/README.md).

## Editing and generation

Edit production files in `pak/`, and Editor-specific files in its overlay.
`tools/assets/prepare_pak.py` now validates these resources without rewriting
any of them. Its optional `--write-provenance` updates only the inventory;
`--replace` is retired.

Artwork, font and legacy presentation generators write staging output under
ignored `build/ui-assets/`, `build/ui-fonts/` and `build/ui-style/` by default.
External background generation defaults to `build/ui-backgrounds/`. Review any
staged output before deliberately copying selected changes into the canonical
tree. Generator `--check` modes validate committed assets without generating
or copying another resource tree. Native font generation needs the pinned
optional [font requirements](requirements-fonts.txt).

Classic ET 2.60b uses the committed DAT/TGA fonts and bottom-up, power-of-two
textures; ETLegacy can also use the included Barlow TTFs. Their licenses and
provenance remain alongside the assets. No engine extension is required.

## Verification

```sh
python -m unittest discover -s tools/ui_modernize -p "test_*.py"
python -m unittest discover -s tools/assets -p "test_*.py"
python tools/ui_modernize/generate_assets.py --check
python tools/ui_modernize/background_assets.py --check
python tools/assets/prepare_pak.py --check
```

Native text, viewport and background regression tools live in `tools/modernize`.
Source and fixture checks do not establish live engine rendering or input
parity. Production public/editor separation and stock-resource exclusion are
checked independently by `tools/release` and `tools/assets`.
