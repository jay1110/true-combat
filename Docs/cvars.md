# Console variables

This reference describes **TCE2 1.0.4**, including retained TC:E controls and changes made before the first versioned TCE2 release. It covers the mod layer on ET 2.60b and ETLegacy. Engine settings are not new TCE2 features merely because a mod module registers or reads them.

The current source contains **636 registration sites and 501 named cvars**. There are 502 case-insensitive keys in total: two retained unused limbo bindings register the same empty name. The detailed sections below cover all current non-SDK registrations, every current name with a registration change in the examined history, and related existing HUD/hit-diagnostic controls. The complete appendix includes unchanged inherited SDK registrations as well.

- {download}`Complete registration appendix: defaults, flags, guards and source locations <cvars-registration.tsv>`
- {download}`Reachable registration history: 120 events <cvars-history.tsv>`
- {download}`Reference descriptions and value ranges <cvars-descriptions.tsv>`

## How to read this reference

Run a client setting in the console, for example `set cg_commandMapScale 0.85`. Set server options in the server configuration or at launch; remote administration uses the server's authenticated RCON facility. A setting does not override server authority, cheat protection, a read-only flag or a compile-time feature boundary.

**Defaults are source registration defaults, not a promise about the current runtime value.** Configurations, mapscripts, saved profiles, engine defaults and registration order can change the effective value. QGame is the server game module; CGame is the client game module; UI is the menu module. UI+CGame denotes a shared translation unit: a registration can still be restricted to one VM by the conditions recorded in the appendix, as background preferences are restricted to UI. Multiple listed bindings refer to the same engine cvar, not independent copies. Cvar names are case-insensitive, so `cg_drawCrosshairNames` and its lowercase spelling are one setting.

`""` means an empty string; `0` in the flags column means no registration flags. Angle brackets denote a source expression rather than a literal default. In particular, `tce_version` resolves `TCE_VERSION` to **1.0.4** in this release; its history is 1.0.0 → 1.0.1 → 1.0.2 → 1.0.3 → 1.0.4. Header-only version bumps do not appear as changes to the registration expression in the history TSV.

| Flag | Meaning |
| --- | --- |
| `ARCHIVE` | Eligible for persistence in the engine configuration. |
| `USERINFO` | Sent as client user information; this does not authenticate its contents. |
| `SERVERINFO` | Published in server information. |
| `SYSTEMINFO` / `WOLFINFO` | Engine-managed information categories; preserve the host engine's protocol behavior. |
| `LATCH` | A change takes effect at the relevant restart/reinitialization boundary. |
| `ROM` | Read-only through ordinary console commands; owned by code/engine. |
| `INIT` | Initial configuration/command-line setting, subject to engine policy. |
| `CHEAT` | Subject to the host engine/server cheat restrictions. |
| `TEMP` | Temporary value, normally not archived. |
| `NORESTART` | Preserved by the engine's cvar restart handling. |

Flags in the tables omit the `CVAR_` prefix. The downloadable appendix preserves the full source expressions and numeric bit masks. Other inherited flags remain visible there.

## Availability and compatibility

**Game** is the normal public build; **Editor** additionally enables NavStudio/waypoint/goal authoring. Editor-only cvars cannot turn a Game binary into an editor. Normal Omni-bot AI remains available in Game when compiled with Omni-bot support. Lua/WolfAdmin controls require `FEATURE_LUA`; bot controls require `FEATURE_OMNIBOT`. See the exact preprocessor conditions in the registration appendix.

The scanner does not evaluate every target's preprocessor configuration. It preserves guarded registrations, including SDK BotAI source that is not the normal Omni-bot runtime. It also labels likely engine-owned mirrors. For example, `r_fastsky`, `sensitivity`, `cl_*`, `r_*` and `mapname` are not evidence that TCE2 implements an engine renderer, input backend or networking stack.

The modern theme requires the accompanying modern UI assets/runtime marker. Menu palettes and sizing do not add renderer features. Full-frame backgrounds and connecting screens remain full-frame; compact voice menus keep their original left-middle anchor. Widget scale settings are independent of ordinary menu scale.

## Settings reference

Each History note identifies inherited/pre-versioned origin or the first source registration event. It is not a claim that the feature was absent from the original TC:E binary. The retained legacy controls below are distinguished from later TCE2 additions.


### HUD and names


#### `cg_wideScreen`

