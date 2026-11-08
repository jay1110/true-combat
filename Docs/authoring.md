# Navigation and goal authoring

## Separate Editor edition

Use the Editor edition and its matching modules/core for local map development.
The normal Game edition contains runtime navigation and missions, but excludes
waypoint/goal editing, editor menus and debug drawing.

The desktop NavStudio provides a map/project workflow. Select TCE2 or normal ET,
choose a map and supported gametype, and import existing navigation or generate
a new draft. TCE2 and ordinary ET have different game modules and mode meanings.

## Project workflow

1. Create/open a named map project.
2. Select the map and mode. NavStudio checks mode-specific mapscripts before
   proposing the corresponding goals.
3. Import or generate navigation; inspect the map, reachable areas and diagnostics.
4. Edit waypoints, connections, flags, goals and additional spawns as needed.
5. Validate the **current draft**, then install it into the selected test game.
6. Observe bots and mission completion. Export the validated navigation and
   companions when the project passes the required checks.

Generation is a starting point backed by movement checks, not a proof that every
map or mission is correct. Rejected geometry probes are not automatically broken
waypoints. Inspect actual reachable routes and objective behavior in the game.

## Mouse editing and camera

- Drag a selected waypoint onto valid ground to move it.
- Add a point by clicking ground; connect source and destination explicitly.
- Choose bidirectional connections or a single direction as appropriate.
- Delete selections and use undo/redo for reversible editing.
- Hold the right mouse button to look around; use WASD and Q/E to move.
- Middle-drag pans, the wheel moves, and F focuses the selection.

Do not infer traversability from a debug line drawn through walls. Radius is
arrival tolerance, not an instruction to ignore collision or drop safety.

## Map data

The runtime uses `.way`, `.gm` and `_goals.gm` companions. TCE2 can select
mode-specific `<map>_gtN` pairs. GT7 custom spawn actions belong in its mapscript;
keep original map logic and encoding when applying a spawn block.

The [complete desktop guide](https://github.com/jay1110/true-combat/blob/final/navstudio/README.md#english)
covers projects, settings, installation paths, validation, recovery, flags,
goals, mapscripts and packaging. Editor tools are optional development inputs;
players do not need them to join a normal server.

```{toctree}
:maxdepth: 1

game-editor-boundary
```
