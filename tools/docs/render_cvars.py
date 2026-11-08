#!/usr/bin/env python3
"""Render the source-derived cvar reference from the checked-in TSV inputs."""
from __future__ import annotations
import argparse
import csv
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / 'Docs'

INTRO = r'''# Console variables

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
'''

TAIL = r'''
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
'''

def read(name):
    with (DOCS / name).open(encoding='utf-8', newline='') as stream:
        return list(csv.DictReader(stream, delimiter='\t'))

def cell(value):
    return str(value).replace('|', '\\|').replace('\n', ' ')

def render():
    rows = read('cvars-registration.tsv')
    descriptions = read('cvars-descriptions.tsv')
    events = read('cvars-history.tsv')
    index = defaultdict(list)
    for row in rows: index[row['name'].casefold()].append(row)
    first = {}
    for event in events:
        first.setdefault(event['name'].casefold(), event)
    out = [INTRO]
    group = ''
    for note in descriptions:
        if group != note['group']:
            group = note['group']
            out.append('\n### ' + group + '\n')
        bindings = index[note['name'].casefold()]
        variants = defaultdict(set)
        for row in bindings:
            value = row['default']
            if value == '': value = '""'
            if note['name'] == 'tce_version': value = '1.0.4 (TCE_VERSION)'
            variants[(value, row['flags'], row['scope'])].add(row['module'])
        out += ['\n#### `' + note['name'] + '`\n', note['purpose'] + '\n',
                '**Values:** ' + note['values'] + '.\n',
                '| Module | Default | Flags | Build scope |\n| --- | --- | --- | --- |']
        for (value, flags, scope), modules in sorted(variants.items()):
            out.append('| ' + ' | '.join(cell(x) for x in
                       [', '.join(sorted(modules)), '`' + value + '`',
                        '`' + flags.replace('CVAR_', '') + '`', scope]) + ' |')
        origins = {r['origin'] for r in bindings}
        if 'ET SDK inherited' in origins:
            origin = 'Inherited ET SDK binding'
        elif 'pre-versioned TC:E registration' in origins:
            origin = 'Present in the pre-versioned TC:E source stage'
        else:
            event = first.get(note['name'].casefold())
            origin = ('First recorded registration: ' + event['version'] +
                      ' ([`' + event['commit'] + '`](https://github.com/jay1110/true-combat/commit/' + event['commit'] + '))') if event else 'Source registration; see appendix'
        sources = sorted({(r['source'], r['line']) for r in bindings})
        links = ', '.join(f'[{path}:{line}](https://github.com/jay1110/true-combat/blob/final/{path}#L{line})' for path, line in sources)
        out += ['\n**History:** ' + origin + '. **Registration sources:** ' + links + '.\n']
    return '\n'.join(out) + TAIL

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    expected = render()
    path = DOCS / 'cvars.md'
    if args.check:
        if not path.exists() or path.read_text(encoding='utf-8') != expected:
            print('Docs/cvars.md is stale; run python tools/docs/render_cvars.py')
            return 1
        print('Cvar reference matches its source inventory and descriptions.')
    else:
        path.write_text(expected, encoding='utf-8', newline='\n')
        print(f'Wrote {path.relative_to(ROOT)}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
