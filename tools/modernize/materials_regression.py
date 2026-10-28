#!/usr/bin/env python3
"""Compile the actual pre-world material loader against bounded fake services."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    inputs = {}

    def read(relative):
        path = ROOT / relative
        inputs[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
        return path.read_text(encoding='utf-8-sig')

    module = read('src/cgame/cg_tce_materials.c')
    read('src/cgame/cg_tce_materials.h')
    main_source = read('src/cgame/cg_main.c')
    caller = function(main_source, 'CG_RegisterGraphics')
    assert caller.index('CG_TCEInstallMaterials(cgs.mapname);') < caller.index('trap_R_LoadWorldMap(cgs.mapname);')
    assert 'trap_R_RegisterShader' not in module and 'trap_R_RemapShader' not in module
    metadata = json.loads(read('src/assets/textures/tce2/details/SOURCE.json'))
    records = metadata['materials']
    assert len(records) == 5
    bodies = []
    for record in records:
        body = read('src/assets/' + record['path'])
        assert record['original_name'] in module and record['path'] in module
        assert len(body.encode()) == record['bytes'] <= 8192
        assert hashlib.sha256(body.encode()).hexdigest() == record['sha256']
        bodies.append(body)
    header = 'static const char *assetBodies[] = {\n' + ',\n'.join(json.dumps(b) for b in bodies) + '\n};\n'
    fixture = Path(__file__).with_name('materials_regression_fixture.c')
    inputs[str(fixture.relative_to(ROOT))] = hashlib.sha256(fixture.read_bytes()).hexdigest()
    unit = out / 'fixture.c'
    unit.write_text(fixture.read_text(encoding='utf-8').replace('/* ASSET_BODIES */', header), encoding='utf-8', newline='\n')
    (out / 'production.c').write_text(module, encoding='utf-8', newline='\n')
    if os.name == 'nt':
        env, cc = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44' if args.sanitize else None)
        exe = out / 'fixture.exe'
        cmd = [cc, '/nologo', '/TC', '/W4', '/O2' if args.optimized else '/Od',
               '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(ROOT), '/I' + str(ROOT / 'src/cgame'),
               str(unit), '/Fe:' + str(exe), '/Fo:' + str(out) + '\\']
        if args.sanitize:
            cmd[1:1] = ['/fsanitize=address', '/Z7']
    else:
        env = None
        cc = shutil.which('clang') or shutil.which('gcc')
        exe = out / 'fixture'
        cmd = [cc, '-std=c99', '-Wall', '-Wextra', '-O2' if args.optimized else '-O0',
               '-I' + str(ROOT), '-I' + str(ROOT / 'src/cgame'), str(unit), '-o', str(exe)]
        if args.sanitize:
            cmd[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    built = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    if built.returncode:
        print(built.stdout + built.stderr)
        return built.returncode
    run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=20)
    report = {'inputs': inputs, 'compiler': cc, 'command': cmd,
              'caller_sha256': hashlib.sha256(caller.encode()).hexdigest(),
              'generated_sha256': hashlib.sha256(unit.read_bytes()).hexdigest(),
              'stdout': run.stdout, 'stderr': run.stderr, 'exit_code': run.returncode,
              'limits': ['Complete actual loader source and shipped assets, bounded fake VFS/cvar/native dynamic shader services.',
                         'Caller order is an actual-source contract; no early RegisterShader or Remap call is linked.',
                         'Native renderer parsing, first BSP lightmap registration and pixel appearance require separate live evidence.',
                         'Custom-map/off original fallback assumes full renderer reload, as documented by the feature.']}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(run.stdout + run.stderr, end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
