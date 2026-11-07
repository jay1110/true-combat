"""Exercise production planning/delivery functions after a previous bot leaves."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]


def extract(text, signature):
    start = text.index(signature)
    begin = text.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--optimized', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--reproduce-old', action='store_true', help='move only m_Client assignment back to the old location; failure must be observed')
    a = p.parse_args()
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'vendor/omni-bot/source/Omnibot/Common/PathPlannerWaypoint.cpp'
    fixture = Path(__file__).with_name('planner_client_lifetime_fixture.cpp')
    actual = source.read_text()
    plan = extract(actual, 'int PathPlannerWaypoint::PlanPathToNearestWithEntryPolicy(')
    get_path = extract(actual, 'void PathPlannerWaypoint::GetPath(')
    if a.reproduce_old:
        assert plan.count('m_Client = _client;') == 1
        plan = plan.replace('\tm_Client = _client;\n', '', 1).replace('\tTcePrepareRoutes(_client);', '\tm_Client = _client;\n\tTcePrepareRoutes(_client);', 1)
    code = fixture.read_text().replace('/* ACTUAL_PLAN */', plan).replace('/* ACTUAL_GET_PATH */', get_path)
    generated = out / 'planner_client.cpp'
    generated.write_text(code, newline='\n')
    if os.name == 'nt':
        env, cc = msvc_environment(out, 'x64' if a.sanitize else 'x86', '14.44')
        exe = out / 'planner_client.exe'
        cmd = [cc, '/nologo', '/EHsc', '/W3', '/O2' if a.optimized else '/Od', str(generated), '/Fe:' + str(exe), '/Fo:' + str(out) + os.sep]
        if a.sanitize:
            cmd += ['/fsanitize=address', '/Zi', '/link', '/DEBUG']
    else:
        env = os.environ.copy()
        exe = out / 'planner_client'
        cmd = [shutil.which('clang++') or shutil.which('g++'), '-std=c++11', '-O2' if a.optimized else '-O0', str(generated), '-o', str(exe)]
        if a.sanitize:
            cmd += ['-fsanitize=address,undefined', '-g']
    built = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr)
    report = {'source_sha256': {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in (source, fixture, Path(__file__))}, 'actual_plan_sha256': hashlib.sha256(plan.encode()).hexdigest(), 'actual_get_path_sha256': hashlib.sha256(get_path.encode()).hexdigest(), 'command': cmd, 'build_exit': built.returncode, 'reproduce_old': a.reproduce_old, 'limits': ['Complete actual PlanPathToNearestWithEntryPolicy/GetPath bodies; controlled empty-graph and engine service stubs.', 'Tests client lifetime and volume-only return, not full path search or native player movement.']}
    if built.returncode:
        print(built.stdout + built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=20)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout + run.stderr, end='')
    report['passed'] = built.returncode == 0 and (report.get('run_exit') == 7 if a.reproduce_old else report.get('run_exit') == 0)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return int(not report['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
