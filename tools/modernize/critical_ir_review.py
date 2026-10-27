"""Bounded arithmetic code-generation comparison and exact critical review gate.

The local decision policy is an explicit input, never generated approval. This
compares scalar slices/helper bodies, not whole-game opcode or runtime parity.
"""
from __future__ import annotations

import argparse
import collections
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function
from source_review import compare, review_policy
from validation_bundle import digest, files_snapshot
from risk_scan import EXCLUDED, scan

ROOT = Path(__file__).resolve().parents[2]
PAIRS = ('place', 'round', 'store', 'ratio', 'divide', 'highest', 'weather',
         'weather_fast', 'backlerp', 'speed', 'radar', 'ui')
SUPPLEMENTAL = ('src/game/tce_damage_controller.inc',)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def indexed(inventory, relocate=False):
    result = {}
    for file in inventory['files']:
        path = file['path']
        if relocate and path.startswith('compat/legacy/'):
            path = 'src/' + path[len('compat/legacy/'):]
        counts = collections.Counter()
        for entry in file['functions']:
            key = (path, entry['name'], counts[entry['name']])
            counts[entry['name']] += 1
            result[key] = dict(entry, path=file['path'], file_sha256=file['sha256'])
    return result


def validate_decisions(root, before, current, reviews, policy, baseline_sha, reviews_sha):
    errors = []
    review = compare(root, before, baseline_sha, reviews)
    if not review['passed']:
        errors.append('Baseline source review did not pass')
    if (policy.get('schema') != 1 or policy.get('review_status') != 'APPROVED'
            or policy.get('baseline_inventory_sha256') != baseline_sha
            or policy.get('source_reviews_sha256') != reviews_sha):
        errors.append('Unapproved policy or exact baseline/source-review binding mismatch')
    _, _, proof_errors = review_policy(root, {'schema': 1, 'baseline_inventory_sha256': baseline_sha,
                                             'proofs': policy.get('proofs', {})}, baseline_sha)
    errors += proof_errors
    proofs = policy.get('proofs', {})
    used_pairs, fixtures = set(), set()

    def check_decision(row):
        valid = isinstance(row.get('ir_rationale'), str) and bool(row['ir_rationale'].strip())
        ids, jobs = row.get('proof_ids'), row.get('fixture_jobs')
        valid = (valid and isinstance(ids, list) and bool(ids) and all(i in proofs for i in ids)
                 and isinstance(jobs, list) and bool(jobs) and all(isinstance(j, str) for j in jobs))
        mode, pairs = row.get('ir_mode'), row.get('ir_pairs', [])
        valid = valid and isinstance(pairs, list) and all(p in PAIRS for p in pairs)
        valid = valid and (mode == 'compare' and bool(pairs) or mode == 'not-applicable' and not pairs)
        if not valid:
            errors.append('Missing explicit IR/NA, proof or fresh fixture decision: ' + str(row.get('name', row.get('path'))))
        else:
            used_pairs.update(pairs)
            fixtures.update(jobs)

    changes = {(r['path'], r['name'], r['ordinal']): r for r in review['changes']}
    decisions = {}
    for row in policy.get('functions', []):
        key = (row.get('path'), row.get('name'), row.get('ordinal'))
        if key in decisions:
            errors.append('Duplicate changed-function decision: ' + str(key))
        decisions[key] = row
        change = changes.get(key)
        if not change or any(row.get(field) != change[field] for field in
                ('current_path', 'baseline_normalized_sha256', 'current_normalized_sha256')):
            errors.append('Stale or unmatched changed-function decision: ' + str(key))
        check_decision(row)
    if set(changes) != set(decisions):
        errors.append('Missing changed-function decisions')
    old, actual = indexed(before), indexed(current, True)
    new = {key: value for key, value in actual.items() if key not in old}
    covered = {}
    for group in policy.get('new_files', []):
        check_decision(group)
        for row in group.get('functions', []):
            path = group.get('path', '')
            canonical = 'src/' + path[len('compat/legacy/'):] if path.startswith('compat/legacy/') else path
            key = (canonical, row.get('name'), row.get('ordinal'))
            entry = new.get(key)
            if key in covered:
                errors.append('Duplicate new-function decision: ' + str(key))
            covered[key] = row
            if (not entry or entry['path'] != path or entry['file_sha256'] != group.get('file_sha256')
                    or entry['normalized_sha256'] != row.get('normalized_sha256')):
                errors.append('Stale or unmatched new-function decision: ' + str(key))
    if set(new) != set(covered):
        errors.append('Missing new-function decisions')
    supplemental = {group.get('path'): group for group in policy.get('supplemental_files', [])}
    if len(supplemental) != len(policy.get('supplemental_files', [])):
        errors.append('Duplicate supplemental critical family')
    if set(supplemental) != set(SUPPLEMENTAL):
        errors.append('Missing/unmatched supplemental critical family')
    for name in SUPPLEMENTAL:
        group = supplemental.get(name)
        if not group:
            continue
        check_decision(group)
        actual_file = scan(root / name, root)
        counts, actual_functions = collections.Counter(), []
        for entry in actual_file['functions']:
            actual_functions.append({'name': entry['name'], 'ordinal': counts[entry['name']],
                                     'normalized_sha256': entry['normalized_sha256']})
            counts[entry['name']] += 1
        if group.get('file_sha256') != actual_file['sha256'] or group.get('functions') != actual_functions:
            errors.append('Stale supplemental critical family: ' + name)
    if used_pairs != set(PAIRS):
        errors.append('Missing required arithmetic comparison pair')
    live = {p.relative_to(root).as_posix() for base in ('src', 'compat', 'platform')
            for p in (root / base).rglob('*') if p.suffix in ('.c', '.cpp', '.h', '.hpp', '.inl')
            and not any(part in EXCLUDED for part in p.relative_to(root / base).parts)}
    if live != {file['path'] for file in current['files']}:
        errors.append('Current risk inventory omits/adds source files')
    for file in current['files']:
        if (not (root / file['path']).is_file() or sha(root / file['path']) != file['sha256']
                or scan(root / file['path'], root)['functions'] != file['functions']):
            errors.append('Stale current risk inventory: ' + file['path'])
    return {'passed': not errors, 'errors': errors, 'changed': len(changes), 'new': len(new),
            'new_files': len(policy.get('new_files', [])), 'supplemental_files': len(supplemental), 'fixture_jobs': sorted(fixtures)}


