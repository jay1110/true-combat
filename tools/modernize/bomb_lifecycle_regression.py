#!/usr/bin/env python3
"""Compile G17 real death/disconnect, launch, pickup and ownership functions.

Deterministic external services replace engine housekeeping and collision. This
is not a player_die, real map, network disconnect or native revive acceptance.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/modernize"))
from compat_validate import msvc_environment
from weapon_regression import function


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/bomb-lifecycle")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--configuration", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--msvc-version")
    parser.add_argument("--compiler", choices=["auto", "gcc", "clang"], default="auto")
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    source_root = args.source_root.resolve()
    names = {
        "g_items.c": ["Pickup_Ammo", "G_TCELaunchObjective", "G_TCEDropBomb",
                      "G_TCEDropVip", "G_TCEReleaseObjectives", "Touch_Item"],
        "g_combat.c": ["TossClientItems"],
        "g_client.c": ["ClientDisconnect"],
        "bg_misc.c": ["BG_CanItemBeGrabbed"],
    }
    units, inputs = [], []
    for filename, functions in names.items():
        path = source_root / "src/game" / filename
        if not path.is_file(): path = source_root / filename
        if not path.is_file(): path = ROOT / "src/game" / filename
        text = path.read_text(encoding="utf-8-sig")
        inputs.append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                       "functions": functions})
        units.extend(function(text, name) for name in functions)
    fixture = ROOT / "tools/modernize/bomb_lifecycle_fixture.c"
    generated = fixture.read_text(encoding="utf-8").replace("/* PRODUCTION_FUNCTIONS */", "\n\n".join(units))
    source = out / "bomb_lifecycle.c"; source.write_text(generated, encoding="utf-8", newline="\n")
    if sys.platform == "win32":
        env, cc = msvc_environment(out, args.arch, args.msvc_version)
        exe = out / "bomb_lifecycle.exe"
        command = [cc, "/nologo", "/TC", "/Od" if args.configuration == "Debug" else "/O2",
                   "/W3", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(ROOT), str(source),
                   "/Fe:" + str(exe), "/Fo:" + str(out / "bomb_lifecycle.obj")]
        if args.sanitize: command.insert(1, "/fsanitize=address")
    else:
        env = None
        cc = ((shutil.which("clang") or shutil.which("gcc")) if args.compiler == "auto" else shutil.which(args.compiler))
        if not cc: raise RuntimeError("No C compiler available")
        exe = out / "bomb_lifecycle"
        command = [cc, "-std=c99", "-O0" if args.configuration == "Debug" else "-O2", "-I", str(ROOT), str(source), "-lm", "-o", str(exe)]
        if args.sanitize: command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    build = subprocess.run(command, capture_output=True, text=True, env=env, cwd=out)
    (out / "compile.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    if build.returncode:
        print(build.stdout + build.stderr); return build.returncode
    run = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
    report = {"source_root": str(source_root), "inputs": inputs, "command": command,
              "arch": args.arch, "configuration": args.configuration, "sanitize": args.sanitize,
              "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "generated_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "unit_sha256": [hashlib.sha256(unit.encode()).hexdigest() for unit in units],
              "stdout": run.stdout, "stderr": run.stderr, "exit_code": run.returncode,
              "limits": ["Engine collision, entity creation and housekeeping are service stubs",
                         "Full native player_die, network disconnect and revive remain separate live tests",
                         "Archived source roots use explicitly reported current files where no archive exists"]}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(run.stdout, end=""); print(run.stderr, end="", file=sys.stderr)
    return run.returncode


if __name__ == "__main__":
    raise SystemExit(main())
