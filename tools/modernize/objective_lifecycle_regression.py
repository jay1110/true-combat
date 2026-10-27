#!/usr/bin/env python3
"""Exercise real objective registration across real level-reset fragments.

The engine services are deterministic fakes. A live mapvote stress test remains
separate. --source-root may point at an archived g_team.c for a failing baseline.
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
from compat_validate import msvc_environment
from movement_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/objective-lifecycle-regression")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--compiler", default="auto")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    source_path = args.source_root / "src/game/g_team.c"
    if not source_path.is_file(): source_path = args.source_root / "g_team.c"
    team = source_path.read_text(encoding="utf-8-sig")
    main_source = (ROOT / "src/game/g_main.c").read_text(encoding="utf-8-sig")
    init = function(main_source, "G_InitGame")
    clear = re.search(r"memset\(&level,\s*0,\s*sizeof\(level\)\);", init)
    if not clear: raise ValueError("Production G_InitGame no longer clears map-owned level state")
    begin = init.index("trap_GetConfigstring(CS_MULTI_INFO, cs, sizeof(cs));")
    end = init.index("G_ResetTeamMapData();", begin)
    reset = clear[0] + "\n" + init[begin:end]
    static_count = re.search(r"^static int numobjectives\s*=\s*0;.*$", team, re.M)
    functions = (static_count[0] if static_count else "") + "\n" + function(team, "team_wolf_objective_use") + "\n" + function(team, "objective_Register")
    template_path = Path(__file__).with_name("objective_lifecycle_regression_fixture.c")
    generated = template_path.read_text(encoding="utf-8").replace("/* PRODUCTION_FUNCTIONS */", functions).replace("/* PRODUCTION_MAP_RESET */", reset)
    source = out / "objective_lifecycle_regression.c"; source.write_text(generated, encoding="utf-8", newline="\n")
    if os.name == "nt":
        env, compiler = msvc_environment(out, "x64", "14.44")
        exe = out / "objective_lifecycle_regression.exe"
        command = [compiler, "/nologo", "/TC", "/O2", "/W3", "/DWIN32", "/DGAMEDLL", "/D_CRT_SECURE_NO_WARNINGS", "/I"+str(ROOT), str(source), str(ROOT / "src/game/q_shared.c"), "/Fe:"+str(exe), "/Fo:"+str(out)+"\\"]
        if args.sanitize: command.insert(1, "/fsanitize=address")
    else:
        env = None
        compiler = (shutil.which("clang") or shutil.which("gcc")) if args.compiler == "auto" else shutil.which(args.compiler)
        if not compiler: raise RuntimeError("No C compiler")
        exe = out / "objective_lifecycle_regression"
        command = [compiler, "-std=c99", "-O2", "-DGAMEDLL", "-I"+str(ROOT), str(source), str(ROOT / "src/game/q_shared.c"), "-lm", "-o", str(exe)]
        if args.sanitize: command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    compiled = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / "compile.log").write_text(compiled.stdout+compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stdout+compiled.stderr); return compiled.returncode
    results = []
    for mode in ("--cycle", "--overflow"):
        run = subprocess.run([str(exe), mode], cwd=out, env=env, capture_output=True, text=True)
        results.append({"mode":mode,"exit_code":run.returncode,"stdout":run.stdout,"stderr":run.stderr})
        print(run.stdout, end=""); print(run.stderr, end="")
    report = {"source_path":str(source_path.resolve()), "compiler":compiler,"command":command,
              "source_sha256":hashlib.sha256(team.encode()).hexdigest(),
              "production_functions_sha256":hashlib.sha256(functions.encode()).hexdigest(),
              "production_map_reset_sha256":hashlib.sha256(reset.encode()).hexdigest(),
              "fixture_sha256":hashlib.sha256(template_path.read_bytes()).hexdigest(),
              "generated_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),
              "runs":results,
              "limits":["Executes actual registration functions and actual level/configstring reset statements, not all G_InitGame services", "Synthetic configstrings/entities; live mapvote cycling remains separate"]}
    (out / "report.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    return int(any(result["exit_code"] for result in results))


if __name__ == "__main__": raise SystemExit(main())