def validate_fresh_fixtures(bundle, jobs, root, source_files):
    frozen = json.loads((bundle / 'source-freeze.json').read_text(encoding='utf-8'))
    report = json.loads((bundle / 'validation.json').read_text(encoding='utf-8'))
    errors, artifacts = [], {}
    if report.get('freeze_sha256') != digest(frozen):
        errors.append('Bundle source-freeze digest mismatch')
    for name in source_files:
        path = (root / name).resolve()
        if frozen['files'].get(str(path)) != sha(path):
            errors.append('Current critical source not bound to this bundle: ' + name)
    steps = {step['name']: step for step in report['steps']}
    for name in jobs:
        step = steps.get(name, {})
        bound = step.get('artifacts', {})
        if step.get('status') != 'PASS' or not bound:
            errors.append('Fresh fixture job did not pass: ' + name)
            continue
        # The executor records full output trees. Rehash every recorded file,
        # including logs/executables; old or changed green annotations fail.
        if files_snapshot(bound) != bound:
            errors.append('Fresh fixture artifact bytes changed: ' + name)
        artifacts[name] = bound
    return {'passed': not errors, 'errors': errors, 'artifacts': artifacts,
            'bundle_freeze_sha256': report.get('freeze_sha256')}


def generate(root, baseline_source):
    template = root / 'tools/modernize/critical_ir_wrappers.c'
    paths = [template, root / 'src/game/q_math.c', root / 'tools/modernize/compat_word_oracle.h',
             baseline_source / 'src/game/tce_ratio_precision.h',
             baseline_source / 'src/game/tce_weather_precision.h',
             baseline_source / 'src/cgame/cg_players.c', baseline_source / 'src/ui/ui_main.c']
    ratio, weather, players, ui = [path.read_text(encoding='utf-8-sig') for path in paths[3:]]
    old_ratio = function(ratio, 'TCE_RatioValue')
    old_divide = function(ratio, 'TCE_FixedDivideFloat')
    old_weather = function(weather, 'TCE_FiniteWeatherOutside')
    # These are verified literal scalar slices of the old controller bodies.
    # The surrounding controller is covered by its independent fixture, not by
    # this scalar compiler experiment.
    backlerp = 'lf->backlerp = 1.0 - (float)(cg.time - lf->oldFrameTime) / (lf->frameTime - lf->oldFrameTime);'
    speed = 'moveSpeed = Distance(cent->lerpOrigin, lf->oldFramePos) / ((float)(cg.time - lf->oldFrameTime) / 1000.0);'
    for literal, body in ((backlerp, function(players, 'CG_RunLerpFrame')),
                          (speed, function(players, 'CG_RunLerpFrameRate')),
                          ('lf->animSpeedScale       = moveSpeed / (float)anim->moveSpeed;', function(players, 'CG_RunLerpFrameRate')),
                          ('textRect.x = text_x - newLineWidth;', function(ui, 'UI_DrawGametypeDescription')),
                          ('textRect.x += rect->x;', function(ui, 'UI_DrawGametypeDescription'))):
        if literal not in body:
            raise ValueError('Reviewed baseline scalar slice changed: ' + literal)
    match = re.search(r'for \(probe = n; probe >> 1; probe >>= 1\)\s*\{\s*\+\+numeratorBit;\s*\}', old_ratio)
    if not match:
        raise ValueError('Reviewed baseline highest-bit loop changed')
    qmath = paths[1].read_text(encoding='utf-8-sig')
    units = [function(qmath, name) for name in ('VectorLength', 'Distance',
             'TCE_VectorLengthSquaredValue', 'TCE_VectorLengthValue')]
    units += [old_ratio.replace('TCE_RatioValue', 'IR_OldRatioValue'),
              old_divide.replace('TCE_FixedDivideFloat', 'IR_OldFixedDivideFloat')
                        .replace('TCE_RatioValue', 'IR_OldRatioValue'),
              old_weather.replace('TCE_FiniteWeatherOutside', 'IR_OldWeatherOutside')]
    source = template.read_text().replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(units))
    source = source.replace('/* BASELINE_BIT_LOOP */', match.group())
    source = source.replace('/* BASELINE_BACKLERP */', 'return 1.0 - (float)e / d;')
    source = source.replace('/* BASELINE_SPEED */', 'float moveSpeed = Distance(a,b) / ((float)e / 1000.0); return moveSpeed / (float)s;')
    source = source.replace('/* BASELINE_UI */', 'float aligned = t - w; return aligned + x;')
    # Bind the full include closure, not just the outer helper headers.
    paths += list((root / 'compat').rglob('*.h')) + [root / 'src/game/q_shared.h',
              root / 'src/game/q_math.h', root / 'src/game/tce_numeric.h', Path(__file__)]
    return source, {str(path.resolve()): sha(path) for path in sorted(set(paths)) if path.is_file()}


