"""Actual voluntary-objective drop, existing item lifecycle and Omni-bot query."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]

def unit(source, name):
    start = re.search(r'(?m)^(?:static )?(?:void|qboolean|int|gentity_t\s*\*)\s*'+re.escape(name)+r'\(', source)
    if not start: raise ValueError('Definition missing: '+name)
    return function(source[start.start():], name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    compiler_headers = {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
                        for p in ('src/game/bg_public.h', 'src/game/g_local.h')}
    inputs = {}
    def read(relative):
        path = args.source_root / relative
        if not path.is_file(): path = args.source_root / Path(relative).name
        if not path.is_file(): path = ROOT / relative
        inputs[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        return path.read_text(encoding='utf-8-sig')
    items = read('src/game/g_items.c'); team = read('src/game/g_team.c')
    commands = function(read('src/game/g_cmds.c'), 'ClientCommand')
    init = function(read('src/cgame/cg_consolecmds.c'), 'CG_InitConsoleCommands')
    route = 'else if (Q_stricmp(cmd, "dropobj") == 0)' in commands
    if route:
        branch = commands[commands.index('else if (Q_stricmp(cmd, "dropobj") == 0)'):]
        route = 'G_TCEManualDropObjective(ent);' in branch[:branch.index('\n\telse if')]
        route = route and commands.index('if (level.intermissiontime)') < commands.index('"dropobj"')
    units = [unit(items, n) for n in ('RespawnItem','LaunchItem','Drop_Item','G_TCELaunchObjective',
             'G_TCEReleaseObjectives','G_TCEManualDropBomb')]
    units += [unit(team, n) for n in ('Team_ResetFlag','Team_DroppedFlagThink','Team_TouchOurFlag',
              'Team_TouchEnemyFlag','Pickup_Team')]
    if 'qboolean G_TCEManualDropObjective(' in items:
        units.append(unit(items, 'G_TCEManualDropObjective'))
    else:
        units.append('qboolean G_TCEManualDropObjective(gentity_t *ent) { (void)ent;return qfalse; }')
    types = read('vendor/omni-bot/source/Omnibot/Common/Omni-Bot_Types.h')
    enum = re.search(r'typedef enum eFlagState\s*\{.*?\}\s*FlagState;', types, re.S)[0]
    template = Path(__file__).with_name('drop_objective_fixture.c')
    generated = template.read_text(encoding='utf-8').replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(units))
    generated = generated.replace('/* ACTUAL_FLAG_ENUM */', enum)
    generated = '#define DROP_ROUTE %d\n#define DROP_REGISTERED %d\n' % (route, 'trap_AddCommand("dropobj");' in init) + generated
    source = out / 'fixture.c'; source.write_text(generated, encoding='utf-8', newline='\n')
    if os.name == 'nt':
        env, cc = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44')
        exe = out / 'fixture.exe'
        command = [cc, '/nologo', '/TC', '/W3', '/O2' if args.optimized else '/Od', '/DWIN32',
                   '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(ROOT), str(source),
                   str(ROOT/'src/game/q_shared.c'), str(ROOT/'src/game/q_math.c'), '/Fe:'+str(exe)]
        if args.sanitize: command += ['/fsanitize=address', '/Zi', '/link', '/DEBUG']
    else:
        env = None; cc = shutil.which('clang') or shutil.which('gcc')
        if not cc: raise RuntimeError('No native compiler')
        exe = out / 'fixture'
        command = [cc, '-std=c99', '-O2' if args.optimized else '-O0', '-I'+str(ROOT), str(source),
                   str(ROOT/'src/game/q_shared.c'), str(ROOT/'src/game/q_math.c'), '-lm', '-o', str(exe)]
        if args.sanitize: command += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
    build = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(build.stdout+build.stderr, encoding='utf-8')
    report = dict(inputs=inputs, command=command, compile_exit=build.returncode, compiler_header_hashes=compiler_headers,
                  template_sha256=hashlib.sha256(template.read_bytes()).hexdigest(),
                  generated_sha256=hashlib.sha256(generated.encode()).hexdigest(),
                  limits=['Actual drop/item/return/pickup/Omni-bot query functions; controlled collision/entity/script services.',
                          'Client registration and guarded server dispatch are source contracts; no engine console forwarding or live map claim.',
                          'Pickup controls call actual Pickup_Team and perform the caller entity cleanup explicitly; no complete Touch_Item emulation.'])
    if build.returncode: print(build.stdout+build.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=20)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout+run.stderr, end='')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    return int(bool(build.returncode or report.get('run_exit')))


if __name__ == '__main__': raise SystemExit(main())
