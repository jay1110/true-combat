# Changelog

Player and server changes in TCE2, an unofficial reconstruction and community continuation of True Combat: Elite. Version headings follow the version milestones recorded in this repository. Earlier entries distinguish the original TC:E distribution from the later reconstruction and community features; they are not official TC:E release announcements.

Configuration names are case-insensitive in the game. The [Cvar reference](Docs/cvars.md) documents defaults, flags, availability and the complete configuration history. Existing ET/TC:E settings are not presented as newly invented TCE2 features.

## 1.0.4 — 2026-11-08

### Added

- An independent Command Map Size control under View → Widget Size (`cg_commandMapScale`).
- A Scoreboard button in the in-game menu. The scoreboard stays open until Escape; the normal held Tab display remains available.
- A Minimize button in the in-game menu on Windows, and a native minimize control in the title bar of windowed ET 2.60b, without replacing the engine.

### Modified

- All four voice-menu variants use compact panels at the original left-middle position. Their size no longer follows the general menu-size setting; each panel accommodates its actual number of entries.
- The chat popup has a larger text-entry row and a thin focus underline, preserving its existing All, Team and Fireteam destinations and keyboard controls.
- Friendly bot groups can pass a request to give way through nearby teammates. Contact handling keeps a passing direction stable briefly instead of repeatedly switching sides.
- Doorway queues can approach their waiting positions in short checked steps instead of requiring one completely clear long movement.

### Fixed

- The “You killed …” notification remains at the lower-left corner when HUD size is reduced.
- The chat-field hover treatment no longer fills the typing line or obscures entered text.
- Bot groups have more opportunities to clear narrow approaches while retaining collision, floor and ledge checks. Bots still wait when there is no safe physical space.

### Cvar changes

- Added registrations: `cg_commandMapScale`.
- `cg_commandMapScale` defaults to `0.85`; `tce_version` reports `1.0.4`.

## 1.0.3 — 2026-11-06

### Added

- Rotating menu and loading backgrounds for daylight, dusk, night and winter scenes. Background settings include country and custom winter months; an optional launcher can supply location-specific weather and sunrise/sunset data.
- Independent compass and fireteam-panel size controls (`cg_compassScale`, `cg_fireteamScale`).
- Client-platform advertisement through `g_oss`, with the default extended to `2047`, including the Android x86 and x86_64 flags.

### Modified

- Team labels use the CQB-style presentation above the head: white for teammates, light green for the player's fireteam, with a short fade when visibility is lost.
- Menu hover and keyboard-focus indicators are more visible across the supplied palettes and transparency settings.
- Publisher logos use small shadows for contrast over bright or dark backgrounds, without a solid black footer bar.
- Fireteam followers keep stable formation slots and stop closing on a nearby leader. Following resumes at a greater distance to avoid rapid stop/start movement.
- Friendly bots give human teammates priority and try checked positions beside their path. Door recipes can reserve a passage for one bot at a time, with waiting positions and arrival ordering.

### Fixed

- Background settings survive restarts. The startup menu retains its full size; scene backgrounds and the connection screen remain full-bleed.
- Grey/Black and custom-opacity Appearance settings no longer expose the incorrect background surface or missing-image pattern.
- Profile creation offers the same supported resolution choices as System settings.
- Command-map grid lines stay inside the map, and player-heading indicators on the map and compass face the correct direction.
- Deployment uses the correct TCE weapon previews and full weapon descriptions, with more readable selection lists.
- Team-name and crosshair-name settings are available in the HUD options. Overhead labels have proper head clearance and retain the last visible position only during their brief fade.
- Compass and fireteam text placement respects their separately scaled panels, including long location and objective labels.
- Existing bots apply changes to `/bot ft_controls` without a map restart. This is an Omni-bot command, not an ET Cvar.
- A stale bot reference in the shared path planner no longer survives reuse after a bot is removed.

### Cvar changes

- Added registrations: `cg_compassScale`, `cg_fireteamScale`, `g_oss`, `ui_backgroundCountry`, `ui_backgroundLast`, `ui_backgroundMode`, `ui_backgroundSession`, `ui_backgroundWinterMonths`.
- Modified registrations: `cg_drawTeamNames`, `g_oss`.
- `g_oss` was introduced with default `511`, then extended to `2047`; `tce_version` reports `1.0.3`.

## 1.0.2 — 2026-10-26

### Added