Use resolution-aware, proportional HUD coordinates. Disabling this restores the retained legacy aspect path for ordinary HUD elements.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`19a5024`](https://github.com/jay1110/true-combat/commit/19a5024)). **Registration sources:** [src/cgame/cg_hud_viewport.c:15](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L15), [src/ui/ui_main.c:10503](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10503).


#### `cg_hudScale`

Scale the general HUD around each widget's screen anchor. Compass, fireteam panel and expanded command map have separate controls.

**Values:** 0.5–1.5; fitted to the screen.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `.85` | `ARCHIVE` | Game/Editor |
| CGame | `0.85` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`19a5024`](https://github.com/jay1110/true-combat/commit/19a5024)). **Registration sources:** [src/cgame/cg_hud_viewport.c:16](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L16), [src/ui/ui_main.c:10506](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10506).


#### `cg_hudMaxHeight`

Cap the nominal physical height used by the general HUD before its scale is applied.

**Values:** Positive pixel height; 0 removes this cap.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1080` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`19a5024`](https://github.com/jay1110/true-combat/commit/19a5024)). **Registration sources:** [src/cgame/cg_hud_viewport.c:17](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L17), [src/ui/ui_main.c:10510](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10510).


#### `ui_menuScale`

Scale ordinary menus. The startup main menu, full-frame backgrounds, loading screens and compact voice menu deliberately use separate fixed/fitted scopes.

**Values:** 0.5–1.5; controls remain on screen.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`19a5024`](https://github.com/jay1110/true-combat/commit/19a5024)). **Registration sources:** [src/cgame/cg_hud_viewport.c:18](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L18), [src/ui/ui_main.c:10504](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10504).


#### `ui_menuMaxHeight`

Cap the nominal height of ordinary menus before ui_menuScale.

**Values:** Positive pixel height; 0 removes this cap.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1080` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`19a5024`](https://github.com/jay1110/true-combat/commit/19a5024)). **Registration sources:** [src/cgame/cg_hud_viewport.c:19](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L19), [src/ui/ui_main.c:10505](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10505).


#### `cg_compassScale`

Resize the top-right compass independently of general HUD/menu size.

**Values:** 0.5–1.5; screen bounds still apply.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `.85` | `ARCHIVE` | Game/Editor |
| CGame | `0.85` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`1beb008`](https://github.com/jay1110/true-combat/commit/1beb008)). **Registration sources:** [src/cgame/cg_hud_viewport.c:20](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L20), [src/ui/ui_main.c:10507](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10507).


#### `cg_fireteamScale`

Resize the top-left fireteam panel independently of general HUD/menu size.

**Values:** 0.5–1.5; screen bounds still apply.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `.85` | `ARCHIVE` | Game/Editor |
| CGame | `0.85` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`1beb008`](https://github.com/jay1110/true-combat/commit/1beb008)). **Registration sources:** [src/cgame/cg_hud_viewport.c:21](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L21), [src/ui/ui_main.c:10508](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10508).


#### `cg_commandMapScale`

Resize the expanded command map independently of the HUD and compass. Its border and content share a right-anchored, uniform projection.

**Values:** 0.5–1.5; View → Widget Size.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `.85` | `ARCHIVE` | Game/Editor |
| CGame | `0.85` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.4 ([`fc5ae89`](https://github.com/jay1110/true-combat/commit/fc5ae89)). **Registration sources:** [src/cgame/cg_hud_viewport.c:22](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_hud_viewport.c#L22), [src/ui/ui_main.c:10509](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10509).


#### `cg_drawTeamNames`

Show teammate names above their heads. Teammates are white, own-fireteam names light green; only the last visible position briefly fades after occlusion.

**Values:** 0 off; 1 team; 2 own fireteam only.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.0 ([`7707e8c`](https://github.com/jay1110/true-combat/commit/7707e8c)). **Registration sources:** [src/cgame/cg_main.c:382](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L382), [src/ui/ui_main.c:10553](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10553).


#### `cg_drawCrosshairNames`

Show the name of the player under the crosshair; this is separate from overhead team labels.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `0` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:380](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L380), [src/ui/ui_main.c:10552](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10552), [src/ui/ui_main.c:10655](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10655).


#### `cg_drawFriend`

Legacy teammate identification icons and nearby teammate indicators. This is separate from cg_drawTeamNames.

**Values:** 0 off; 1 visible; 2 always, subject to the retained rendering rules.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `2` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:591](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L591).


#### `cg_drawIcons`

Enable icon/model details in the TCE fireteam presentation.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`074ee19`](https://github.com/jay1110/true-combat/commit/074ee19)). **Registration sources:** [src/cgame/cg_tce_fireteam.c:32](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_tce_fireteam.c#L32).


#### `cg_drawFireteamOverlay`

Show the fireteam HUD when the local player belongs to a fireteam. Existing SDK setting with a TCE-specific renderer.

**Values:** 0 off; nonzero on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:557](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L557).


#### `cg_drawCompass`

Choose the compass presentation. The TCE map/blueprint options require corresponding map artwork; the renderer retains a safe fallback if art is missing.

**Values:** 0 off; 1 classic; 2 map; 3 blueprint.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:491](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L491), [src/ui/ui_main.c:10650](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10650).


#### `cg_countryflags`

Show available country flags beside scoreboard player names. Requires server country data.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame, UI | `1` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`4770335`](https://github.com/jay1110/true-combat/commit/4770335)). **Registration sources:** [src/cgame/cg_main.c:381](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L381), [src/ui/ui_main.c:10649](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10649).


### Menu appearance


#### `ui_themePreset`

Choose the optional modern menu palette. Existing palette IDs are preserved; RGB controls are used only for Custom.

**Values:** 0 Gold; 1 Cyan; 2 Red; 3 Green; 4 Custom; 5 Black/Grey; 6 Grey/Black; 7 Slate; 8 Copper; 9 Violet.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `0` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:44](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L44).


#### `ui_themeOpacityPreset`

Select surface opacity. This affects menu surfaces, not text opacity or gameplay rendering.

**Values:** 0 Solid; 1 Balanced (0.85); 2 Clear (0.60); 3 Custom.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `0` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:45](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L45).


#### `ui_themeOpacity`

Custom surface opacity, used only when ui_themeOpacityPreset is 3.

**Values:** 0.25–1; larger values are more opaque.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.85` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:46](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L46).


#### `ui_themeAccentR`

Custom accent red channel; used when ui_themePreset is 4.

**Values:** 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.96` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:49](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L49).


#### `ui_themeAccentG`

Custom accent green channel; used when ui_themePreset is 4.

**Values:** 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.73` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:49](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L49).


#### `ui_themeAccentB`

Custom accent blue channel; used when ui_themePreset is 4.

**Values:** 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.27` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:49](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L49).


#### `ui_themeBackgroundR`

Custom menu-surface red channel; used when ui_themePreset is 4.

**Values:** 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.035` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:50](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L50).


#### `ui_themeBackgroundG`

Custom menu-surface green channel; used when ui_themePreset is 4.

**Values:** 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.055` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:50](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L50).


#### `ui_themeBackgroundB`

Custom menu-surface blue channel; used when ui_themePreset is 4.

