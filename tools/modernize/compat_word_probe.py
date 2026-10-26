#!/usr/bin/env python3
"""Compare optimized integer compatibility primitives to their frozen bodies."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]
ORACLE_SHA256 = "4b0ebcc25da66b89521fe9fb7a48571011de167ed174df47a620d0872da8e7f9"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/runtime-parity-compat-word")
    parser.add_argument("--msvc-version", default="14.44")
    parser.add_argument("--sanitize", action="store_true", help="Run the same frozen-body comparisons with MSVC AddressSanitizer")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if hashlib.sha256((ROOT / "tools/modernize/compat_word_oracle.h").read_bytes()).hexdigest() != ORACLE_SHA256:
        raise ValueError("Frozen pre-optimization oracle changed; review rather than silently refreshing it")
    env, compiler = msvc_environment(out, "x86", args.msvc_version)
    source = ROOT / "tools/modernize/compat_word_golden.c"
    records = []
    for name, flag in (("debug", "/Od"), ("release", "/O2")):
        executable = out / (name + ".exe")
        command = [compiler, "/nologo", "/TC", flag, "/D_CRT_SECURE_NO_WARNINGS", "/Fo:" + str(out / (name + ".obj")),
                   "/Fe:" + str(executable), str(source)]
        if args.sanitize:
            command[4:4] = ["/fsanitize=address", "/Zi"]
        build = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True, errors="replace")
        (out / (name + "-build.log")).write_text(build.stdout + build.stderr, encoding="utf-8")
        record = {"name": name, "command": command, "sanitizer_requested": args.sanitize, "build_returncode": build.returncode}
        if not build.returncode:
            run = subprocess.run([str(executable)], cwd=out, env=env, capture_output=True, text=True, errors="replace", timeout=60)
            record.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
            (out / (name + "-run.log")).write_text(run.stdout + run.stderr, encoding="utf-8")
        records.append(record)
    report = {"schema": 1, "runs": records, "passed": all(r.get("returncode") == 0 for r in records),
              "files": {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in
                        ["compat/legacy/game/tce_trigger_precision.h", "compat/legacy/game/tce_rotate_precision.h",
                         "tools/modernize/compat_word_oracle.h", "tools/modernize/compat_word_golden.c"]}}
    (out / "compat-word-probe.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