- A modern menu theme with reorganized main, options, server-browser, profile, replay, vote and referee screens.
- Redesigned deployment, scoreboard, connection and loading views, retaining the existing team, class, equipment and match information.
- Appearance settings for palettes, transparency presets and custom accent/surface RGB colors. Menu and gameplay-HUD sizes can be adjusted separately.
- Expanded fireteam presentation with health, role, weapon and location information; updated compass, objective and map symbols.
- TCE weapon statistics for shots, hits, accuracy, kills, deaths, headshots and damage. The Weapon Stats action can be assigned in Controls.
- Optional country flags using server-provided country data.
- A bindable action for dropping the GT5 bomb.

### Modified

- Player-pose information is shared with server hit detection and antilag calculations, keeping reconstructed hit geometry closer to the displayed player.
- GT7 custom deathmatch spawns prefer positions far from the nearest living opponent and account for blocked or occupied candidates.
- Menus and HUD elements use aspect-aware layout and bounded scaling. Font handling includes classic ET 2.60b assets and improved low-resolution text fitting.
- The standard-map bot navigation and GT2/GT5 mission data cover Delta, Northport, Railhouse, Snow, Stadtrand and Village.

### Fixed

- Reload cancellation and weapon switching no longer grant ammunition or transfer a reload timer to another weapon. Weapon-raise transitions and firearm selection after the last grenade are corrected.
- Leaning permits the intended hip fire. Jump and ledge assistance respect stamina and landing delays; sprint footsteps remain audible while leaning or preparing a grenade.
- Hit registration recovers after respawns and animation-state resets. Moving-target angle rewind uses the shorter rotational path.
- Confirmed head, body and leg hits retain the intended regional damage without an additional reduction at the edge of a confirmed hit zone.
- Precise hit positions are retained for damage calculation before network impact effects are rounded.
- Grenade collision respects a nearer wall when glass or player clip lies behind it; destroyed glass no longer causes a second bounce. Solid cover blocks the former residual blast damage to players.
- Shooting a protected GT7 opponent no longer adds an unintended ROE penalty to the attacker.
- Bomb release on death or disconnect remains available beyond the previous two-drop limit. Interrupted objective actions are cleared correctly.
- Rotating doors reverse without an angle jump, including after quiet opening or being pushed open.
- Round and statistics resets discard stale kills, deaths, weapon statistics, health and scoreboard state.
- Visibility checks through transparent glass still respect a solid wall behind the glass. Weather-effect restarts and impact-mark handling are stabilized.
- Server-browser queries, occupancy filters, favorites, ping handling and table widths are corrected. Control values, labels, sliders and wrapped messages fit their intended fields more consistently.

### Removed

- The experimental enhanced-graphics settings, material overlays and muzzle-light treatment. The original Set Recommended menu behavior was restored.
- The withdrawn graphics preset no longer creates or reads its temporary UI state: `ui_tceGfx_aa`, `ui_tceGfx_af`, `ui_tceGfx_canRestore`, `ui_tceGfx_compress`, `ui_tceGfx_filter`, `ui_tceGfx_hasAA`, `ui_tceGfx_old_aa`, `ui_tceGfx_old_af`, `ui_tceGfx_old_afdegree`, `ui_tceGfx_old_afmax`, `ui_tceGfx_old_bits`, `ui_tceGfx_old_compress`, `ui_tceGfx_old_curve`, `ui_tceGfx_old_detail`, `ui_tceGfx_old_fbo`, `ui_tceGfx_old_filter`, `ui_tceGfx_old_lod`, `ui_tceGfx_old_mips`, `ui_tceGfx_old_texture`, `ui_tceGfx_texture`.
- Waypoint/goal editor commands, authoring displays and navigation diagnostics from normal public gameplay builds. Ordinary Omni-bot gameplay remains available.

### Cvar changes