**Values:** 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `.075` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`9845ba9`](https://github.com/jay1110/true-combat/commit/9845ba9)). **Registration sources:** [src/ui/tce_modern_theme.inc:50](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_theme.inc#L50).


### Background selection


#### `ui_backgroundMode`

Choose seasonal, optional weather-driven, or time-of-day-only background selection. The selection is made once per game process and kept through UI/map reloads.

**Values:** 0 Season; 1 Live weather; 2 Time of day only.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `0` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`85705fa`](https://github.com/jay1110/true-combat/commit/85705fa)). **Registration sources:** [src/ui/tce_modern_ui.c:187](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L187), [src/ui/tce_modern_ui.c:229](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L229).


#### `ui_backgroundCountry`

Country code used for seasonal artwork rules. It is not an IP-derived location or an observed snow report.

**Values:** Two-letter uppercase country code; default DE.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `DE` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`85705fa`](https://github.com/jay1110/true-combat/commit/85705fa)). **Registration sources:** [src/ui/tce_modern_ui.c:187](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L187), [src/ui/tce_modern_ui.c:230](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L230).


#### `ui_backgroundWinterMonths`

Override the seasonal winter-artwork months. Automatic defaults use northern or southern winter rules; unknown/tropical countries do not infer snow.

**Values:** auto; none; or comma-separated months 1–12, such as 12,1,2.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `auto` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`85705fa`](https://github.com/jay1110/true-combat/commit/85705fa)). **Registration sources:** [src/ui/tce_modern_ui.c:187](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L187), [src/ui/tce_modern_ui.c:231](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L231).


#### `ui_backgroundLast`

Archived previous background selection, used to avoid immediately repeating the same appropriate picture.

**Values:** Internal selection ID; normally leave unchanged.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `0` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`85705fa`](https://github.com/jay1110/true-combat/commit/85705fa)). **Registration sources:** [src/ui/tce_modern_ui.c:187](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L187), [src/ui/tce_modern_ui.c:227](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L227).


#### `ui_backgroundSession`

Current process background selection. This is transient UI state rather than a permanent picture selector.

**Values:** 0 before selection; internal IDs 1–16.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI+CGame | `0` | `0` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`85705fa`](https://github.com/jay1110/true-combat/commit/85705fa)). **Registration sources:** [src/ui/tce_modern_ui.c:228](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c#L228).


### Server gameplay


#### `g_gametype`

Server game mode. Select it before startup/map load; changing it inside a running TCE game does not perform a supported mode transition. CGame's registration is only a mirror.

**Values:** 2 Reinforced Objective; 5 Objective; 7 Bodycount.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `0` | Game/Editor |
| QGame, UI | `5` | `SERVERINFO\|LATCH` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:485](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L485), [src/game/g_main.c:268](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L268), [src/ui/ui_main.c:10633](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10633).


#### `sv_official`

Retained TC:E official-map policy. Custom-map and custom-mode setups commonly start with this disabled.

**Values:** 0 permits custom setups; 1 applies the official-map policy.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `SERVERINFO` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:270](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L270).


#### `sv_gametype`

Read-only TC:E game-type state, separate from the operator-facing g_gametype selection.

**Values:** Engine/game-maintained; do not configure.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `ROM` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:269](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L269).


#### `g_mapScriptDirectory`

Directory checked for custom mapscript overrides before the packaged map script. An empty registered default does not mean that local launch configurations cannot select mapscripts.

**Values:** Empty or an override directory such as mapscripts.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `""` | `0` | Game/Editor |

**History:** First recorded registration: 1.0.0 ([`7707e8c`](https://github.com/jay1110/true-combat/commit/7707e8c)). **Registration sources:** [src/game/g_main.c:392](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L392).


#### `g_newbbox`

Select the retained TC:E player-body/hull profile; the presentation/hit-registration code also uses this profile's scale.

**Values:** 0 legacy smaller profile; 1 enlarged profile; set consistently before playing.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `SERVERINFO\|NORESTART` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:323](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L323).


#### `g_realism`

Retained TC:E realism/movement profile. In particular, 1 selects the normal 0.45 run-speed scale; the alternate branch uses 0.51.

**Values:** Legacy server policy; default 1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `SERVERINFO\|LATCH` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:327](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L327).


#### `g_leanmode`

Retained TC:E lean presentation and shot-position policy, published to clients.

**Values:** 0 versus positive values select the retained lean policies.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `SERVERINFO\|LATCH` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:324](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L324).


#### `g_killmessage`

Retained kill-message policy published to clients; distinct from the new HUD scaling settings.

**Values:** Legacy TC:E mode value.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `SERVERINFO\|LATCH` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:325](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L325).


#### `g_starthonor`

Initial honor/armament-progression setting used by the retained TC:E round rules.

**Values:** Legacy server setting; default 0.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `SERVERINFO\|LATCH` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:326](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L326).


#### `g_aabasetime`

Read-only armament-availability progression interval. Mapscript actions set it; non-GT5 modes use the interval in minutes.

**Values:** Internal/map-script-owned minutes; default 5.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `5` | `ROM` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:328](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L328).


#### `g_botvar`

Multiplier used by the retained TC:E bot-info enemy-selection probability code. It is not an Omni-bot crowd-distance or skill control.

