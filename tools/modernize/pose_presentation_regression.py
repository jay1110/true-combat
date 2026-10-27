#!/usr/bin/env python3
"""Tiny shared-pose/ABI fixture; captures are optional explicit oracle inputs.

No game/VM/process access. The default input set is a compact tracked extract
of actual ladder/prone captures; expected axes are the NEW canonical policy,
not a claim that legacy observer-local axes have been recovered.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]
INPUTS = ROOT / 'tools/modernize/pose_presentation_inputs.json'


def angle(value):
    return (value + 180) % 360 - 180


def basis(a):
    p, y, r = map(math.radians, a)
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    return [[cp * cy, cp * sy, -sp],
            [sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp],
            [cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp]]


def oracle(row):
    aim = list(map(angle, row['aim']))
    body = [0, angle(aim[1] + row['movementYaw']), 0]
    torso = [-3 if row['mode'] == 3 else aim[0] * .75, aim[1], 0]
    initial = basis(body)
    # Independent equivalent vector formula for the travel tilt, before lean.
    body[2] -= .05 * sum(v * a for v, a in zip(row['velocity'], initial[1]))
    body[0] += .05 * sum(v * a for v, a in zip(row['velocity'], initial[0]))
    lean = row['lean'] * (1.65 if row['lean'] < 0 else 1) if row['leanMode'] else 0
    torso[2] += lean
    head = [aim[0] + row['freelook'][0] * .7, aim[1] + row['freelook'][1] * .7, aim[2] + lean]
    result = [basis(body), basis([angle(a - b) for a, b in zip(torso, body)]),
              basis([angle(a - b) for a, b in zip(head, torso)])]
    result[0] = [[v * row['scale'] for v in axis] for axis in result[0]]
    return result


def capture_inputs(folder):
    records = []
    ladder = folder / 'pose-readonly-ladder-coherent.json'
    data = json.loads(ladder.read_text())
    for phase in data['phases']:
        for i, row in enumerate(phase['consistent_samples']):
            ps = row['predicted_ps']
            if not ps['pm_flags'] & 4:
                continue
            records.append(dict(capture=ladder.name, phase=phase['label'], index=i,
                                server_command=row['server_ps']['commandTime'], predicted_command=ps['commandTime'],
                                mode=1, aim=ps['viewangles'], movementYaw=0,
                                movementYaw_source='explicit zero movement-yaw control; PS movementDir was not captured',
                                velocity=ps['velocity'], lean=ps['leanf'], freelook=[0, 0],
                                freelook_source='explicit zero freelook policy input', scale=1.25, leanMode=1,
                                rateQ8=round(row['lerp']['legs']['animSpeedScale'] * 256)))
    prone = folder / 'pose-control-prone-aim.jsonl'
    grouped = {}
    for line in prone.read_text().splitlines():
        row = json.loads(line)
        if 'id' in row:
            grouped.setdefault(row['id'], {})[row['part']] = row
    for ident, parts in grouped.items():
        ps = parts['local_server_ps']['data']
        if not ps[2] & 0x80000:
            raise ValueError('Source prone control is not actually prone')
        a2 = parts['angles2']['data']
        scale = math.sqrt(sum(v * v for v in parts['body_axis']['data'][0]))
        records.append(dict(capture=prone.name, sample=ident, server_command=ps[0],
                            mode=3, aim=parts['server_angles']['data'], movementYaw=a2[1],
                            velocity=parts['velocity']['data'], lean=a2[2],
                            freelook=parts['freelook']['data'][:2], scale=round(scale, 6), leanMode=1,
                            rateQ8=256))
    return dict(schema=1, policy='new snapshot-only pose1 axes; no legacy renderer-phase parity claim',
                capture_hashes={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (ladder, prone)},
                cases=records)


def cf(value):
    value = format(float(value), '.9g')
    if '.' not in value and 'e' not in value:
        value += '.0'
    return value + 'f'


def cv(values):
    return '{' + ','.join(cv(v) if isinstance(v, list) else cf(v) for v in values) + '}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--arch', choices=('x86', 'x64'), default='x86')
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler')
    parser.add_argument('--extract-captures', type=Path, help='Explicit offline source directory; rewrites compact fixture input JSON')
    args = parser.parse_args()
    if args.extract_captures:
        INPUTS.write_text(json.dumps(capture_inputs(args.extract_captures), indent=2) + '\n', encoding='utf-8', newline='\n')
    inputs = json.loads(INPUTS.read_text())
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    cases = []
    for row in inputs['cases']:
        axes = oracle(row)
        cases.append('{' + '{' + ','.join([str(row['mode']), cv(row['aim']), cf(row['movementYaw']), cv(row['velocity']), cf(row['lean']), cv(row['freelook']), cf(row['scale']), str(row['leanMode'])]) + '},' + ','.join(cv(a) for a in axes) + ',' + str(row['rateQ8']) + 'u}')
    generated_inputs = 'static const presentFixture_t presentFixtures[] = {\n' + ',\n'.join(cases) + '\n};'
    source = (ROOT / 'src/game/bg_misc.c').read_text()
    macro = source[source.index('#define SETUP_MOUNTEDGUN_STATUS'):source.index('/*', source.index('#define SETUP_MOUNTEDGUN_STATUS'))]
    functions = '\n'.join(function(source, name) for name in ('BG_TCEPlayerStateFields', 'BG_SnapshotSequenceAdd', 'BG_SnapshotVector', 'BG_PlayerStateToEntityState', 'BG_PlayerStateToEntityStateExtraPolate'))
    fixture = (ROOT / 'tools/modernize/pose_presentation_test.c').read_text().replace('/* ACTUAL_PLAYER_CONVERTERS */', macro + functions).replace('/* CAPTURED_INPUTS */', generated_inputs)
    test = out / 'presentation_generated.c'; test.write_text(fixture, encoding='utf-8', newline='\n')
    sources = [test, ROOT / 'src/game/bg_tce_pose_presentation.c', ROOT / 'src/game/bg_tce_pose_clock.c']
    if os.name == 'nt':
        env, compiler = msvc_environment(out, args.arch, '14.44' if args.sanitize else '14.39')
        exe = out / 'presentation_test.exe'
        command = [compiler, '/nologo', '/TC', '/W4', '/WX', '/fp:precise', '/O2' if args.optimized else '/Od', '/DWIN32', '/DGAMEDLL', '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(ROOT / 'src/game'), '/I' + str(ROOT)]
        if args.sanitize:
            command += ['/fsanitize=address', '/Zi']
        command += list(map(str, sources)) + ['/Fo:' + str(out) + os.sep, '/Fe:' + str(exe)]
    else:
        env = os.environ.copy(); exe = out / 'presentation_test'
        command = [args.compiler or 'cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off', '-O2' if args.optimized else '-O0', '-DGAMEDLL', '-I' + str(ROOT / 'src/game'), '-I' + str(ROOT)]
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        command += list(map(str, sources)) + ['-lm', '-o', str(exe)]
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    names = ['src/game/bg_tce_pose_clock.c', 'src/game/bg_tce_pose_clock.h', 'src/game/bg_tce_pose_presentation.c', 'src/game/bg_tce_pose_presentation.h', 'src/game/bg_misc.c', 'src/game/q_shared.h', 'tools/modernize/pose_presentation_test.c', 'tools/modernize/pose_presentation_inputs.json', 'tools/modernize/pose_presentation_regression.py']
    report = dict(command=command, build_exit=built.returncode, hashes={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in names}, limits=['Unactivated pure policy, not recovered legacy observer state', 'Actual converters prove words survive current code, including invisible transitions; no carrier is wired', 'No game/VM/network/process action'])
    if not built.returncode:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout + run.stderr, end='')
    else:
        print(built.stdout + built.stderr)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return report.get('run_exit', built.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
