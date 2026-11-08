"""Native Windows regression for safe, process-owned ET/SDL window controls."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    for arch in ('x86', 'x64'):
        work = out / arch
        work.mkdir(exist_ok=True)
        env, compiler = msvc_environment(work, arch, '14.39')
        exe = work / 'fixture.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/O2', '/UNDEBUG',
                   '/I' + str(ROOT), str(ROOT / 'tools/modernize/window_fixture.c'),
                   str(ROOT / 'platform/platform_window.c'), '/Fe:' + str(exe),
                   '/link', 'user32.lib']
        built = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True)
        (work / 'compile.log').write_text(built.stdout + built.stderr)
        row = dict(arch=arch, compile_exit=built.returncode)
        if built.returncode == 0:
            run = subprocess.run([str(exe)], cwd=work, env=env, capture_output=True,
                                 text=True, timeout=15)
            row.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        rows.append(row)
    paths = ['platform/platform_window.c', 'platform/platform_window.h',
             'tools/modernize/window_fixture.c', 'tools/modernize/window_regression.py']
    report = dict(passed=all(row.get('run_exit', -1) == 0 for row in rows), results=rows,
                  source_sha256={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths})
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    return int(not report['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