- Added registrations: `cg_countryflags`, `cg_drawIcons`, `cg_hudMaxHeight`, `cg_hudScale`, `cg_weaponLight`, `cg_wideScreen`, `g_countryflags`, `g_debugBulletsClient`, `tce_pose`, `tce_poseChallengeCounter`, `tce_poseProtocol`, `tce_poseStep`, `tceft`, `tceftgear`, `ui_menuMaxHeight`, `ui_menuScale`, `ui_themeAccentB`, `ui_themeAccentG`, `ui_themeAccentR`, `ui_themeBackgroundB`, `ui_themeBackgroundG`, `ui_themeBackgroundR`, `ui_themeOpacity`, `ui_themeOpacityPreset`, `ui_themePreset`.
- Modified registrations: `bot_editWaypoints`, `cg_countryflags`, `cg_drawCrosshairNames`, `cg_drawTeamNames`, `cg_omnibot_render_distance`, `cg_omnibotdrawing`, `mapname`, `omnibot_tce_explore_debug`, `ui_menuMaxHeight`, `ui_menuScale`.
- Removed registrations: `cg_weaponLight`.
- `cg_weaponLight` was introduced by the graphics experiment and removed again when that experiment was withdrawn. Authoring/drawing registrations were restricted to the separate Editor build; they are not ordinary gameplay controls. `tce_version` reports `1.0.2`.

## 1.0.1 — 2026-10-18

### Added

- Gametype-specific Omni-bot mission scripts and goals, allowing one map to define different GT2, GT5 and GT7 tasks without mixing an incomplete mode with another mode's files.
- Map-script announcements connected to Omni-bot triggers, and runtime route variation over existing navigation links.
- Map-controlled `info_player_deathmatch` spawn support for GT7.

### Modified

- Bot interaction uses TCE's activation action for object pickup and return, bomb tasks and switches. Attack, defend and hold tasks use role allocation and adjusted holding times while retaining priority for mission objectives.
- Bot traversal follows TCE movement rules for jumps, ladders, ledge climbing, crouched passages and safe descents. Door and switch routes can open, wait and cross when the map supplies suitable navigation.

### Fixed

- Reconstructed walking, sprinting, crouching, prone movement, jumping, landing, water movement and collision handling were corrected toward the reference game behavior.
- Reloading, ammunition, weapon changes, aiming, recoil, weapon motion and animation transitions were corrected in the reconstructed client and server paths.
- Platform-specific corrections cover weapon views, scopes, crosshairs, shot/impact effects, smoke, water effects and event audio on Windows and Linux.
- Spawn candidates are checked against world collision instead of accepting unsuitable positions inside solid geometry.

### Cvar changes

- Added registrations: `cg_drawCrosshairNames`, `omnibot_tce_explore`, `omnibot_tce_explore_debug`, `tce_scannerRecipientPackets`.
- Modified registrations: `cg_omnibot_render_distance`, `mapname`.
- Restored `cg_drawCrosshairNames` after its temporary registration removal in 1.0.0. `tce_version` reports `1.0.1`.

## 1.0.0 — 2026-10-10

### Added

- The first repository milestone identifying the mod as TCE2 1.0.0, with community version reporting for the client and server.
- `lua_status` reporting and an operator-only `lua_restart` command that reloads server scripts without restarting the map or disconnecting players. Script authentication sessions are recreated during a reload.
- TC:E-aware WolfAdmin session statistics, map listing and next-map voting support, persistent spree records, and native slap/gib feedback.
- Initialization of missing or invalid client GUIDs from an existing ET key, while preserving valid GUIDs and leaving the key file unchanged.

### Modified

- Established the versioned baseline for the reconstructed TC:E client presentation, user interface and server gameplay, retaining Reinforced Objective (GT2), Objective (GT5) and Bodycount (GT7).
- Carried forward TCE-specific Omni-bot gameplay and the optional server Lua/WolfAdmin integration introduced before the versioned milestones below.

### Cvar changes

- Added registrations: `cg_drawTeamNames`, `cl_guid`, `g_mapScriptDirectory`.
- Modified registrations: `tce_version`.
- Removed registrations: `cg_drawCrosshairNames`.
- The temporary removal of the `cg_drawCrosshairNames` registration was reversed in 1.0.1. Cvar-name capitalization does not create a separate setting; `tce_version` reports `1.0.0`.

## Before 1.0.0

### Added

- **2026 source adaptation:** reconstructed TC:E-specific client, UI and server behavior within the ET mod interfaces. This is a community reconstruction, not an official release of the original TC:E source.
- **Omni-bot integration:** TCE team, class, weapon and equipment handling, plus runtime map navigation and mission scripting.
- **Server Lua:** optional Lua 5.4 scripting with game/client lifecycle callbacks, console/client commands, damage, weapon, spawn and chat hooks, and bounded access to entity and server data.
- **WolfAdmin adaptation:** optional permission management, kick/ban, mute, team/map/bot administration, SQLite persistence and VFS-backed Lua loading. Private session authentication and explicit GUID-trust policy avoid treating a player name or slot as a persistent administrator identity.
- **Player communication:** private messages and replies through `/pm`, `/m` and `/r` when the corresponding administration module is enabled.