def assembly_units(text):
    result = {}
    for match in re.finditer(r'^(\S+)\s+PROC\b(.*?)^\1\s+ENDP\b', text, re.M | re.S):
            label = match[1]
            lines = []
            for line in match[2].splitlines():
                line = line.split(';', 1)[0].strip()
                if not line or line.endswith(':') or line.startswith(('PUBLIC', 'EXTRN', 'INCLUDELIB')):
                    continue
                # Listings have address and machine-byte columns before the
                # actual assembly. Retain instructions/operands, not offsets.
                line = re.sub(r'^[0-9a-f]{5}\s+', '', line, flags=re.I)
                line = re.sub(r'^(?:[0-9a-f]{2}\s+)+', '', line, flags=re.I)
                if line and not re.fullmatch(r'[0-9a-f ]+', line, re.I):
                    lines.append(line)
            result[label] = lines
    return result


def assembly_functions(text):
    units = assembly_units(text)
    result = {}
    for name in PAIRS:
        for side in ('old', 'new'):
            label = 'ir_' + side + '_' + name
            value = units.get('_' + label, units.get(label))
            if not value:
                raise ValueError('Missing actual compiled wrapper: ' + label)
            result[label] = value
    return result


def emitted_closure(label, units):
    """Follow actual local call/tail-jump bodies; record external calls as such."""
    pending, seen, lines, external = [label], set(), [], set()
    while pending:
        name = pending.pop(0)
        if name in seen:
            continue
        if len(seen) >= 256:
            raise ValueError('Bounded emitted helper closure exceeded')
        seen.add(name)
        lines += ['FUNCTION ' + name, *units[name]]
        for instruction in units[name]:
            match = re.match(r'(?:call|jmp)\s+(?:(?:SHORT|NEAR PTR)\s+)?(\S+)', instruction, re.I)
            if not match:
                continue
            target = match[1]
            if target in units:
                pending.append(target)
            elif not target.startswith('$'):
                external.add(target)
    return lines, sorted(seen), sorted(external)