**Values:** Legacy multiplier; default 1.0.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1.0` | `0` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:329](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L329).


#### `g_antilag`

Existing SDK antialag setting extended with optional TCE hit-debug layers. Bit 1 enables rewind; bits 2 and 4 request the corresponding rewound/current diagnostic shot layers.

**Values:** Bitmask: 1 rewind; 2 rewound-shot diagnostics; 4 current-shot diagnostics.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `0` | Game/Editor |
| QGame, UI | `1` | `ARCHIVE\|SERVERINFO` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:496](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L496), [src/game/g_main.c:394](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L394), [src/ui/ui_main.c:10662](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10662).


#### `g_debugBullets`

Existing cheat-protected bullet diagnostic selector. TCE additions use bit 2 for box/trace drawing, 4 for periodic samples, and 8 for bounded diagnostic logging.

**Values:** 0 off; combine supported diagnostic bits; sv_cheats is required.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:359](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L359).


#### `g_debugBulletsClient`

Limit TCE hit diagnostics to a client slot.

**Values:** -1 all; otherwise a valid client number.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `-1` | `CHEAT` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`19a5024`](https://github.com/jay1110/true-combat/commit/19a5024)). **Registration sources:** [src/game/g_main.c:360](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L360).


### Server information and administration


#### `g_countryflags`

Enable server country lookup and choose bot nationality handling. Uses an optional local IPv4 GeoIP.dat; missing data falls back to unknown/local identifiers.

**Values:** 0 off; 1 real players plus configured server address for bots; 2 random bot country icons.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `ARCHIVE\|LATCH` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`4770335`](https://github.com/jay1110/true-combat/commit/4770335)). **Registration sources:** [src/game/g_main.c:298](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L298).


#### `g_oss`

Advertise available client-platform binaries to compatible browsers. This does not install modules, enforce architecture access or detect deployed files.

**Values:** Bitmask; 2047 advertises all eleven represented platform flags.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `2047` | `SERVERINFO` | Game/Editor |

**History:** First recorded registration: 1.0.3 ([`1beb008`](https://github.com/jay1110/true-combat/commit/1beb008)). **Registration sources:** [src/game/g_main.c:299](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L299).


#### `lua_modules`

Space/semicolon-separated server Lua module paths loaded by the optional FEATURE_LUA runtime. Use lua_restart in the server console/RCON after changing the list.

**Values:** Empty disables module loading; example lua/wolfadmin/main.lua.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `""` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`37fd8bc`](https://github.com/jay1110/true-combat/commit/37fd8bc)). **Registration sources:** [src/game/tce_lua.c:218](https://github.com/jay1110/true-combat/blob/final/src/game/tce_lua.c#L218).


#### `wolfadmin_password`

Optional WolfAdmin session-login password. Empty disables password login; keep a configured value in a server-private configuration.

**Values:** Empty or a private operator-selected password; not serverinfo/userinfo.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `""` | `0` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`9f86d22`](https://github.com/jay1110/true-combat/commit/9f86d22)). **Registration sources:** [src/game/tce_lua.c:216](https://github.com/jay1110/true-combat/blob/final/src/game/tce_lua.c#L216).


#### `g_wolfadminTrustGuid`

Explicitly allow WolfAdmin to trust a unique valid client-supplied GUID. TCE's GUIDs are not engine-authenticated; console administration does not need this opt-in.

**Values:** 0 default distrust; 1 explicit GUID trust opt-in.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `0` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`9f86d22`](https://github.com/jay1110/true-combat/commit/9f86d22)). **Registration sources:** [src/game/tce_lua.c:217](https://github.com/jay1110/true-combat/blob/final/src/game/tce_lua.c#L217).


#### `cl_guid`

Existing engine/client identity variable. The UI preserves a valid existing GUID, otherwise derives one from an existing etmain/etkey for classic ET; it does not create or replace the key.

**Values:** 32 hexadecimal characters supplied/derived at runtime; not a fixed default.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `""` | `USERINFO` | Game/Editor |
| UI | `<guid>` | `USERINFO` | Game/Editor |

**History:** First recorded registration: 1.0.0 ([`7707e8c`](https://github.com/jay1110/true-combat/commit/7707e8c)). **Registration sources:** [src/ui/tce_client_guid.c:22](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_client_guid.c#L22), [src/ui/tce_client_guid.c:46](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_client_guid.c#L46).


### Omni-bot runtime


#### `omnibot_enable`

Load the optional Omni-bot gameplay runtime on map initialization. The matching core library and scripts must be installed.

**Values:** 0 off; 1 on; latched.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `ARCHIVE\|LATCH` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`fcf641e`](https://github.com/jay1110/true-combat/commit/fcf641e)). **Registration sources:** [src/game/g_main.c:253](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L253).


#### `omnibot_path`

Optional Omni-bot core-library search path. Empty uses the loader's platform-specific search locations.

**Values:** Empty or a configured core-library directory; latched.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `""` | `ARCHIVE\|LATCH` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`fcf641e`](https://github.com/jay1110/true-combat/commit/fcf641e)). **Registration sources:** [src/game/g_main.c:254](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L254).


#### `omnibot_flags`

Retained Omni-bot option bitmask. Its declared bits cover XP-save, tank/gun mounting, advertised bot count, gib targeting, mine handling and shove integration; only applicable TCE interactions have an effect.

**Values:** 0 baseline; declared bits 1, 2, 4, 8, 16, 32, 64.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`fcf641e`](https://github.com/jay1110/true-combat/commit/fcf641e)). **Registration sources:** [src/game/g_main.c:255](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L255).


#### `omnibot_playing`

Read-only bot-count/status information maintained by the integration.

**Values:** 0 initially; positive active count; -1 when disabled in the update path.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `SERVERINFO\|ROM` | Game/Editor |

**History:** First recorded registration: pre-1.0.0 ([`fcf641e`](https://github.com/jay1110/true-combat/commit/fcf641e)). **Registration sources:** [src/game/g_main.c:256](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L256).


#### `omnibot_tce_explore`

Allow the TCE exploration goal when normal objective/combat conditions permit. It still uses installed navigation and cannot invent missing paths.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `ARCHIVE` | Game/Editor |

**History:** First recorded registration: 1.0.1 ([`f7e1622`](https://github.com/jay1110/true-combat/commit/f7e1622)). **Registration sources:** [src/game/g_main.c:249](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L249).


### Editor-only controls


#### `bot_editWaypoints`

Original navigation-editing state retained only in FEATURE_NAVSTUDIO authoring builds.

**Values:** Transient editor state; not a public Game control.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `TEMP` | Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:331](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L331).


#### `cg_omnibotdrawing`

Master Omni-bot waypoint/goal drawing switch in the authoring CGame.

**Values:** 0 off; 1 on; requires the Editor variant.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Editor |

**History:** First recorded registration: pre-1.0.0 ([`fcf641e`](https://github.com/jay1110/true-combat/commit/fcf641e)). **Registration sources:** [src/cgame/cg_main.c:350](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L350).


#### `cg_omnibot_render_distance`

Maximum distance for authoring navigation/goal drawing, limiting submitted debug geometry.

**Values:** Game units; default 800; requires the Editor variant.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `800` | `ARCHIVE` | Editor |

**History:** First recorded registration: pre-1.0.0 ([`fcf641e`](https://github.com/jay1110/true-combat/commit/fcf641e)). **Registration sources:** [src/cgame/cg_main.c:351](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L351).


#### `omnibot_tce_explore_debug`

Native registration for exploration diagnostics is restricted to FEATURE_NAVSTUDIO. Some installed GM scripts also read this name.

**Values:** 0 silent; nonzero script diagnostics; no public authoring interface.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `0` | Editor |

**History:** First recorded registration: 1.0.1 ([`f7e1622`](https://github.com/jay1110/true-combat/commit/f7e1622)). **Registration sources:** [src/game/g_main.c:251](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L251).


### Protocol and internal state


#### `tce_version`

Read-only TCE2 mod version string compiled from TCE_VERSION.

**Values:** 1.0.4 in the current source; version marker, not a tuning setting.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1.0.4 (TCE_VERSION)` | `SERVERINFO\|ROM` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/game/g_main.c:279](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L279).


