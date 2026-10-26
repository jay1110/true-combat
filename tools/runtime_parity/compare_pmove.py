#!/usr/bin/env python3
"""Replay the real Win32 client/server Pmove against authenticated originals.

Requires an existing MSVC Win32 module build and matching linker maps. This
does not launch a game or replace movement routines with mocks. The trace
callbacks describe four deterministic synthetic collision worlds, not a BSP.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "modernize"))
from compat_validate import msvc_environment

ORIGINAL_HASHES = {
    "cgame": "d85a8e94da7c94956eff560e4cc406b4116f5a6ffd820b59accd9ad7e1e2414c",
    "qagame": "f4f17855f568fcd9be760047943d50cfcb2e3bcf8691471172fe7771c2ae04c7",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def behavior_checks(results: list[list[str]]) -> dict[str, bool]:
    """Do not let two inactive controllers count as successful traversal."""
    checks = {}
    for row in results:
        _, fps, mode, fixed = row[:4]
        world = row[10]
        position, apex = float(row[4]), float(row[11])
        key = f"world{world}-mode{mode}-{fps}fps-fixed{fixed}"
        if (world, mode) == ("0", "6"):
            checks[key] = apex > 40  # The actual original standing-jump arc.
        elif (world, mode) == ("1", "0"):
            checks[key] = position > 600 and apex >= 120
        elif (world, mode) == ("2", "0"):
            checks[key] = 241.99 <= position <= 242.001
        elif (world, mode) == ("3", "8"):
            checks[key] = position > 500 and apex >= 56
    return checks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--original-dir", type=Path, default=ROOT / "tcetest")
    parser.add_argument("--config", choices=["Debug", "Release"], default="Release")
    parser.add_argument("--module", choices=["both", "cgame", "qagame"], default="both")
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "pmove-parity")
    parser.add_argument("--msvc-version")
    parser.add_argument("--arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--capture-reference", action="store_true",
                        help="Write compact full-state Win32 original replay oracles")
    parser.add_argument("--reference-dir", type=Path,
                        help="Read previously captured original oracles (required for x64)")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    build = args.build_dir.resolve()
    originals = args.original_dir.resolve()
    if args.arch == "x64" and not args.reference_dir:
        parser.error("x64 requires --reference-dir captured with the Win32 original")
    if args.capture_reference and args.reference_dir:
        parser.error("capture and reference-dir are mutually exclusive")
    env, compiler = msvc_environment(out, args.arch, args.msvc_version)
    source = ROOT / "src" / "tests" / "tce_pmove_runtime_test.c"
    modules = ORIGINAL_HASHES if args.module == "both" else [args.module]
    report = {
        "source": str(source), "source_sha256": sha256(source),
        "config": args.config, "arch": args.arch, "compiler": compiler,
        "claim": "Real Pmove/PmoveSingle controllers; complete playerState and pmoveExt comparison after every usercmd",
        "limitations": ["Windows module replay", "Synthetic collision worlds", "Empty animation tables", "No engine networking/prediction replay or BSP traversal"],
        "cases": [],
    }
    failed = False
    for module in modules:
        original = originals / f"{module}_mp_x86.dll"
        suffix = "x86" if args.arch == "x86" else "x64"
        port = build / "bin" / args.config / f"{module}_mp_{suffix}.dll"
        symbols = build / f"{module}-symbols-{args.config}.map"
        if sha256(original) != ORIGINAL_HASHES[module]:
            raise RuntimeError(f"Original identity mismatch: {original}")
        for path in (port, symbols):
            if not path.is_file():
                raise FileNotFoundError(path)
        exe = out / f"pmove-{module}-{args.arch}-{args.config}.exe"
        obj = exe.with_suffix(".obj")
        define = "CGAMEDLL" if module == "cgame" else "GAMEDLL"
        command = [compiler, "/nologo", "/TC", "/DWIN32", f"/D{define}",
                   "/D_CRT_SECURE_NO_WARNINGS", "/O2", "/Ob2", "/MD",
                   "/I" + str(ROOT), "/I" + str(ROOT / "src" / "game"),
                   str(source), "/Fo" + str(obj), "/Fe" + str(exe)]
        compile_result = subprocess.run(command, env=env, capture_output=True, text=True, errors="replace")
        (out / f"compile-{module}-{args.config}.log").write_text(
            compile_result.stdout + compile_result.stderr, encoding="utf-8")
        if compile_result.returncode:
            raise RuntimeError(f"Harness compile failed: {module}")
        output = out / f"movement-{module}-{args.config}.csv"
        invocation = [str(exe), str(original), str(port), str(symbols), str(output)]
        reference = None
        if args.capture_reference:
            reference = out / f"original-{module}.pmove"
            invocation += [str(reference), "write"]
        elif args.reference_dir:
            reference = args.reference_dir.resolve() / f"original-{module}.pmove"
            invocation += [str(reference), "read"]
        run = subprocess.run(invocation,
                             env=env, capture_output=True, text=True, errors="replace")
        (out / f"run-{module}-{args.config}.log").write_text(run.stdout + run.stderr, encoding="utf-8")
        with output.open(encoding="utf-8") as stream:
            rows = list(csv.reader(stream))
        summaries = [row for row in rows if row[0] == "summary"]
        results = [row for row in rows if row[0] == "result"]
        summary = summaries[-1] if summaries else None
        behavior = behavior_checks(results)
        passed = (run.returncode == 0 and summary == ["summary", "432", "0", "0", "0", "0"]
                  and len(behavior) == 48 and all(behavior.values()))
        entry = {"module": module, "passed": passed, "exit_code": run.returncode,
                 "summary": summary, "streams": len(results), "csv": str(output),
                 "behavior_checks": behavior,
                 "original_sha256": sha256(original), "port_sha256": sha256(port),
                 "map_sha256": sha256(symbols), "harness_sha256": sha256(exe)}
        if reference:
            entry.update(reference=str(reference), reference_sha256=sha256(reference))
        report["cases"].append(entry)
        failed |= not passed
        print(f"{module} {args.config}: {'PASS' if passed else 'FAIL'}; {run.stdout.strip()}", flush=True)
    report["passed"] = not failed
    (out / f"report-{args.config}.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
