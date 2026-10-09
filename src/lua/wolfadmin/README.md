# WolfAdmin 1.2.1 for TC:E

This is the original WolfAdmin 1.2.1 Lua implementation by Timo “Timothy” Smit,
adapted for TC:E's server Lua 5.4 runtime. Upstream copyright notices, GPL license,
SQLite schema, configuration examples and command modules are retained. No client
PK3, menus, sounds or external Lua DLL are required.

Copy this directory to `tce2/lua/wolfadmin`, then set:

```
set lua_modules "lua/wolfadmin/main.lua"
```

Restart the map. The first start creates `wolfadmin.db` in the writable game
directory using the bundled original schema. Existing databases are preserved.
The server reads `wolfadmin.toml` from the game directory when present; otherwise
it uses `lua/wolfadmin/config/wolfadmin.toml`. Copy that file to the game directory
to customize it. Admin actions are logged to `admin.log`.

Use the server console or authenticated RCON for administration. For example:

```
!help
!listplayers
!put 0 s
!mute 0 60 reason
!unmute 0
!setlevel 0 3
acl listlevels
```

Commands use the original WolfAdmin syntax and permission names. The console is
the original level-5 pseudo-player. No player is automatically made admin.
`!setlevel` requires a connected human with a unique, nonzero, 32-digit hexadecimal
GUID. Missing/unknown GUIDs and bots remain guests. Their database keys exist only
for session bookkeeping and never grant permission. Duplicate GUIDs and changes
of GUID during a connection cannot authorize commands. Names, slots and IP
addresses are never persistent authentication credentials.

For in-game administration without trusting GUIDs, configure a private server
password with `set wolfadmin_password "your-secret"` (never `sets`/`setu`). Leave
it empty to disable password login. The player then enters `/wolfauth your-secret`
in their game console, **not chat**. The command is consumed before WolfAdmin
logging, and attempts are limited to one per five seconds per connection. A
successful login grants level-5 rights only for this session; disconnecting,
restarting the map or changing the server password invalidates it. This does not
grant a persistent level to a missing/unknown GUID. The game connection itself
does not encrypt this credential; use a unique server password.

**Persistent GUID-based client administration is disabled by default.** TC:E exposes `cl_guid` from
userinfo; this is not proof of identity. Storing a level with console `!setlevel`
does not enable client administration. Only an operator who has independently
secured GUID authentication should enable `set g_wolfadminTrustGuid "1"`.
This opt-in trusts that external assurance; it does not add authentication or make
spoofable GUIDs safe. Without it or a password-authenticated session, clients can only use help, admintest, time and
greeting; privileged commands remain available through server console/RCON.

Enabled administration includes original ACL management, player/alias/history
lists, kick/ban/unban, warnings, text/voice mutes, rename, team changes/locks,
balancing, bot controls, slap/gib, restart/reset/nextmap, shuffle/swap and pause.
Persistent bans reject guests, bots and duplicate GUIDs explicitly; use `!kick`
for them. Duration zero (or omitted with the original permissions) is a permanent
ban, displayed as permanent and retained until `!unban` removes its ban ID. Ban enforcement for real GUIDs has the same identity trust limitation.

ET-specific news audio, spree messages/sounds/records, ET weapon statistics,
campaign/map-voting lists and next-map vote override are disabled and omitted
from command help. ET fireteam metadata is omitted from player lists. Bundled
ET client menus and sound assets are intentionally not deployed. Native TC:E
team restrictions and gameplay lifecycle remain authoritative.

The adaptation uses VFS module loading/listing and file I/O, bounded native
SQLite access, and time/date functions. It never launches shell commands.
Upstream messages are sent directly using sanitized client protocol strings;
other queued engine operations use a fixed allowlist.

Private messages are implemented server-side: `/pm <name|slot> <message>`
(or `/m`) selects one uniquely matching connected player, and `/r <message>`
replies to the last sender. Replies are cleared on disconnect/slot reuse. Text
mutes also block private messages, replies and admin chat. Outgoing messages
are sanitized and bounded to fit the native client protocol.

## This TC:E Windows integration

42 admin commands are registered (57 total command-module entries). Core paths
were exercised on obj_railhouse with a real client and Omni-bots: session login,
permission denial, chat mute/unmute, rename/team changes, slap/gib, warn/history,
player listing, private messages/reply, pause/unpause, kick, permanent ban storage,
showbans/unban, ACL level storage and map-restart persistence. Reconnection while
banned and every combination of all commands have not been exhaustively tested.

Only SQLite is bundled; MySQL is not supported in this build. Native TC:E team
balance/lock rules can refuse a forced move, and this is reported rather than
bypassed. Original incognito permission can display an owner as Guest in the
player list; it does not remove their session permissions.

The local installation adds `exec wolfadmin.cfg` before map startup and an
editable `wolfadmin-private.cfg` template with password login disabled. Set your
own password there. No test credentials, test database or test modules are installed.