#### `tce_pose`

CGame userinfo capability/acknowledgement for the shared pose-presentation protocol. The server issues and validates the runtime challenge.

**Values:** 0 initially; protocol-generated acknowledgement; do not set manually.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `USERINFO\|ROM` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`50b90f0`](https://github.com/jay1110/true-combat/commit/50b90f0)). **Registration sources:** [src/cgame/cg_tce_pose_presentation.c:23](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_tce_pose_presentation.c#L23).


#### `tce_poseChallengeCounter`

Server-maintained counter used to issue pose-protocol challenges.

**Values:** Internal read-only counter.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `ROM` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`50b90f0`](https://github.com/jay1110/true-combat/commit/50b90f0)). **Registration sources:** [src/game/g_tce_pose_presentation.c:82](https://github.com/jay1110/true-combat/blob/final/src/game/g_tce_pose_presentation.c#L82).


#### `tce_poseProtocol`

Server pose-presentation protocol identifier.

**Values:** 2; read-only serverinfo.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `2` | `SERVERINFO\|ROM` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`50b90f0`](https://github.com/jay1110/true-combat/commit/50b90f0)). **Registration sources:** [src/game/g_tce_pose_presentation.c:114](https://github.com/jay1110/true-combat/blob/final/src/game/g_tce_pose_presentation.c#L114).


#### `tce_poseStep`

Server-published pose update step. The initial 50 ms registration is replaced with the observed valid frame step.

**Values:** Read-only milliseconds; 50 is the initialization fallback.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `50` | `SERVERINFO\|ROM` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`50b90f0`](https://github.com/jay1110/true-combat/commit/50b90f0)). **Registration sources:** [src/game/g_tce_pose_presentation.c:115](https://github.com/jay1110/true-combat/blob/final/src/game/g_tce_pose_presentation.c#L115).


#### `tce_scannerRecipientPackets`

Enable the TCE scanner/compass recipient-packet extension.

**Values:** 0 opt out; 1 enabled; latched serverinfo.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `1` | `SERVERINFO\|LATCH` | Game/Editor |

**History:** First recorded registration: 1.0.1 ([`2fa37ff`](https://github.com/jay1110/true-combat/commit/2fa37ff)). **Registration sources:** [src/game/g_spawn.c:1307](https://github.com/jay1110/true-combat/blob/final/src/game/g_spawn.c#L1307).


#### `tceft`

Client userinfo capability for TCE fireteam information.

**Values:** 1; read-only capability declaration.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `USERINFO\|ROM` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`ecd77a9`](https://github.com/jay1110/true-combat/commit/ecd77a9)). **Registration sources:** [src/cgame/cg_tce_fireteam.c:30](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_tce_fireteam.c#L30).


#### `tceftgear`

Client userinfo capability for the richer fireteam equipment payload.

**Values:** 1; read-only capability declaration.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `USERINFO\|ROM` | Game/Editor |

**History:** First recorded registration: 1.0.2 ([`074ee19`](https://github.com/jay1110/true-combat/commit/074ee19)). **Registration sources:** [src/cgame/cg_tce_fireteam.c:31](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_tce_fireteam.c#L31).


### Legacy TC:E controls


#### `cg_aspectMode`

Legacy aspect-canvas selector. Automatic widescreen handling bypasses this selector for ordinary HUD geometry.

**Values:** Retained mode values 0–2.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:415](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L415).


#### `cg_aspectFovMode`

Legacy field-of-view adaptation policy for the selected TC:E aspect mode.

**Values:** Retained policy values 0–2.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:609](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L609).


#### `cg_dynamicEye`

Strength of the retained screen-brightness/eye-adaptation effect; this does not add an HDR renderer to the engine.

**Values:** 0 disables; the View slider exposes 0–1.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0.0` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:607](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L607).


#### `cg_freeAim`

Enable the TC:E free-aim input policy; the preference is sent in userinfo.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `ARCHIVE\|USERINFO` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:611](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L611).


#### `cg_gunPosition`

Horizontal first-person weapon placement.

**Values:** 0 more centered; 1 default; 2 more to the right.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:593](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L593).


#### `cg_portalScopes`

Enable the retained second-scene/portal scope presentation where the selected weapon supports it.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:595](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L595).


#### `cg_predictBullets`

Enable the retained client bullet-effect prediction path. Server damage remains authoritative.

**Values:** 0 off; 1 on.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:587](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L587).


#### `cg_toggleAiming`

Choose toggle-style aiming instead of holding the aiming action.

**Values:** 0 hold; 1 toggle.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE\|USERINFO` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:597](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L597).


#### `cg_toggleCrouch`

Choose toggle-style crouching instead of holding crouch.

**Values:** 0 hold; 1 toggle.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE\|USERINFO` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:589](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L589).


### Legacy TC:E diagnostics


#### `cg_gunPitch`

Developer first-person weapon pitch offset.

**Values:** Cheat-protected angle adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:579](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L579).


#### `cg_gunYaw`

Developer first-person weapon yaw offset.

**Values:** Cheat-protected angle adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:581](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L581).


#### `cg_gunRoll`

Developer first-person weapon roll offset.

**Values:** Cheat-protected angle adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:583](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L583).


#### `cg_tacX`

Developer tactical first-person weapon X offset.

**Values:** Cheat-protected position adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:573](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L573).


#### `cg_tacY`

Developer tactical first-person weapon Y offset.

**Values:** Cheat-protected position adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:575](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L575).


#### `cg_tacZ`

Developer tactical first-person weapon Z offset.

**Values:** Cheat-protected position adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:577](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L577).


#### `cg_gun_foreshorten`

Override positive weapon-model foreshortening in the retained view-weapon renderer.

**Values:** 0 normal behavior; positive developer override; cheat-protected.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:585](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L585).


#### `cg_thirdPersonOffset`

Side offset of the retained third-person camera.

**Values:** Cheat-protected camera adjustment.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:601](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L601).


#### `cg_snd_reverb`

Scale the retained room/sound-zone contribution. It is not an engine-wide new audio backend.

**Values:** Nominal 0–255; cheat-protected.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `255` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:599](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L599).


### Legacy compatibility registrations


#### `cg_hudAlpha`

Legacy TC:E archived binding. No current production reader of this variable was identified; modern menu transparency uses ui_themeOpacityPreset/ui_themeOpacity instead.

**Values:** Registered value retained for compatibility; do not use as the modern transparency control.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0.25` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:603](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L603).


#### `cg_specSwing`

Legacy archived binding; toggleswing still updates it, but no current presentation consumer was identified.

**Values:** Compatibility state; not a verified modern camera feature.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:569](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L569).


