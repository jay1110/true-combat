# Game and NavStudio builds

Both variants include normal Omni-bot gameplay, navigation, mission scripts,
team balancing and bot administration. The Game variant is the public mod.
NavStudio is a separate authoring variant with its own matched bot core and
script payload. Do not mix their DLLs, cores or scripts.

The Game core excludes waypoint and goal authoring commands, editor/debug
rendering, operator script consoles, remote debugging, live script updates
and NavStudio diagnostics. Registered script commands and aliases are
classified in `tools/release/runtime-policy.json`; unknown additions fail
the package check. Mission callbacks and internal goal/navigation APIs remain
available. Compatibility drawing hooks in mission scripts have no effect in
Game builds.

## Validation for 1.0.2

Windows x86 and x64 Game and NavStudio packages were built and checked against
their file manifests. The checks establish that 86 native authoring handlers,
14 real drawing methods and 44 navigation diagnostic format strings are
excluded from Game and retained in NavStudio. The script policy classifies
138 registrations and applies 51 exact diagnostic transformations; the raw
Editor payload is preserved.

The final x86 Game package was loaded in the existing ET 2.60b installation
on `obj_delta`, GT2, with four fighting bots. Actual `/bot help` contained
only the reviewed public commands. Eighteen direct editing/display/debug
command probes returned `Unrecognized command`. The fresh session log
recorded 68 combat kills, zero matching NavStudio navigation diagnostic
lines and zero script errors or drawing-export warnings. A clean spectator
screenshot was inspected. This is a release-boundary smoke test, not a claim
of complete mission or platform runtime parity.

Reproduce the package and help checks with:

```text
python tools/release/validate.py archives <release-directory>
python tools/release/validate.py bot-help <captured-console-text> --variant game
python -m unittest discover -s tools/release -p "test_*.py"
```

All release workflows are manually dispatched. See `BUILDING.md` for target
toolchains, complete archive requirements and public versus authoring outputs.
