# Installation and upgrades

## Requirements

- A working True Combat: Elite 0.49b installation with its original asset packages.
- An external Enemy Territory 2.60b or compatible ETLegacy engine.
- Libraries matching the **engine process architecture**, not merely the OS.
  Classic ET 2.60b on Windows is a 32-bit process even on 64-bit Windows.

Use the Game edition for normal clients and public servers. The Editor edition
is for navigation/goal authoring; its modules and bot runtime must stay together.

## Package contents

| ZIP path | Install into the game's `tce2` directory |
| --- | --- |
| `tce2/tce2-1.0.4.pk3` | Mod PK3 with project resources and UI/CGame modules. |
| `server/common/*` | Shared bot scripts, map navigation and Lua support. Merge directories. |
| `server/<target>/*` | QGame and bot core for the engine architecture. Merge directories. |

Keep original TC:E packages. Replace the previous TCE2 update rather than
stacking versions: an older alphabetically later PK3 can override new menus or
client modules. Do not put QGame inside an additional client PK3.

## Classic ET client modules

Some hosts require the selected UI/CGame pair as loose native files. The
installer extracts the **same pair from the mod PK3**, checks its hashes and
installs the selected server runtime. From the **unpacked ZIP directory**, run
the included `install.py`:

```sh
python install.py --target windows-x86 --mod-dir C:/Games/ET/tce2 --classic-client
```

Use `python install.py --help` for its current options. The explicit
`--classic-client` flag extracts the native client pair. Check the target and
destination before installing; an existing installation is required.

## Start the mod

From a configured game directory:

```text
ET.exe +set fs_game tce2
```

For a local development map:

```text
ET.exe +set fs_game tce2 +set g_gametype 5 +set sv_official 0 +devmap obj_delta
```

Use the external engine executable appropriate to your platform. Choose TCE2's
gametype **before map startup**; mode and mapscript selection must agree.

## Upgrade safely

1. Stop the game/server before replacing loaded native libraries.
2. Keep your server configuration, profiles, scripts and administration database.
3. Remove the previous TCE2 update PK3 from the active search path.
4. Install the new PK3, selected server target and matching shared runtime.
5. Start the game and check the version and bot initialization in the console.

A core/build mismatch is resolved by installing the matching runtime. See
[server configuration](server.md) and [troubleshooting](troubleshooting.md).