#### `cg_vip`

Legacy archived binding with no identified current production consumer.

**Values:** Retained compatibility state.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `ARCHIVE` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:571](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L571).


#### `cg_recording_showstatusline`

Legacy transient recording-status binding with no identified current production consumer.

**Values:** Retained compatibility state.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `0` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:613](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L613).


### Inherited SDK bindings changed before 1.0.0


#### `cg_draw2D`

Existing HUD drawing switch, made cheat-protected in the TC:E source snapshot.

**Values:** HUD suppression is additionally restricted by the existing developer check.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:374](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L374).


#### `cg_drawCrosshair`

Existing crosshair selector. CGame's TC:E default/cheat flags differ from the UI's inherited archived binding; the registrations share one engine cvar.

**Values:** Do not infer an effective runtime default from UI's registration alone.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |
| UI | `4` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:379](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L379), [src/ui/ui_main.c:10551](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10551).


#### `cg_drawGun`

Existing first-person gun drawing switch. CGame's TC:E binding is cheat-protected; the UI has an archived mirror.

**Values:** 0 hidden; nonzero drawn, subject to gameplay policy.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `1` | `ARCHIVE` | Game/Editor |
| CGame | `1` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:355](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L355), [src/ui/ui_main.c:10648](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10648).


#### `cg_fov`

Existing field-of-view setting with TC:E cheat-protected registration. Scope/aspect policies still affect the actual view.

**Values:** Legacy field-of-view preference; not an unrestricted public FOV override.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `90` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:367](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L367).


#### `cg_shadows`

Existing player-shadow presentation selector, cheat-protected by the TC:E binding.

**Values:** Retained legacy selector.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `1` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/cgame/cg_main.c:370](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L370).


#### `g_autoFireteams`

Existing automatic fireteam policy; TC:E registers it archived.

**Values:** 0 off; nonzero enabled.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `0` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:484](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L484).


#### `g_friendlyFire`

Existing friendly-fire policy. QGame uses SERVERINFO + CHEAT; UI adds an archived mirror of the same name.

