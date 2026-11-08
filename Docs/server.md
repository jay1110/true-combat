# Server administration

## Native modules and architecture

Install the package's `server/common` data and the QGame/bot core for the server
process. Linux x86_64 requires the x86_64 QGame **and** x86_64 bot library;
a 64-bit OS running a 32-bit engine instead needs the i386 libraries.

Start with a private configuration containing your hostname, network/download
policy and administrative passwords. The examples under
[`src/server`](https://github.com/jay1110/true-combat/tree/final/src/server)
provide an Objective rotation for the six standard TCE maps. Set passwords in
your deployed configuration, not in a published source file.

## Startup

An example using an installed dedicated engine:

```text
etded +set fs_game tce2 +set dedicated 2 +set net_port 27960 +set g_gametype 5 +set sv_official 0 +exec serverconfig.cfg
```

Choose network port and gametype before startup. The example map cycle itself
loads its first map. `sv_official 0` allows the community setup rather than the
original official-map restrictions. Use `devmap` for a local development game;
ordinary public server rotation uses `map`.

For a client-side listen server:

```text
ET.exe +set fs_game tce2 +set g_gametype 5 +set sv_official 0 +devmap obj_delta
```

GT2 = Reinforced Objective, GT5 = Objective, GT7 = Bodycount. Change the startup
configuration and restart when switching modes. Maps must support the selected mode.

## Bots and scripts

Enable bots before map initialization:

```text
set omnibot_enable 1
set omnibot_path ""
```

The packaged `omnibot-release.cfg` sets the intended enable/path and mapscript
directory settings. See [bots](bots.md) for commands and navigation requirements.
[Lua/WolfAdmin](scripting.md) is optional and is activated through `lua_modules`.

## Platform advertisement and country flags

`g_oss` is server information used by compatible browsers, not an installer or
platform detector. Advertise the client libraries actually supplied. Its full
matrix default is `2047`; Windows x86/x64 and Linux i386/x86_64 alone use `263`.
The [Cvar reference](cvars.md) and
[platform flag table](server-platforms.md) explain the bit values.

Country flags require server `g_countryflags`, client `cg_countryflags` and an
appropriate legacy IPv4 country database. No database means unknown countries,
not a failure to load the mod. Keep this server data outside the client PK3.

## GT7 custom spawns

With `g_mapscriptdirectory "mapscripts"`, a matching custom mapscript can override
the map's packaged script. TCE2's mode-specific names include
`obj_delta_gt7.script`. Preserve the map's existing script logic.

The mapscript actions are **script commands, not console Cvars**:

```text
tce_dmspawn 128 256 64 90
tce_dmspawns 1
```

`tce_dmspawn` supplies x/y/z/yaw. `tce_dmspawns 1` enables map-local GT7 deathmatch
spawn selection; `0` keeps the normal selection. Use validated player-accessible
positions. The selector favors distance to the nearest living opponent and
retains safe fallbacks.

## Configuration reference

Use the [Cvar reference](cvars.md) for defaults, persistence, latch/read-only
flags, mode restrictions and introduced/changed names. Do not assume that an ET
engine's renderer/network Cvar is a new TCE2 setting simply because a menu uses it.

```{toctree}
:maxdepth: 1

server-platforms
```
