#!/usr/bin/env python3
"""Check preserved helper hashes and run small deterministic native contracts.

This is a compatibility-layer check, not a module/game parity claim. The host
WASM-vector case checks packing only and does not claim a working WASM runtime.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def msvc_environment(out: Path, arch: str, version: str | None) -> tuple[dict[str, str], str]:
    program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    candidates = sorted(program_files.glob(
        "Microsoft Visual Studio/*/*/Common7/Tools/VsDevCmd.bat"), reverse=True)
    if not candidates:
        raise RuntimeError("No C compiler in PATH and no Visual Studio developer environment found")
    setup = out / "compat-msvc-environment.cmd"
    version_option = ""
    if version:
        if not re.fullmatch(r"\d+(?:\.\d+){1,2}", version):
            raise RuntimeError("Invalid MSVC toolset version")
        version_option = " -vcvars_ver=" + version
    setup.write_text(
        f'@call "{candidates[0]}" -arch={arch} -host_arch=x64{version_option} >nul\n'
        '@if errorlevel 1 exit /b 1\n@set\n', encoding="utf-8")
    result = subprocess.run(["cmd.exe", "/d", "/c", str(setup)],
                            capture_output=True, text=True, errors="replace")
    if result.returncode:
        raise RuntimeError("Visual Studio developer environment failed: " + result.stderr)
    env = os.environ.copy()
    for line in result.stdout.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key.upper()] = value
    compiler = shutil.which("cl.exe", path=env.get("PATH"))
    if not compiler:
        raise RuntimeError("Visual Studio did not provide cl.exe")
    return env, compiler


def relocation_checks(root: Path) -> dict:
    manifest = root / "tools/modernize/compat_implementations.json"
    data = json.loads(manifest.read_text(encoding="utf-8"))
    failures = []
    reviewed_changes = []
    unchanged = 0
    for entry in data["files"]:
        implementation = root / entry["implementation"]
        forwarder = root / entry["original"]
        actual = digest(implementation) if implementation.is_file() else None
        if actual == entry["baseline_sha256"]:
            unchanged += 1
        else:
            review = entry.get("reviewed_change", {})
            if (actual and actual == review.get("sha256") and review.get("reason")
                    and review.get("regression_commands") and review.get("evidence")):
                reviewed_changes.append({"implementation": entry["implementation"],
                                         "baseline_sha256": entry["baseline_sha256"],
                                         **review})
            else:
                failures.append(entry["implementation"] + ": unreviewed implementation hash change")
        expected = ('/* Compatibility implementation lives outside gameplay sources. */\n'
                    f'#include "../../{entry["implementation"]}"\n').encode()
        if not forwarder.is_file() or forwarder.read_bytes() != expected:
            failures.append(entry["original"] + ": forwarding include changed")
    return {"files": len(data["files"]), "unchanged": unchanged,
            "reviewed_changes": reviewed_changes, "failures": failures,
            "claim": "Baseline hashes retained; explicit reviewed runtime corrections reported separately; exact forwarding includes"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="auto")
    parser.add_argument("--msvc-arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--msvc-version", help="Installed MSVC toolset, for example 14.44")
    parser.add_argument("--configuration", choices=["Debug", "Release", "both"], default="both")
    parser.add_argument("--out", type=Path, default=Path("build/modernize-compat"))
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--hash-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = args.out if args.out.is_absolute() else root / args.out
    out.mkdir(parents=True, exist_ok=True)
    report = {"schema": 1, "relocation": relocation_checks(root), "runs": [],
              "limits": ["Does not prove whole-module or gameplay parity",
                         "Host vector test checks WASM ABI packing, not a WASM host",
                         "Fixed-grid cases run only under their original MSVC/x86 guard"]}
    failed = bool(report["relocation"]["failures"])
    if not args.hash_only:
        compiler = None if args.compiler == "auto" else shutil.which(args.compiler)
        env = os.environ.copy()
        if args.compiler != "auto" and not compiler:
            if Path(args.compiler).is_file():
                compiler = str(Path(args.compiler).resolve())
            else:
                raise RuntimeError("Requested compiler not found: " + args.compiler)
        if compiler is None:
            compiler = shutil.which("clang") or shutil.which("gcc") or shutil.which("cl")
        if compiler is None or Path(compiler).name.lower() in ("cl", "cl.exe"):
            if os.name == "nt":
                env, detected = msvc_environment(out, args.msvc_arch, args.msvc_version)
                compiler = compiler or detected
        if compiler is None:
            raise RuntimeError("No supported C compiler available")
        report["compiler"] = compiler
        msvc = Path(compiler).name.lower() in ("cl", "cl.exe")
        fixture = out / "directory-fixture"
        fixture.mkdir(exist_ok=True)
        (fixture / "first.lua").write_text("return true\n", encoding="utf-8")
        (fixture / "second.txt").write_text("fixture\n", encoding="utf-8")
        (fixture / "nested").mkdir(exist_ok=True)
        configurations = ["Debug", "Release"] if args.configuration == "both" else [args.configuration]
        source = root / "tools/modernize/compat_golden.c"
        for configuration in configurations:
            for vector in (False, True):
                name = configuration.lower() + ("-host-vector" if vector else "-native")
                target = out / (name + (".exe" if os.name == "nt" else ""))
                define = ["TCE_COMPAT_TEST_WASM_ABI=1"] if vector else []
                if msvc:
                    command = [compiler, "/nologo", "/TC", "/std:c11", "/W3", "/fp:strict",
                               "/Od" if configuration == "Debug" else "/O2",
                               "/I" + str(root), "/Fo:" + str(out / (name + ".obj")),
                               "/Fe:" + str(target), *["/D" + d for d in define], str(source)]
                    if args.sanitize:
                        command.insert(1, "/fsanitize=address")
                else:
                    command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Wno-unused-function",
                               "-O0" if configuration == "Debug" else "-O2", "-g",
                               "-I", str(root), *["-D" + d for d in define], str(source), "-o", str(target)]
                    if args.sanitize:
                        command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                result = subprocess.run(command, cwd=out, env=env, capture_output=True,
                                        text=True, errors="replace")
                (out / (name + ".build.log")).write_text(result.stdout + result.stderr, encoding="utf-8")
                run = {"name": name, "build_returncode": result.returncode,
                       "command": command, "sanitizer_requested": args.sanitize}
                if result.returncode == 0:
                    executed = subprocess.run([str(target), str(fixture)], cwd=out, env=env,
                                              capture_output=True, text=True, errors="replace")
                    run.update(returncode=executed.returncode, stdout=executed.stdout,
                               stderr=executed.stderr)
                    failed |= executed.returncode != 0
                else:
                    failed = True
                report["runs"].append(run)
                print(json.dumps({k: v for k, v in run.items() if k != "command"}, ensure_ascii=False))
    report["passed"] = not failed
    report_path = out / ("compat-hashes.json" if args.hash_only else "compat-validation.json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": not failed, "hash_files": report["relocation"]["files"],
                      "report": str(report_path)}))
    return int(failed)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(2)