### Modified

- **2015–2017 distribution history:** the repository packaged TC:E 0.49 and its 0.49b update, supplied client/dedicated-server launch configurations and added Windows and Linux ALSA launch support. These entries describe distribution support, not original TC:E gameplay authored by this project.

### Fixed

- **2016 server browser:** supplied a server-list compatibility fix for the legacy packaged client.

### Cvar changes

- Added registrations: `bot_editWaypoints`, `cg_aspectFovMode`, `cg_aspectMode`, `cg_drawFriend`, `cg_dynamicEye`, `cg_freeAim`, `cg_gun_foreshorten`, `cg_gunPitch`, `cg_gunPosition`, `cg_gunRoll`, `cg_gunYaw`, `cg_hudAlpha`, `cg_omnibot_render_distance`, `cg_omnibotdrawing`, `cg_portalScopes`, `cg_predictBullets`, `cg_recording_showstatusline`, `cg_snd_reverb`, `cg_specSwing`, `cg_tacX`, `cg_tacY`, `cg_tacZ`, `cg_thirdPersonOffset`, `cg_toggleAiming`, `cg_toggleCrouch`, `cg_vip`, `g_aabasetime`, `g_botvar`, `g_killmessage`, `g_leanmode`, `g_newbbox`, `g_realism`, `g_starthonor`, `g_wolfadminTrustGuid`, `lua_modules`, `omnibot_enable`, `omnibot_flags`, `omnibot_path`, `omnibot_playing`, `r_fastsky`, `sv_gametype`, `sv_official`, `tce_version`, `wolfadmin_password`.
- Modified registrations: `cg_draw2D`, `cg_drawCrosshair`, `cg_drawGun`, `cg_fov`, `cg_shadows`, `g_autoFireteams`, `g_friendlyFire`, `g_gametype`, `g_gravity`, `g_lms_matchlimit`, `g_lms_roundlimit`, `g_speed`, `g_warmup`, `match_minplayers`, `ui_gametype`, `ui_menuFiles`, `ui_netGametype`.
- This list includes reconstructed TC:E controls, inherited engine settings given mod-side registrations, and early bot diagnostics. It does not imply these were all newly designed gameplay features or that authoring settings are available in current Game builds.

## Configuration history

The lists above cover every named registration addition, modification and removal recorded in the [Cvar history](Docs/cvars-history.tsv). A registration change can mean a default, flag, module binding or Game/Editor availability change; it does not always change the visible default. The history contains the individual commit, date and before/after registration for each event. Version-string updates to `tce_version` are stated explicitly because they can be supplied by a header rather than by a registration-table edit.

The [current Cvar reference](Docs/cvars.md) also distinguishes inherited settings, ROM/protocol fields, diagnostic controls, script-read gates and engine/launcher inputs. No bot-distance, crowd-yield or door-queue tuning Cvars were introduced in 1.0.3 or 1.0.4; `/bot ft_controls` remains an Omni-bot command.

Additional script and launcher values are documented separately from registered controls:

- **1.0.1 bot scripts:** `omnibot_tce_roamcamp`, `omnibot_tce_roamcamp_debug` and `omnibot_tce_objective_debug` are script-read gates, not registered distance or traffic controls.
- **1.0.2 withdrawn graphics preset:** the temporary `ui_tceGfx_*` names are listed under Removed above. They are historical UI state, not supported current commands.
- **1.0.3 weather handoff:** `ui_backgroundLocation`, `ui_backgroundWeatherStamp`, `ui_backgroundUtcOffset`, `ui_backgroundSunrise`, `ui_backgroundSunset` and `ui_backgroundSnow` are launcher-supplied inputs. `ui_backgroundStatus` is generated status, not a user preference.

See the reference for these values and for inherited engine inputs used by the mod; an engine setting does not become a new mod feature merely because a menu or launcher writes it.

Bot traversal and door behavior require appropriate map navigation. Route variation selects existing paths; it does not generate new paths during a match. Seasonal artwork is not an observed snow report; actual weather is used only when supplied through the optional weather mode. Lua and WolfAdmin are optional server features.
