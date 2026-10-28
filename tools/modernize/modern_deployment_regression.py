"""Exercise actual deployment layout and native mouse callbacks with controlled services.

The fixture compiles the production layout, native panel dispatcher and deployment
handlers. Engine rendering, network transport and loadout eligibility are controlled;
this is a behavioral check, not a claim of live mouse or visual parity.
"""
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


def definition(source: str, name: str) -> str:
    # Some native getters first occur in multi-line `if (getter()...) {`
    # expressions. Restrict the starting line to an actual C return type.
    match = re.search(r'^(?:static\s+)?[A-Za-z_][\w\s*]*\b' + re.escape(name)
                      + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Function definition not found: ' + name)
    return function(source[match.start():], name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', choices=('auto', 'clang', 'gcc'), default='auto')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    production = ROOT / 'src/cgame/cg_limbopanel.c'
    modern = ROOT / 'src/cgame/cg_modern_limbopanel.inc'
    shared = ROOT / 'src/ui/ui_shared.c'
    fixture = Path(__file__).with_name('modern_deployment_fixture.c')
    source = production.read_text(encoding='utf-8')
    layout = definition(source, 'TCE_LimboLayout')
    # Bind to the real object names and actual initializer, rather than maintain a
    # second hand-written layout whose divergence could hide a broken hit target.
    objects = sorted(set(re.findall(r'\b(\w+)\s*=\s*value;', layout)))
    fonts = sorted(set(re.findall(r'&(\w*Font\w*)\b', layout)))
    pool = re.search(r'panel_button_t\s*\*limboPanelButtons\[\]\s*=\s*\{.*?\n\};', source, re.S)
    if not pool:
        raise ValueError('Actual native deployment button pool not found')
    declarations = '\n'.join('panel_button_t ' + name + ';' for name in objects)
    declarations += '\n' + '\n'.join('panel_button_text_t ' + name + ';' for name in fonts)
    handlers = ('CG_LimboPanel_NameEditFinish', 'CG_LimboPanel_CancelButton_KeyDown',
                'CG_LimboPanel_SendSetupMsg', 'CG_LimboPanel_OkButton_KeyDown',
                'CG_LimboPanel_TeamButton_KeyDown', 'CG_LimboPanel_ClassButton_KeyDown',
                'CG_LimboPanel_WeaponLights_KeyDown', 'CG_LimboPanel_WeaponPanel_KeyDown',
                'CG_LimboPanel_WeaponPanel_KeyUp', 'CG_LimboPanel_GetTeam',
                'CG_LimboPanel_GetClass', 'CG_LimboPanel_GetSelectedWeaponForSlot',
                'CG_LimboPanel_GetSelectedWeaponNum', 'CG_LimboPanel_SetSelectedWeaponNumForSlot',
                'CG_LimboPanel_SetSelectedWeaponNum')
    panel = ('BG_RectContainsPoint', 'BG_CursorInRect', 'BG_PanelButtons_GetFocusButton',
             'BG_PanelButtons_SetFocusButton', 'BG_PanelButtonsKeyEvent',
             'BG_PanelButtonsGetHighlightButton', 'BG_PanelButtonsSetup')
    modern_source = modern.read_text(encoding='utf-8')
    text = fixture.read_text(encoding='utf-8')
    text = text.replace('/* ACTUAL_DECLARATIONS */', declarations + '\n' + pool.group())
    text = text.replace('/* ACTUAL_PANEL */', '\n'.join(definition(shared.read_text(encoding='utf-8'), name) for name in panel))
    text = text.replace('/* ACTUAL_HANDLERS */', '\n'.join(definition(source, name) for name in handlers))
    text = text.replace('/* ACTUAL_LAYOUT */', layout + '\n' + modern_source)
    text = text.replace('/* ACTUAL_INIT */', definition(source, 'CG_LimboPanel_Init'))
    text = text.replace('/* ACTUAL_KEY_FLOW */', definition(source, 'TCE_LimboKeyContents') +
                        '\n' + definition(source, 'CG_LimboPanel_KeyHandling'))
    unit = out / 'modern_deployment.c'
    unit.write_text(text, encoding='utf-8', newline='\n')
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44')
        exe = out / 'modern_deployment.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS',
                   '/I' + str(ROOT), '/O2' if args.optimized else '/Od', str(unit),
                   '/Fo:' + str(out) + os.sep, '/Fe:' + str(exe)]
        if args.sanitize:
            command[1:1] = ['/fsanitize=address', '/Zi', '/Fd:' + str(out / 'fixture.pdb')]
            command += ['/link', '/DEBUG']
    else:
        env = os.environ.copy()
        compiler = (shutil.which('clang') or shutil.which('gcc')) if args.compiler == 'auto' else shutil.which(args.compiler)
        if not compiler:
            raise RuntimeError('Native compiler unavailable')
        exe = out / 'modern_deployment'
        command = [compiler, '-std=c99', '-Wall', '-Wextra', '-I', str(ROOT),
                   '-O2' if args.optimized else '-O0', str(unit), '-lm', '-o', str(exe)]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
    result = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    headers = [ROOT / path for path in ('src/ui/ui_shared.h', 'src/game/q_shared.h',
               'src/game/bg_public.h', 'src/cgame/cg_local.h', 'src/cgame/tce_weapon_media.h',
               'src/game/tce_bg.h', 'src/game/tce_numeric.h', 'compat/legacy/game/tce_numeric.h',
               'src/ui/tce_modern_ui.h')]
    report = {'build_exit': result.returncode, 'command': command,
              'inputs': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in (production, modern, shared, fixture, Path(__file__), *headers)},
              'limits': ['Actual native button pool, layout, mouse dispatcher and deployment callbacks; controlled eligibility and engine services.',
                         'Does not prove live mouse delivery, pixels, server-side selection acceptance or network behavior.']}
    if not result.returncode:
        result = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
        report.update(run_exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
