"""Strict, source-bound validation executor. No implicit builds or deployments."""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass, field
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def file_hash(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def files_snapshot(paths):
    """Exact paths and bytes, including directory additions; missing != empty."""
    result = {}
    for item in sorted({str(Path(p).resolve()) for p in paths}):
        path = Path(item)
        if path.is_symlink():
            raise ValueError('Validation inputs/outputs must not be symlinks: ' + item)
        if path.is_dir():
            result[item + '/'] = 'directory'
            for child in sorted(path.rglob('*')):
                if child.is_symlink():
                    raise ValueError('Validation tree contains symlink: ' + str(child))
                if child.is_file():
                    result[str(child.resolve())] = file_hash(child)
        else:
            result[item] = file_hash(path) if path.is_file() else None
    return result


def source_paths(root):
    """Tracked and untracked release inputs; never local evidence or game trees."""
    from subprocess import check_output
    names = check_output(['git', '-C', str(root), 'ls-files', '-z', '--cached',
                          '--others', '--exclude-standard']).decode('utf-8').split('\0')
    # Read the literal release policy without importing release packaging tools
    # (and their package-name imports) into the executor process.
    import ast
    tree = ast.parse((root / 'tools/release/source_archive.py').read_text(encoding='utf-8'))
    policy = {node.targets[0].id: ast.literal_eval(node.value) for node in tree.body
              if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name)
              and node.targets[0].id in ('PREFIXES', 'ROOT_FILES')}
    prefixes, roots = policy['PREFIXES'], policy['ROOT_FILES']
    skipped = {'build', '__pycache__', 'node_modules', '.venv', 'reconstruction'}
    return sorted({root / name for name in names if name and
                   (name in roots or name.startswith(prefixes)) and
                   not skipped.intersection(Path(name).parts) and
                   Path(name).name != 'AGENTS.md' and
                   Path(name).suffix.lower() not in ('.dll', '.so', '.exe', '.pdb', '.zip', '.pk3', '.dmp', '.pyc') and
                   (root / name).is_file()})


@dataclass
class Job:
    name: str
    category: str
    command: list[str] = field(default_factory=list)
    outputs: list[str] = field(default_factory=list)
    needs: list[str] = field(default_factory=list)
    unavailable: str = ''
    env: dict[str, str] = field(default_factory=dict)
    required: bool = True
    evidence: list[str] = field(default_factory=list)
    artifact_manifest: str = ''


def artifact_paths(job):
    paths = job.outputs + job.evidence
    if job.artifact_manifest and Path(job.artifact_manifest).is_file():
        extra = json.loads(Path(job.artifact_manifest).read_text(encoding='utf-8')).get('artifact_paths')
        if not isinstance(extra, list) or not all(isinstance(p, str) and Path(p).is_absolute() for p in extra):
            raise ValueError('Invalid generated artifact path manifest: ' + job.artifact_manifest)
        paths = paths + extra
    return paths


def tool_identity(command, env):
    executable = shutil.which(command[0], path=env.get('PATH')) if command else None
    return {'path': executable, 'sha256': file_hash(executable) if executable else None}


