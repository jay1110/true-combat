# Lua and WolfAdmin

## Server Lua

Lua is server-side and optional. It is independent of Omni-bot's GameMonkey
mission scripts. No Lua script runs merely because the runtime was compiled in.

Place your script under the installed mod directory and configure, for example:

```text
set lua_modules "lua/example.lua"
```

The runtime loads trusted administrator scripts through the engine filesystem.
Use `lua_status` to inspect loaded modules. From the server console or RCON,
`lua_restart` reloads configured scripts without reconnecting players; script
states and authentication sessions are recreated.

Callbacks cover initialization/shutdown/frames, player lifecycle, commands,
chat, damage and weapon fire. Entity access is bounded to documented fields.
It is an ET-style API subset, not complete compatibility with every ET mod's
Lua environment. Scripts using Lua 5.1-specific behavior may need migration.

## WolfAdmin

The optional WolfAdmin adaptation provides ACLs, player administration,
kick/ban/mute, team management, private messages, map controls and bot controls.
Enable its module explicitly:

```text
set lua_modules "lua/wolfadmin/main.lua"
```

Review the supplied default configuration, set private credentials as needed,
and run `lua_restart` or restart the map. Preserve the writable administration
database during upgrades. Server console and authenticated RCON provide
administrative access; players are not automatically made administrators.

`wolfadmin_password` supports session authentication through `/wolfauth`.
Persistent GUID-based client administration is opt-in through
`g_wolfadminTrustGuid`. A userinfo GUID alone is not cryptographic identity.
Never put the authentication password in chat or published configuration.

```{toctree}
:maxdepth: 1

lua-api
wolfadmin
```