**Values:** 0 off; 1 on, subject to protection/game-mode rules.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `1` | `ARCHIVE` | Game/Editor |
| QGame | `1` | `SERVERINFO\|CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:297](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L297), [src/ui/ui_main.c:10536](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10536).


#### `g_gravity`

Existing world-gravity setting, cheat-protected in this TC:E registration.

**Values:** Engine/game numeric setting; default 800.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `800` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:347](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L347).


#### `g_speed`

Existing base movement-speed parameter, cheat-protected in TC:E. Actual TCE movement also applies posture and realism scales.

**Values:** Default 320; not a direct promise of running speed.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `320` | `CHEAT` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:346](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L346).


#### `g_lms_matchlimit`

Existing SDK LMS match-limit binding. QGame and UI retain different registration defaults; TCE's documented public modes are 2, 5 and 7.

**Values:** Legacy SDK compatibility binding.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `2` | `ARCHIVE` | Game/Editor |
| QGame | `5` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:455](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L455), [src/ui/ui_main.c:10665](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10665).


#### `g_lms_roundlimit`

Existing SDK LMS round-limit binding. QGame and UI retain different registration defaults.

**Values:** Legacy SDK compatibility binding.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `3` | `ARCHIVE` | Game/Editor |
| QGame | `5` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:454](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L454), [src/ui/ui_main.c:10664](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10664).


#### `g_warmup`

Existing warmup duration. QGame registers 25, UI's inherited mirror registers 60; server/map configuration and registration order matter.

**Values:** Seconds; do not treat a UI mirror as the server's configured value.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `25` | `ARCHIVE` | Game/Editor |
| UI | `60` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:303](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L303), [src/ui/ui_main.c:10663](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10663).


#### `match_minplayers`

Existing minimum-player match setting. QGame and UI bindings have different persistence flags.

**Values:** Default 4.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `4` | `0` | Game/Editor |
| UI | `4` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/game/g_main.c:402](https://github.com/jay1110/true-combat/blob/final/src/game/g_main.c#L402), [src/ui/ui_main.c:10704](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10704).


#### `ui_gametype`

UI game-type selection state adapted to the TC:E default mode.

**Values:** UI state, not a substitute for startup g_gametype.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `5` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/ui/ui_main.c:10583](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10583).


#### `ui_netGametype`

UI network game-type selection state adapted to TC:E.

**Values:** UI selection/cache value.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `5` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/ui/ui_main.c:10585](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10585).


#### `ui_menuFiles`

UI menu registry path; TC:E selects the appropriate game-type registry.

**Values:** Initial registry ui/menus_gt5.txt; UI-owned state.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| UI | `ui/menus_gt5.txt` | `ARCHIVE` | Game/Editor |

**History:** Inherited ET SDK binding. **Registration sources:** [src/ui/ui_main.c:10582](https://github.com/jay1110/true-combat/blob/final/src/ui/ui_main.c#L10582).


### Engine-owned compatibility binding


#### `r_fastsky`

Engine renderer cvar also registered by the legacy TC:E CGame with a cheat flag. This is not a newly implemented TCE2 renderer setting.

**Values:** Use the host engine's documented behavior.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| CGame | `0` | `CHEAT` | Game/Editor |

**History:** Present in the pre-versioned TC:E source stage. **Registration sources:** [src/cgame/cg_main.c:605](https://github.com/jay1110/true-combat/blob/final/src/cgame/cg_main.c#L605).


#### `mapname`

Read-only current map information mirrored by several mod subsystems. Repeated binding changes in history are not additions of new maps or game modes.

**Values:** Engine/server-maintained map name.

| Module | Default | Flags | Build scope |
| --- | --- | --- | --- |
| QGame | `""` | `SERVERINFO\|ROM` | Editor |
| QGame | `""` | `SERVERINFO\|ROM` | Game/Editor |
| SDK BotAI | `""` | `SERVERINFO\|ROM` | Legacy SDK AI |

**History:** Inherited ET SDK binding. **Registration sources:** [src/botai/ai_main.c:2037](https://github.com/jay1110/true-combat/blob/final/src/botai/ai_main.c#L2037), [src/botai/ai_team.c:3434](https://github.com/jay1110/true-combat/blob/final/src/botai/ai_team.c#L3434), [src/game/g_cmds.c:3689](https://github.com/jay1110/true-combat/blob/final/src/game/g_cmds.c#L3689), [src/game/g_save.c:1491](https://github.com/jay1110/true-combat/blob/final/src/game/g_save.c#L1491), [src/game/g_save.c:2288](https://github.com/jay1110/true-combat/blob/final/src/game/g_save.c#L2288), [src/game/g_script.c:431](https://github.com/jay1110/true-combat/blob/final/src/game/g_script.c#L431), [src/game/g_stats.c:579](https://github.com/jay1110/true-combat/blob/final/src/game/g_stats.c#L579), [src/game/tce_nodes.c:21](https://github.com/jay1110/true-combat/blob/final/src/game/tce_nodes.c#L21), [src/game/tce_nodes.c:212](https://github.com/jay1110/true-combat/blob/final/src/game/tce_nodes.c#L212), [src/game/tce_nodes.c:244](https://github.com/jay1110/true-combat/blob/final/src/game/tce_nodes.c#L244), [src/game/tce_nodes.c:72](https://github.com/jay1110/true-combat/blob/final/src/game/tce_nodes.c#L72).

## Background weather inputs and session state

These names are **not native registration-table settings**. The launcher supplies weather inputs with `+set`; menu code reads them and writes status. Consequently they have no native registration default or flag mask in the appendix.

| Name | Producer / meaning |
| --- | --- |
| `ui_backgroundWeatherStamp` | Launcher; Unix time of the weather sample. `0` means no usable sample. |
| `ui_backgroundUtcOffset` | Launcher; location's UTC offset in seconds, bounded to −50400…50400 by the consumer. |
| `ui_backgroundSunrise` | Launcher; local sunrise minute of day. |
| `ui_backgroundSunset` | Launcher; local sunset minute of day. |
| `ui_backgroundSnow` | Launcher; `1` indicates snow for the supplied sample. |
| `ui_backgroundLocation` | Launcher; nonempty location label required for the live-weather path. |
| `ui_backgroundStatus` | UI-created descriptive status, not a user preference. |

Live weather requires a valid sample no more than three hours old. Without usable live data, selection falls back to local time/season rules, with 07:00/19:00 sunrise/sunset defaults. Country/winter-month preferences select seasonal artwork; they are not country-level weather measurements. `ui_backgroundLast` is the archived previous image identifier; `ui_backgroundSession` is a non-archived session choice, keeping an image stable across UI reloads and map changes within a process.

Sources: [background consumer](https://github.com/jay1110/true-combat/blob/final/src/ui/tce_modern_ui.c), [launcher weather arguments](https://github.com/jay1110/true-combat/blob/final/tools/launcher/start_game.py).

## Bot script gates and traffic behavior

`omnibot_tce_roamcamp`, `omnibot_tce_roamcamp_debug` and `omnibot_tce_objective_debug` are read by the supplied goal scripts through `GetCvar`, but are not native registered settings with guaranteed startup defaults or flag masks. An unset value is treated as zero by these script checks. `omnibot_tce_explore_debug` is also read by scripts, while its native registration belongs to Editor builds. These distinctions matter when reviewing the public/editor boundary.

The automatic crowd separation, following distance, yielding and doorway ordering introduced in 1.0.3/1.0.4 have **no new ET cvars**. Their distances and timing are implementation constants. `/bot ft_controls` is an Omni-bot command/configuration interface, not an ET cvar. Do not invent `g_*` traffic controls from these features.

Source: [TCE goal scripts](https://github.com/jay1110/true-combat/tree/final/vendor/omni-bot/tce2-scripts/goals). See also [bots](bots.md).

## Server platform advertisement

`g_oss` is an explicitly configured advertisement, not a check of deployed files. Its default `2047` is the sum of these eleven flags:

| Bit | Advertised platform |
| --- | --- |
| 1 | Windows x86 |
| 2 | Linux x86 |
| 4 | Linux x86-64 |
| 8 | macOS x86-64 |
| 16 | Android ARM64 |
| 32 | Linux ARMv7 |
| 64 | Linux ARM64 |
| 128 | macOS ARM64 |
| 256 | Windows x86-64 |
| 512 | Android x86 |
| 1024 | Android x86-64 |

There are no separate Android ARMv7 or WASM bits in this compatibility mask. `0` means unspecified; for example, a deployment containing only Windows/Linux x86/x86-64 modules can advertise `263`. See [server platforms](server-platforms.md) before advertising modules a server does not actually supply.

## Game-type and mapscript controls

For TCE2's supported modes, select `g_gametype` at process launch: `2` reinforced objective, `5` objective, `7` bodycount. Use `sv_official 0` for the custom/development configuration described by the map setup. A running TCE2 match is not a supported place to switch these modes. `g_mapScriptDirectory` chooses the external mapscript override directory; game-type-specific scripts/goals must actually exist for the selected map/mode.

`tce_dmspawn` and `tce_dmspawns` are **mapscript actions**, not cvars. The GT7 distant-opponent spawn choice does not expose a separate distance cvar. A newly generated `_gt7.script` is map content rather than a cvar registration.

## Lua/WolfAdmin configuration aliases

The bundled WolfAdmin compatibility layer has a Lua-side settings table. It reads the following legacy cvar aliases and can override them from its TOML configuration. These values are **Lua fallback defaults**, not native `trap_Cvar_Register` defaults. They do not make a disabled Lua build support WolfAdmin. Consult [WolfAdmin](wolfadmin.md) for installation, configuration and authentication.

| Alias | Lua fallback |
| --- | --- |
| `g_standalone` | `1` |
| `g_balancedteams` | `0` |
| `g_logChat` | `chat.log` |
| `g_logAdmin` | `admin.log` |
| `g_fileBanners` | `banners.toml` |
| `g_fileGreetings` | `greetings.toml` |
| `g_fileRules` | `rules.toml` |
| `g_fileSprees` | `sprees.toml` |
| `g_playerHistory` | `1` |
| `g_spreeMessages`, `g_spreeSounds` | `0` |
| `g_spreeRecords`, `g_botRecords` | `1` |
| `g_announceRevives` | `0` |
| `g_greetingArea` | `3` |
| `g_botGreetings` | `1` |
| `g_bannerInterval` | `120` seconds |
| `g_bannerRandomize` | `1` |
| `g_bannerArea` | `3` |
| `g_evenerMinDifference` | `2` |
| `g_evenerMaxDifference` | `5` |
| `g_evenerPlayerSelection` | `0` |
| `g_evenerInterval` | `30` seconds |
| `g_voteNextMapTimeout` | `0` |
| `g_restrictedVotes` | `""` |
| `g_renameLimit` | `80` |
| `g_debugWolfAdmin` | `0` |
| `omnibot_maxbots` | `10` |
| `db_type` | `sqlite3` |
| `db_file` | `wolfadmin.db` |
| `db_hostname` | `localhost` |
| `db_port` | `3306` |
| `db_database` | `wolfadmin` |
| `db_username`, `db_password` | `""` |

The current native registrations are instead `lua_modules`, `wolfadmin_password` and `g_wolfadminTrustGuid`, described above. Do not publish password values in server information, screenshots or shared configuration. `cl_guid` is user information, not independently authenticated identity; GUID trust is an explicit server opt-in, disabled by default.

Source: [WolfAdmin settings compatibility table](https://github.com/jay1110/true-combat/blob/final/src/lua/wolfadmin/util/settings.lua).

## Removed experiments

`cg_weaponLight` was a transient registered setting in 1.0.2, introduced in commit [3e9b943](https://github.com/jay1110/true-combat/commit/3e9b943) and removed in [0fa43eb](https://github.com/jay1110/true-combat/commit/0fa43eb). It is not a current supported option.

The withdrawn graphics-options experiment also created these 20 **unregistered UI state variables** in [074ee19](https://github.com/jay1110/true-combat/commit/074ee19), removed in [aae03d9](https://github.com/jay1110/true-combat/commit/aae03d9):

`ui_tceGfx_aa`, `ui_tceGfx_af`, `ui_tceGfx_canRestore`, `ui_tceGfx_compress`, `ui_tceGfx_filter`, `ui_tceGfx_hasAA`, `ui_tceGfx_texture`, `ui_tceGfx_old_aa`, `ui_tceGfx_old_af`, `ui_tceGfx_old_afdegree`, `ui_tceGfx_old_afmax`, `ui_tceGfx_old_bits`, `ui_tceGfx_old_compress`, `ui_tceGfx_old_curve`, `ui_tceGfx_old_detail`, `ui_tceGfx_old_fbo`, `ui_tceGfx_old_filter`, `ui_tceGfx_old_lod`, `ui_tceGfx_old_mips`, `ui_tceGfx_old_texture`.

They held staged values, capability indicators or snapshots of engine settings, without native table defaults. They are not current commands. Existing engine `r_*` settings used by that experiment were not removed from the engine.

## History, coverage and reproduction

The history appendix replays only commits reachable from the current `final` branch, beginning with SDK baseline `82d3fca`. The pre-versioned TC:E source stage is `f8c5d52`; Lua/Omni-bot/WolfAdmin integration before 1.0.0 is retained as pre-1.0.0 history. Version markers are `7707e8c` (1.0.0), `f7e1622` (1.0.1), `4770335` (1.0.2), `85705fa` (1.0.3) and `fc5ae89` (1.0.4). A registration event can be a default, flags, module-binding or compile-guard change; it does not necessarily introduce a new setting or gameplay feature.

The source scanner preserves direct registrations, table rows, paired-array registrations and formatted RGB channels. Duplicate bindings remain separate rows; unresolved dynamic registration calls cause failure. It does not claim that a lexical source inventory is an exhaustive runtime engine dump, or that every inherited setting is meaningful in every TC:E mode.

```text
python tools/docs/cvar_inventory.py --check
python tools/docs/render_cvars.py --check
python -m unittest discover -s tools/docs -p test_cvar_inventory.py
```

To regenerate the checked-in appendix after a deliberate registration change:

```text
python tools/docs/cvar_inventory.py --out Docs/cvars-registration.tsv --history Docs/cvars-history.tsv
python tools/docs/render_cvars.py
```

The registry appendix records source expressions verbatim. History normalizes equivalent numeric spellings and compares names case-insensitively. Legacy SDK defaults, UI mirrors and editor guards must not be flattened into a single supposed runtime default. Console commands such as `tce_minimize` and `toggleScoreboard`, mapscript actions and engine settings remain distinct from native mod registrations.
