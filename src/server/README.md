# Dedicated server examples

Copy `serverconfig.cfg` and `objmaps.cfg` into the installed mod directory and
customize them before running a server. The default rotation uses Objective
(GT5) and the six standard TCE maps. The engine and original TC:E assets are
installed separately.

On Windows, `tceded.bat <game-directory> [engine-name]` launches an existing
installation; the default engine name is `etded.exe`. Configure the process
architecture, native modules, ports and private passwords for your server.

The [server manual](../../Docs/server.md) covers installation, bots, Lua,
custom spawns and troubleshooting. Set the gametype and network port before
startup. No example enables administration with a published password.