def compile_comparisons(root, baseline_source, out):
    out.mkdir(parents=True, exist_ok=True)
    source, inputs = generate(root, baseline_source)
    generated = out / 'wrappers.c'
    generated.write_text(source, encoding='utf-8', newline='\n')
    env, compiler = msvc_environment(out, 'x86', '14.44')
    dumpbin = Path(compiler).with_name('dumpbin.exe')
    if not dumpbin.is_file():
        raise ValueError('Matching MSVC dumpbin unavailable')
    inputs.update({str(Path(compiler).resolve()): sha(compiler), str(dumpbin.resolve()): sha(dumpbin)})
    for name in ('c1.dll', 'c2.dll', 'link.exe'):
        path = Path(compiler).with_name(name)
        if not path.is_file():
            raise ValueError('Matching compiler/backend/linker missing: ' + str(path))
        inputs[str(path.resolve())] = sha(path)
    report = {'schema': 1, 'source_inputs': inputs, 'generated_sha256': sha(generated), 'runs': [],
        'limits': ['12 scalar/helper pairs, not complete controllers or a game build',
                   'Assembly differences are recorded, never treated as opcode equality or numerical parity',
                   'Tiny runtime controls supplement required freshly executed complete fixture reports']}
    for configuration, flag in (('debug', '/Od'), ('release', '/O2')):
        directory = out / configuration
        directory.mkdir(exist_ok=True)
        obj, asm, exe = directory / 'wrappers.obj', directory / 'wrappers.asm', directory / 'wrappers.exe'
        dependencies = directory / 'dependencies.json'
        command = [compiler, '/nologo', '/TC', flag, '/fp:strict', '/Gy', '/sourceDependencies', str(dependencies), '/FAcs', '/Fa' + str(asm),
                   '/I' + str(root), '/Fo' + str(obj), '/Fe' + str(exe), str(generated), '/link', '/OPT:REF']
        built = subprocess.run(command, cwd=directory, env=env, capture_output=True, text=True)
        (directory / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
        if built.returncode:
            raise ValueError('Arithmetic wrapper compile failed: ' + str(directory / 'compile.log'))
        includes = json.loads(dependencies.read_text(encoding='utf-8-sig')).get('Data', {}).get('Includes')
        if not includes:
            raise ValueError('No actual compiler include trace')
        inputs.update({str(Path(name.strip()).resolve()): sha(name.strip()) for name in includes})
        disasm = subprocess.run([str(dumpbin), '/disasm', str(obj)], env=env, capture_output=True, text=True)
        (directory / 'disassembly.txt').write_text(disasm.stdout + disasm.stderr, encoding='utf-8')
        if disasm.returncode:
            raise ValueError('Actual object disassembly failed')
        assembly = asm.read_text(errors='replace')
        instructions = assembly_functions(assembly)
        # Wrappers may call or tail-jump to a helper rather than inline it.
        # Preserve all actual emitted helper bodies for that comparison too.
        units = assembly_units(assembly)
        write_json(directory / 'emitted-helper-bodies.json', units)
        comparisons = []
        for name in PAIRS:
            old, new = [instructions['ir_' + side + '_' + name] for side in ('old', 'new')]
            diff = directory / (name + '.diff.txt')
            diff.write_text('\n'.join(difflib.unified_diff(old, new, fromfile='baseline scalar/helper',
                            tofile='current scalar/helper', lineterm='')) + '\n', encoding='utf-8', newline='\n')
            comparisons.append({'pair': name, 'baseline_instruction_lines': len(old),
                'current_instruction_lines': len(new), 'equal_listing': old == new, 'diff': str(diff)})
            expanded = [emitted_closure('_ir_' + side + '_' + name, units) for side in ('old', 'new')]
            closure_diff = directory / (name + '.helper-closure.diff.txt')
            closure_diff.write_text('\n'.join(difflib.unified_diff(expanded[0][0], expanded[1][0],
                fromfile='baseline wrapper and emitted helpers', tofile='current wrapper and emitted helpers',
                lineterm='')) + '\n', encoding='utf-8', newline='\n')
            comparisons[-1].update(helper_closure_diff=str(closure_diff),
                baseline_helper_symbols=expanded[0][1], current_helper_symbols=expanded[1][1],
                baseline_external_targets=expanded[0][2], current_external_targets=expanded[1][2])
        run = subprocess.run([str(exe)], cwd=directory, env=env, capture_output=True, text=True, timeout=30)
        (directory / 'run.log').write_text(run.stdout + run.stderr, encoding='utf-8')
        result = json.loads(run.stdout)
        if run.returncode or result.get('checks') != 12 or result.get('failures') != 0:
            raise ValueError('Arithmetic wrapper runtime controls failed: ' + str(directory / 'run.log'))
        report['runs'].append({'configuration': configuration, 'command': command,
            'dumpbin_command': [str(dumpbin), '/disasm', str(obj)], 'runtime': result, 'comparisons': comparisons,
            'artifacts': {str(p.resolve()): sha(p) for p in directory.iterdir() if p.is_file()}})
    report['passed'] = all(Path(name).is_file() and sha(name) == value for name, value in inputs.items())
    if not report['passed']:
        raise ValueError('Compiler/header/source inputs changed during comparison')
    write_json(out / 'arithmetic-comparisons.json', report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--baseline-source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--arithmetic-only', action='store_true', help='Diagnostic compiler comparison only; not the critical gate')
    mode.add_argument('--policy', type=Path, help='Explicit locally reviewed exact critical policy')
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--risk', type=Path)
    parser.add_argument('--reviews', type=Path)
    parser.add_argument('--bundle-out', type=Path)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = {'mode': 'arithmetic-only' if args.arithmetic_only else 'critical-gate', 'passed': False}
    if args.policy:
        if not all((args.baseline, args.risk, args.reviews, args.bundle_out)):
            parser.error('Critical gate requires --baseline, --risk, --reviews and --bundle-out')
        before, current, reviews, policy = [json.loads(path.read_text(encoding='utf-8')) for path in
                                           (args.baseline, args.risk, args.reviews, args.policy)]
        report['decisions'] = validate_decisions(root, before, current, reviews, policy,
                                                sha(args.baseline), sha(args.reviews))
        if report['decisions']['passed']:
            report['fixtures'] = validate_fresh_fixtures(args.bundle_out, report['decisions']['fixture_jobs'],
                                                        root, [file['path'] for file in current['files']] + list(SUPPLEMENTAL))
        if not report['decisions']['passed'] or not report.get('fixtures', {}).get('passed'):
            write_json(out / 'report.json', report)
            print(json.dumps(report))
            return 1
        # Old slices must really be the exact preserved baseline source bytes.
        files = {file['path']: file['sha256'] for file in before['files']}
        for name in ('src/game/tce_ratio_precision.h', 'src/game/tce_weather_precision.h',
                     'src/cgame/cg_players.c', 'src/ui/ui_main.c'):
            if sha(args.baseline_source / name) != files[name]:
                raise ValueError('Old wrapper source does not match baseline inventory: ' + name)
    arithmetic = compile_comparisons(root, args.baseline_source.resolve(), out / 'arithmetic')
    report.update(passed=arithmetic['passed'], arithmetic=arithmetic)
    write_json(out / 'report.json', report)
    print(json.dumps({'mode': report['mode'], 'passed': report['passed'], 'configurations': len(arithmetic['runs']), 'pairs': len(PAIRS)}))
    return int(not report['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
