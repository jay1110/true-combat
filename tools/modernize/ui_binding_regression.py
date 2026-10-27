#!/usr/bin/env python3
"""Exercise actual UI binding table/load/default/save/capture against fake key storage."""
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
    parser.add_argument("--out", type=Path, default=ROOT / "build/ui-binding-regression")
    parser.add_argument("--source", type=Path, default=ROOT / "src/ui/ui_shared.c")
    parser.add_argument("--arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    shared = args.source.read_text(encoding="utf-8-sig")
    typedef = re.search(r"typedef struct\s*\{\s*char \*command;.*?\}\s*bind_t;", shared, re.S)
    start = shared.index("static bind_t g_bindings[] =")
    end = shared.index(";", shared.index("static const int g_bindCount", start))+1
    bindings = typedef[0] + "\n" + shared[start:end]
    menu_path = ROOT / "src/assets/ui/options_controls.menu"
    menu = re.sub(r'//[^\n]*|/\*.*?\*/', '', menu_path.read_text(encoding="utf-8"), flags=re.S)
    menu_commands = re.findall(r'BIND\([^\n]*,\s*"([^"]+)",\s*"[^"]*"\s*\)', menu)
    if not {"+wstats", "commandmap", "+mapexpand", "mapzoomin", "mapzoomout"}.issubset(menu_commands):
        raise ValueError("Required stats/map controls are missing from the active menu")
    bindings += "\nstatic const char *menuBindings[] = {" + ",".join(json.dumps(c) for c in menu_commands) + "};\n"
    names = ["Controls_GetConfig", "Controls_SetConfig", "Controls_SetDefaults", "BindingIDFromName", "BindingFromName", "Item_Bind_HandleKey"]
    bodies = "\n\n".join(function(shared, name) for name in names)
    fixture = Path(__file__).with_name("ui_binding_regression_fixture.c")
    source = out / "ui_binding_regression.c"
    source.write_text(fixture.read_text(encoding="utf-8").replace("/* PRODUCTION_BINDINGS */", bindings).replace("/* PRODUCTION_FUNCTIONS */", bodies), encoding="utf-8", newline="\n")
    if os.name == "nt":
        env, compiler = msvc_environment(out, args.arch, "14.44")
        exe = out / "ui_binding_regression.exe"
        cmd = [compiler, "/nologo", "/TC", "/O2", "/W3", "/DWIN32", "/DUIDLL", "/D_CRT_SECURE_NO_WARNINGS", "/I"+str(ROOT), str(source), str(ROOT / "src/game/q_shared.c"), "/Fe:"+str(exe), "/Fo:"+str(out)+"\\"]
        if args.sanitize: cmd.insert(1, "/fsanitize=address")
    else:
        env = None; compiler = shutil.which("clang") or shutil.which("gcc")
        exe = out / "ui_binding_regression"
        cmd = [compiler, "-std=c99", "-O2", "-DUIDLL", "-I"+str(ROOT), str(source), str(ROOT / "src/game/q_shared.c"), "-lm", "-o", str(exe)]
        if args.sanitize: cmd[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    built = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)
    (out / "compile.log").write_text(built.stdout+built.stderr, encoding="utf-8")
    if built.returncode: print(built.stdout+built.stderr); return built.returncode
    run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    report = {"source":str(args.source.resolve()), "bindings_sha256":hashlib.sha256(bindings.encode()).hexdigest(), "functions_sha256":{n:hashlib.sha256(function(shared,n).encode()).hexdigest() for n in names},"fixture_sha256":hashlib.sha256(fixture.read_bytes()).hexdigest(), "compiler":compiler,"command":cmd,"stdout":run.stdout,"stderr":run.stderr,"exit_code":run.returncode,"limits":["Actual UI table and binding functions; engine key storage and rectangle predicate faked", "No live input/rendering or disk persistence claim"]}
    (out / "report.json").write_text(json.dumps(report,indent=2)+"\n", encoding="utf-8")
    print(run.stdout, end=""); print(run.stderr, end="")
    return run.returncode


if __name__ == "__main__": raise SystemExit(main())