class Executor:
    def __init__(self, root, out, jobs, inputs, context=None):
        self.root, self.out, self.jobs = Path(root), Path(out), jobs
        self.inputs, self.context = inputs, context or {}
        names = set()
        for job in jobs:
            if not re.fullmatch(r'[a-zA-Z0-9_-]+', job.name) or job.name in names or any(n not in names for n in job.needs):
                raise ValueError('Duplicate, unordered or missing job dependency: ' + job.name)
            if not job.command and not job.unavailable:
                raise ValueError('A runnable gate needs a command: ' + job.name)
            names.add(job.name)

    def freeze(self):
        env = os.environ.copy()
        relevant = ('PATH', 'INCLUDE', 'LIB', 'LIBPATH', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS',
                    'LDFLAGS', 'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET',
                    'PYTHONPATH', 'PYTHONHOME', 'VCToolsVersion', 'WindowsSDKVersion')
        return {'files': files_snapshot(self.inputs()), 'plan': [asdict(j) for j in self.jobs],
                'context': self.context, 'python': sys.version, 'platform': sys.platform,
                'environment': {key: env.get(key) for key in relevant},
                'tools': {j.name: tool_identity(j.command, dict(env, **j.env)) for j in self.jobs}}

    def save(self, report):
        self.out.mkdir(parents=True, exist_ok=True)
        temporary = self.out / 'validation.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        temporary.replace(self.out / 'validation.json')
        rows = ['# Strict mod validation bundle', '',
                'Source freeze: `' + report['freeze_sha256'] + '`', '',
                '| Gate | Category | Status | Log / reason |', '|---|---|---|---|']
        for step in report['steps']:
            rows.append('| ' + ' | '.join(str(step.get(k, '')).replace('|', '\\|')
                        for k in ('name', 'category', 'status', 'detail')) + ' |')
        rows += ['', 'Result: ' + report['status'] + '.', '',
                 'A deterministic fixture is not live-engine, network, map, or gameplay parity proof.']
        if report.get('missing_inputs'):
            rows += ['', 'Missing required frozen inputs:', *['- ' + p for p in report['missing_inputs']]]
        if report.get('source_stable') is False:
            rows += ['', 'Source/input bytes changed during execution; all-green steps are invalid.']
        (self.out / 'validation.md').write_text('\n'.join(rows) + '\n', encoding='utf-8')

    def run(self, resume=False, verify=False):
        frozen = self.freeze()
        freeze_hash = digest(frozen)
        previous = None
        path = self.out / 'validation.json'
        if path.exists():
            if not (resume or verify):
                raise ValueError('Output has a result; explicitly --resume or select an empty output')
            previous = json.loads(path.read_text(encoding='utf-8'))
            if previous.get('freeze_sha256') != freeze_hash:
                raise ValueError('Source/configuration/tool freeze changed; use a new output directory')
        elif verify:
            raise ValueError('No source-bound bundle to verify')
        elif self.out.exists() and any(self.out.iterdir()):
            raise ValueError('Output is not empty')
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / 'source-freeze.json').write_text(json.dumps(frozen, indent=2) + '\n', encoding='utf-8')
        report = {'schema': 2, 'mode': 'strict-release', 'started': time.time(),
                  'freeze_sha256': freeze_hash, 'steps': [], 'passed': False, 'status': 'INCOMPLETE'}
        report['missing_inputs'] = [name for name, value in frozen['files'].items() if value is None]
        old = {s['name']: s for s in previous['steps']} if previous else {}
        completed = {}
        for job in self.jobs:
            step = {'name': job.name, 'category': job.category, 'required': job.required,
                    'command': job.command, 'status': 'INCOMPLETE', 'detail': ''}
            dependency = {name: completed[name].get('result_sha256') for name in job.needs}
            cached = old.get(job.name, {})
            stale = cached.get('status') == 'STALE'
            if cached.get('status') == 'PASS':
                stale = cached.get('artifacts') != files_snapshot(artifact_paths(job) + [cached['log']])
            if stale and not verify:
                raise ValueError('Passed gate artifacts changed: ' + job.name + '; use a new output directory')
            if job.unavailable:
                step['detail'] = job.unavailable
            elif stale:
                step.update(status='STALE', detail='Passed result artifacts changed; a new clean bundle is required')
            elif any(completed[name]['status'] != 'PASS' for name in job.needs):
                step['detail'] = 'Dependency did not pass: ' + ', '.join(job.needs)
            elif (cached.get('status') == 'PASS' and cached.get('dependencies') == dependency
                  and cached.get('artifacts') == files_snapshot(artifact_paths(job) + [cached['log']])):
                step = dict(cached, reused=True)
            elif verify:
                step['detail'] = 'Missing, failed or modified result/artifact; verification never executes'
            else:
                # Old files cannot satisfy a new success without being produced again.
                before = {p: Path(p).stat().st_mtime_ns if Path(p).exists() else None for p in job.outputs}
                log = self.out / (job.name + '.log')
                started = time.time()
                try:
                    with log.open('w', encoding='utf-8') as stream:
                        result = subprocess.run(job.command, cwd=self.root,
                                                env=dict(os.environ, **job.env),
                                                stdout=stream, stderr=subprocess.STDOUT)
                    rc = result.returncode
                    artifacts = files_snapshot(artifact_paths(job) + [log])
                    missing = [p for p in artifact_paths(job) if not Path(p).exists()]
                    unchanged = [p for p in job.outputs if Path(p).is_file() and before[p] is not None
                                 and Path(p).stat().st_mtime_ns == before[p]]
                    if job.outputs and not any(Path(p).is_file() for p in job.outputs):
                        unchanged.append('No regenerated file output was declared/produced')
                    step.update(returncode=rc, seconds=round(time.time() - started, 3),
                                log=str(log), artifacts=artifacts, dependencies=dependency)
                    step['status'] = 'PASS' if rc == 0 and not missing and not unchanged else 'FAIL'
                    step['detail'] = ('Missing outputs: ' + ', '.join(missing) if missing else
                                      'Outputs not regenerated: ' + ', '.join(unchanged) if unchanged else str(log))
                    step['result_sha256'] = digest({k: step[k] for k in ('artifacts', 'dependencies', 'returncode')})
                except (OSError, ValueError) as error:
                    step.update(status='FAIL', detail=str(error))
            report['steps'].append(step)
            completed[job.name] = step
            self.save(report)
            print(job.name + ': ' + step['status'], flush=True)
        stable = self.freeze() == frozen
        for job, step in zip(self.jobs, report['steps']):
            if step['status'] == 'PASS' and step['artifacts'] != files_snapshot(artifact_paths(job) + [step['log']]):
                step.update(status='STALE', detail='Artifact changed after this gate ran; a new clean bundle is required')
        report.update(finished=time.time(), source_stable=stable)
        required = [s for s in report['steps'] if s['required']]
        report['passed'] = bool(required) and stable and not report['missing_inputs'] and all(s['status'] == 'PASS' for s in required)
        report['status'] = ('PASS' if report['passed'] else 'FAIL' if not stable or
                            any(s['status'] in ('FAIL', 'STALE') for s in required) else 'INCOMPLETE')
        self.save(report)
        return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--build-map', type=Path, required=True)
    parser.add_argument('--baseline-dir', type=Path)
    parser.add_argument('--reviews', type=Path, help='Explicit reviewed source-diff manifest; overrides build-map source_reviews')
    parser.add_argument('--plan', action='store_true', help='Print a plan; execute nothing and create no output')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--parallel', type=int, default=4)
    args = parser.parse_args(argv)
    from validation_catalog import make_plan
    config = json.loads(args.build_map.read_text(encoding='utf-8'))
    if args.reviews:
        config['source_reviews'] = str(args.reviews.resolve())
    jobs, extra = make_plan(ROOT, args.out.resolve(), config, args.baseline_dir, args.parallel)
    executor = Executor(ROOT, args.out.resolve(), jobs,
                        lambda: source_paths(ROOT) + [args.build_map.resolve()] + extra,
                        {'build_map': config, 'baseline': str(args.baseline_dir)})
    if args.plan:
        print(json.dumps({'schema': 2, 'jobs': [asdict(j) for j in jobs],
                          'required': sum(j.required for j in jobs),
                          'unavailable': [j.name for j in jobs if j.unavailable]}, indent=2))
        return 0
    return 0 if executor.run(args.resume, args.verify)['passed'] else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(2)
