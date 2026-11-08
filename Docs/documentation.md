# Building and publishing the documentation

## Local build

The manual uses Sphinx, MyST Markdown and the Read the Docs theme. Dependencies
are pinned in `Docs/requirements.txt`. No engine, game asset installation or C/C++
build is required for the manual.

```sh
python -m venv build/docs-venv
# Activate the environment, or use its Python executable directly.
python -m pip install -r Docs/requirements.txt
python -m sphinx -W --keep-going -b html Docs build/docs/html
python -m sphinx -W --keep-going -b epub -d build/docs/doctrees-epub Docs build/docs/epub
```

Open `build/docs/html/index.html`. Warnings are treated as failures. The version
is read from `src/game/tce_version.h`, and the release chapter includes the root
`CHANGELOG.md` rather than a second independently edited history.

## Read the Docs

1. Import `jay1110/true-combat` into your Read the Docs account.
2. Select `final` as the documentation branch/version to build.
3. Use the repository's `.readthedocs.yaml`; it configures Python, requirements,
   Sphinx, warning failures, HTML download and EPUB output.
4. After the first successful hosted build, use the address assigned to that
   project for the public documentation link.

Creating/importing the hosted project is an account operation; committing a
configuration file does not itself create a site. The documentation build does
not dispatch the mod's GitHub release workflow.

The official [Read the Docs Sphinx guide](https://docs.readthedocs.com/platform/stable/intro/sphinx.html)
and [configuration reference](https://docs.readthedocs.com/platform/stable/config-file/v2.html)
describe the hosting configuration. Markdown support follows
[MyST's configuration reference](https://myst-parser.readthedocs.io/en/latest/configuration.html).

## Maintaining the manual

- Write public documentation in English.
- Use Added, Removed, Fixed and Modified categories in the changelog.
- Record actual Cvar names/default/flag changes; distinguish console commands,
  mapscript actions and engine-owned settings from mod Cvars.
- Keep reproducible source inventories current when registrations or packaged
  server resources change.
- Preserve component credits/licenses and link to identifiable source origins.
- Keep passwords, player databases and personal filesystem paths out of examples.

Game-facing behavior belongs in player/server chapters. Build diagnostics and
internal audit notes must not be presented as new gameplay features.
