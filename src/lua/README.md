# Server-side Lua for TC:E

Build with `-DFEATURE_LUA=ON` (default). Lua 5.4.9 is linked statically into
qagame; no external Lua DLL is needed. No scripts run by default.
Copy `example.lua` to `tce2/lua/example.lua`, then configure before map start:

```
set lua_modules "lua/example.lua"
```

Separate module paths with spaces or semicolons, maximum16 modules; engine VFS
loads each text script (maximum1MiB). Each module has a separate Lua state.
Change `lua_modules` and restart the map to reload. `lua_status` lists loaded
modules and their active/disabled state. This is an initial API subset using
ET mod callback conventions, not full ETPub/ETLegacy script compatibility.
Lua5.1-specific scripts may need syntax/library migration.

## Callbacks

- `et_InitGame(levelTime, randomSeed, restart)` after native game initialization.
- `et_ShutdownGame(restart)` before native shutdown.
- `et_RunFrame(levelTime)` after native frame processing.
- `et_ClientConnect(clientNum, firstTime, isBot)`: return a string to reject;
  return nil to allow. Flags are integer0/1, as in ET conventions.
- `et_ClientBegin(clientNum)` after player initialization.
- `et_ClientUserinfoChanged(clientNum)` after userinfo processing.
- `et_ClientDisconnect(clientNum)` before native disconnect processing.
- `et_ClientCommand(clientNum, command)` and `et_ConsoleCommand(command)`:
  return integer1 to consume, otherwise native dispatch continues.

Modules run in configured order. Rejection/consumption stops later modules.
Actual client functions are hooked, including native Omni-bot client calls.
Lua initializes before Omni-bot initialization. Native internal reconnect calls
are processed according to their existing firstTime/restart arguments.

## `et` API

`RegisterModname(name)`, `FindSelf()`, `G_Print(text)`, `trap_Cvar_Get(name)`,
`trap_Cvar_Set(name,value)`, `trap_Argc()`, `trap_Argv(index)`,
`trap_GetUserinfo(clientNum)`, `Info_ValueForKey(info,key)`,
`trap_SendServerCommand(clientNum,text)` (-1 broadcasts),
`trap_SendConsoleCommand(mode,text)`, `trap_Milliseconds()`.
Console commands always use EXEC_APPEND to prevent reentrant VM destruction.
Use newline-terminated commands. Global `print` accepts a single string.

### Gameplay callbacks (API version 2)

- `et_ClientSpawn(clientNum, revived, hostage)`: notification after native spawn.
- `et_Damage(target, attacker, damage, dflags, mod)`: return integer 1 to cancel
  damage before body weighting and health effects. Arguments contain raw damage.
  Native immunity checks and earlier special mover/mine paths remain authoritative.
- `et_Obituary(victim, killer, mod)`: death notification; return value is ignored.
- `et_WeaponFire(clientNum, weapon)`: return integer 1 to cancel authoritative
  weapon effects. Shared movement has already consumed ammunition and predicted
  animation; this hook does not refund ammo or undo the client animation.
- `et_Chat(sender, receiver, text)`: called separately for each recipient allowed
  by native chat filters. Return nil to keep text, false to suppress, or a string
  to replace. Replacements chain through modules, are length limited, and replace
  control characters, quotes and backslashes for safe native chat transport.

Nested native operations skip callbacks into the currently executing Lua module
so its stack remains intact; other idle modules can receive those callbacks.

### Entity and server access

`gentity_get(entityNum, field[, index])` requires an existing entity. Fields:
`health`, `classname`, `targetname`, `r.currentOrigin`, `s.origin`, `s.number`.
Player fields: `pers.netname`, `pers.connected`, `sess.sessionTeam`,
`sess.playerType`, `ps.weapon`, `ps.origin`, `ps.viewangles`, `ps.ammo`,
`ps.ammoclip`, `ps.stats`. The last three require a native zero-based index.
Vectors are Lua tables with components at indices 1, 2, 3.

`gentity_set(entityNum, field, value)` (or zero index followed by value) supports:
- `health`: 1..9999; player health and STAT_HEALTH synchronized. Cannot resurrect.
- `ps.origin` / `r.currentOrigin`: living connected player, native teleport.
- `ps.viewangles`: connected player, native view-angle update.
- `ps.ammo` / `ps.ammoclip`: use `(entityNum, field, index, value)`, value 0..999.

Other fields are read-only. Ammo writes do not grant a weapon. Class selection,
weapon grants and revival require additional dedicated lifecycle APIs.

`trap_GetConfigstring(index)`, `trap_SetConfigstring(index,text)`,
`trap_SetUserinfo(clientNum,info)`, `Info_SetValueForKey(info,key,value)` and
`G_SetTeam(clientNum,team)` are available. Userinfo changes run native validation
and notification. Team changes use native SetTeam without forcing restrictions;
return is boolean. Team strings: axis, allies, spectator (red/blue/s aliases).
Configstrings and userinfo remain engine-owned data: use valid native layouts.

Constants: TEAM_AXIS, TEAM_ALLIES, TEAM_SPECTATOR, EXEC_APPEND,
CON_CONNECTED, CON_CONNECTING, CON_DISCONNECTED, STAT_HEALTH, MAX_WEAPONS,
MAX_CLIENTS, MAX_GENTITIES; `API_VERSION="tce2-lua-2"`.
Native team enum names remain for compatibility with script conventions.

## Errors and scope

Script errors disable only that module and print file/event/error details.
A callback instruction hook limits ordinary accidental infinite loops to about
1million Lua instructions. Base/table/string/math/utf8 libraries are available;
package/io/os/debug, dofile and loadfile are unavailable. These are trusted
server-admin plugins, NOT a hardened sandbox for hostile code: memory, expensive
C-library calls and caught hook errors are not a complete resource boundary.

No database libraries, cross-module IPC,
client-side Lua or full gentity setter API yet. Omni-bot still uses GameMonkey;
Lua is an independent game-server extension. Runtime assets remain separate from
Git. Linux source integration exists but this addition was built/tested on Windows.
