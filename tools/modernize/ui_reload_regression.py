#!/usr/bin/env python3
"""Exercise the unchanged extracted production UI_Load body with fake services."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from compat_validate import msvc_environment
from movement_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/ui-reload-regression")
    parser.add_argument("--source", type=Path, default=ROOT / "src/ui/ui_main.c")
    parser.add_argument("--arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    body = function(args.source.read_text(encoding="utf-8-sig"), "UI_Load")
    fixture = Path(__file__).with_name("ui_reload_regression_fixture.c")
    source = out / "ui_reload_regression.c"
    source.write_text(fixture.read_text(encoding="utf-8").replace("/* PRODUCTION_UI_LOAD */", body), encoding="utf-8", newline="\n")
    if os.name == "nt":
        env, compiler = msvc_environment(out, args.arch, "14.44")
        exe = out / "ui_reload_regression.exe"
        cmd = [compiler, "/nologo", "/TC", "/O2", "/W3", "/D_CRT_SECURE_NO_WARNINGS", str(source), "/Fe:"+str(exe), "/Fo:"+str(out)+"\\"]
        if args.sanitize: cmd.insert(1, "/fsanitize=address")
    else:
        env = None; compiler = shutil.which("clang") or shutil.which("gcc")
        exe = out / "ui_reload_regression"
        cmd = [compiler, "-std=c99", "-O2", str(source), "-o", str(exe)]
        if args.sanitize: cmd[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    built = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)
    (out / "compile.log").write_text(built.stdout+built.stderr, encoding="utf-8")
    if built.returncode: print(built.stdout+built.stderr); return built.returncode
    run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    report = {"source":str(args.source.resolve()), "function_sha256":hashlib.sha256(body.encode()).hexdigest(), "fixture_sha256":hashlib.sha256(fixture.read_bytes()).hexdigest(), "compiler":compiler,"command":cmd,"stdout":run.stdout,"stderr":run.stderr,"exit_code":run.returncode,"limits":["Actual UI_Load body; engine parser/render services faked", "Baseline no-focus activation counted without reading uninitialized name", "Not a proof that this defect caused the live cursor-only menu"]}
    (out / "report.json").write_text(json.dumps(report,indent=2)+"\n", encoding="utf-8")
    print(run.stdout, end=""); print(run.stderr, end="")
    return run.returncode


if __name__ == "__main__": raise SystemExit(main())
