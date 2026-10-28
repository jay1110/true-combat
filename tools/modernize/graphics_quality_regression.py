#!/usr/bin/env python3
"""Exercise actual quality-menu helpers against a latched engine cvar service."""
from __future__ import annotations

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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=ROOT / 'src/ui/ui_main.c')
    parser.add_argument('--console-source', type=Path, default=ROOT / 'src/ui/ui_atoms.c')
    parser.add_argument('--arch', choices=('x86', 'x64'), default='x86')
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = args.source.read_text(encoding='utf-8-sig')
    table = source.index('static const tceGraphicsSetting_t tceGraphicsSettings[]')
    start = source.rfind('typedef struct', 0, table)
    end = source.index('\nvoid UI_RunMenuScript(', table)
    actual = source[start:end]
    console = function(args.console_source.read_text(encoding='utf-8-sig'), 'UI_ConsoleCommand')
    menu_paths = ('src/assets/ui/options_quality.menu', 'src/assets/ui/options_system.menu',
                  'src/assets/ui/menus.txt')
    menus = {p: (ROOT / p).read_text(encoding='utf-8-sig') for p in menu_paths}
    quality_menu = menus[menu_paths[0]]
    assert 'onOpen { uiScript tceGraphicsGet }' in quality_menu
    escape = re.search(r'onESC\s*\{([^}]*)\}', quality_menu)[1]
    cancel = next(line for line in quality_menu.splitlines() if '"CANCEL"' in line)
    for script in (escape, cancel):
        assert 'close options_quality' in script and 'open options_system' in script
        assert 'uiScript' not in script and 'vid_restart' not in script
    assert not re.search(r'\bonClose\b', quality_menu)
    assert 'loadMenu { "ui/options_quality.menu" }' in menus[menu_paths[2]]
    assert 'open options_quality' in menus[menu_paths[1]]
    dispatch = re.search(r'\n\s*if \(!Q_stricmp\(name, "tceGraphicsGet"\)\).*?'
                         r'\n\s*if \(!Q_stricmp\(name, "tceGraphicsRestore"\)\)[^\n]*', source, re.S)
    if not dispatch:
        raise ValueError('Actual graphics menu dispatch prefix missing')
    fixture = Path(__file__).with_name('graphics_quality_regression_fixture.c')
    generated = fixture.read_text(encoding='utf-8').replace('/* PRODUCTION_HELPERS */', actual)
    generated = generated.replace('/* PRODUCTION_DISPATCH_PREFIX */', dispatch.group())
    generated = generated.replace('/* PRODUCTION_CONSOLE_COMMAND */', console)
    unit = out / 'graphics_quality_regression.c'
    unit.write_text(generated, encoding='utf-8', newline='\n')
    if os.name == 'nt':
        env, compiler = msvc_environment(out, args.arch, '14.44' if args.sanitize else None)
        exe = out / 'graphics_quality_regression.exe'
        command = [compiler, '/nologo', '/TC', '/W4', '/O2' if args.optimized else '/Od',
                   '/D_CRT_SECURE_NO_WARNINGS', str(unit), '/Fe:' + str(exe), '/Fo:' + str(out) + '\\']
        if args.sanitize:
            command[1:1] = ['/fsanitize=address', '/Z7']
    else:
        env = None
        compiler = shutil.which('clang') or shutil.which('gcc')
        exe = out / 'graphics_quality_regression'
        command = [compiler, '-std=c99', '-Wall', '-Wextra', '-O2' if args.optimized else '-O0',
                   str(unit), '-o', str(exe)]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    if built.returncode:
        print(built.stdout + built.stderr)
        return built.returncode
    run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    report = {'source': str(args.source.resolve()),
              'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
              'actual_helpers_sha256': hashlib.sha256(actual.encode()).hexdigest(),
              'actual_dispatch_sha256': hashlib.sha256(dispatch.group().encode()).hexdigest(),
              'console_source': str(args.console_source.resolve()),
              'console_source_sha256': hashlib.sha256(args.console_source.read_bytes()).hexdigest(),
              'actual_console_sha256': hashlib.sha256(console.encode()).hexdigest(),
              'menu_inputs': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in menu_paths},
              'fixture_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
              'generated_sha256': hashlib.sha256(unit.read_bytes()).hexdigest(),
              'compiler': compiler, 'command': command, 'stdout': run.stdout,
              'stderr': run.stderr, 'exit_code': run.returncode,
              'limits': ['Actual complete graphics helper block and actual five-command dispatch prefix.',
                         'Actual complete normal-Game UI_ConsoleCommand; unrelated commands/engine menu services are stubbed.',
                         'Mock engine cvars preserve active/latched strings and engine state across UI VM recreation.',
                         'No GPU, driver sample-count support, engine crash recovery or visual menu proof.']}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(run.stdout, end='')
    print(run.stderr, end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
