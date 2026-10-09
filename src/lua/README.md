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

`gentity_get(clientNum,field)` supports these read-only client fields:
`pers.netname`, `pers.connected`, `sess.sessionTeam`, `sess.playerType`, `health`.
Unsupported fields raise an error. No arbitrary entity memory access.
Constants: TEAM_AXIS, TEAM_ALLIES, TEAM_SPECTATOR, EXEC_APPEND,
CON_CONNECTED, CON_CONNECTING, CON_DISCONNECTED; `API_VERSION="tce2-lua-1"`.
Native team enum names remain for compatibility with script conventions.

## Errors and scope

Script errors disable only that module and print file/event/error details.
A callback instruction hook limits ordinary accidental infinite loops to about
1million Lua instructions. Base/table/string/math/utf8 libraries are available;
package/io/os/debug, dofile and loadfile are unavailable. These are trusted
server-admin plugins, NOT a hardened sandbox for hostile code: memory, expensive
C-library calls and caught hook errors are not a complete resource boundary.

No damage/weapon/spawn/chat mutation hooks, database libraries, cross-module IPC,
client-side Lua or full gentity setter API yet. Omni-bot still uses GameMonkey;
Lua is an independent game-server extension. Runtime assets remain separate from
Git. Linux source integration exists but this addition was built/tested on Windows.
