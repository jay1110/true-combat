"""Exercise actual stats command dispatch, reply parsing and window drawing."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def unit(source, name):
    # Start at the definition: an earlier `if (CG_addString(...)) {` is a call.
    match = re.search(r'^(?:static )?(?:void|int|qboolean|cg_window_t\s*\*)\s*' +
                      re.escape(name) + r'\(', source, re.M)
    if not match:
        raise ValueError('Definition not found: ' + name)
    return function(source[match.start():], name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--arch', choices=('x86', 'x64'), default='x86')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--editor', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    paths = ('src/cgame/cg_consolecmds.c', 'src/cgame/cg_servercmds.c',
             'src/cgame/cg_window.c', 'src/game/bg_misc.c', 'src/cgame/cg_tce_weapon_stats.c',
             'src/game/tce_weapon_stats.h', 'src/cgame/cg_main.c', 'src/cgame/cg_tce_weapon_stats.h',
             'src/cgame/cg_draw.c')
    sources = {}
    for relative in paths:
        path = args.source_root / relative
        if not path.exists():
            path = args.source_root / Path(relative).name
        if not path.exists():
            path = ROOT / relative
        sources[relative] = path.read_text(encoding='utf-8-sig')
    console = sources[paths[0]]
    assert 'CG_TCEWeaponStatsReset();' in function(sources['src/cgame/cg_main.c'], 'CG_Init')
    table = re.search(r'static consoleCommand_t commands\[\] =\s*\{.*?\n\};', console, re.S)[0]
    handlers = set(re.findall(r'\{\s*"[^"\n]+",\s*(\w+)\s*\}', table))
    actual = {'CG_wStatsDown_f', 'CG_wStatsUp_f', 'CG_TCEIgnoredConsoleCommand'}
    stubs = '\n'.join('void ' + name + '(void) { ++unrelatedCalls; }'
                      for name in sorted(handlers - actual))
    units = [unit(console, name) for name in sorted(actual)]
    units += [table, unit(console, 'CG_ConsoleCommand'), unit(console, 'CG_InitConsoleCommands')]
    units += [unit(sources[paths[1]], name)
              for name in ('CG_wstatsParse_cmd', 'CG_parseWeaponStats_cmd', 'CG_MapRestart')]
    window_names = (
        'CG_StatsCell', 'CG_StatsTextMetrics', 'CG_StatsColumns', 'CG_DrawStatsRow',
        'CG_StatsNormalize', 'CG_DrawStatsWindow',
        'CG_createStatsWindow', 'CG_windowInit', 'CG_windowReset', 'CG_windowAlloc',
        'CG_windowFree', 'CG_windowCleanup', 'CG_windowDraw', 'CG_windowNormalizeOnText',
        'CG_printWindow', 'CG_initStrings', 'CG_addString', 'CG_removeStrings')
    optional_helpers = {'CG_StatsTextMetrics', 'CG_StatsNormalize', 'CG_DrawStatsWindow'}
    for name in window_names:
        # Only these new helpers are optional when testing the archived baseline.
        # All original tested bodies remain required and fail extraction if gone.
        if name in optional_helpers and not re.search(r'^static [^\n;]*\b'+name+r'\(', sources[paths[2]], re.M):
            continue
        units.append(unit(sources[paths[2]], name))
    text_units = [unit(sources['src/cgame/cg_draw.c'], name) for name in
                  ('CG_Text_Width_Ext', 'CG_Text_Height_Ext', 'CG_Text_PaintChar_Ext', 'CG_Text_Paint_Ext')]
    text_units[-1] = text_units[-1].replace('void CG_Text_Paint_Ext(', 'static void CG_Actual_Text_Paint_Ext(', 1)
    units = text_units + units
    misc = sources[paths[3]]
    units += [unit(misc, 'BG_cleanName')]
    for declaration in ('const weap_ws_t aWeaponInfo[WS_MAX]',
                        'const char *rankNames_Axis[NUM_EXPERIENCE_LEVELS]',
                        'const char *rankNames_Allies[NUM_EXPERIENCE_LEVELS]',
                        'const char *skillNames[SK_NUM_SKILLS]',
                        'const int skillLevels[NUM_SKILL_LEVELS]'):
        units.append(re.search(re.escape(declaration) + r'\s*=\s*\{.*?\n\};', misc, re.S)[0])
    generated = (ROOT / 'tools/modernize/stats_popup_regression_fixture.c').read_text(encoding='utf-8')
    generated = generated.replace('/* UNRELATED_COMMAND_STUBS */', stubs)
    generated = generated.replace('/* PRODUCTION_FUNCTIONS */', '\n\n'.join(units))
    path = out / 'fixture.c'
    path.write_text(generated, encoding='utf-8', newline='\n')
    stats_path = out / 'cg_tce_weapon_stats.c'
    stats_path.write_text(sources[paths[4]], encoding='utf-8', newline='\n')
    env, compiler = msvc_environment(out, args.arch, '14.44' if args.sanitize else None)
    exe = out / 'fixture.exe'
    command = [compiler, '/nologo', '/TC', '/Od' if args.configuration == 'Debug' else '/O2', '/Gy', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS',
               '/I' + str(ROOT), str(path), str(ROOT / 'src/game/q_shared.c'),
               '/I' + str(ROOT / 'src/cgame'), str(stats_path),
               str(ROOT / 'src/game/q_math.c'), '/Fe:' + str(exe)]
    if args.editor:
        command.insert(1, '/DFEATURE_NAVSTUDIO')
    if args.sanitize:
        command[1:1] = ['/fsanitize=address', '/Z7']
    command += ['/link', '/OPT:REF']
    if args.sanitize:
        command.append('/DEBUG')
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    report = {'arch': args.arch, 'asan': args.sanitize, 'editor': args.editor, 'configuration': args.configuration,
              'command': command, 'build_returncode': built.returncode,
              'source_hashes': {name: hashlib.sha256(text.encode()).hexdigest()
                                for name, text in sources.items()},
              'generated_sha256': hashlib.sha256(generated.encode()).hexdigest(),
              'font_metrics_sha256': hashlib.sha256((ROOT/'tools/modernize/stats_courbd30_metrics.h').read_bytes()).hexdigest(),
              'limits': ['Actual command table/dispatch, request/debounce, response parser and window lifecycle/draw bodies',
                         'Engine transport and rasterizer callbacks are instrumented services, no live server/game',
                         'Unrelated command callbacks are inert stubs; their table entries and feature guards are unchanged']}
    if built.returncode:
        print(built.stdout + built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=20)
        report.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout + run.stderr, end='')
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return int(bool(built.returncode or report.get('returncode')))


if __name__ == '__main__':
    raise SystemExit(main())
