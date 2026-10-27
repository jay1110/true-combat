#!/usr/bin/env python3
"""Compile focused real weapon/objective functions with deterministic services.

This checks state transitions, not engine networking, BSP glass or live gameplay.
Use --source-root for an archived source tree to demonstrate the old failures.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/modernize"))
from compat_validate import msvc_environment


def function(source: str, name: str) -> str:
    match = re.search(r"^(?:static )?[^\n;{}]*\b" + re.escape(name)
                      + r"\([^;{}]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError("Function not found: " + name)
    start = source.index("{", match.start())
    # Erase strings/comments while preserving positions before counting braces.
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: " " * len(m.group()), source, flags=re.S)
    depth = 1
    end = start + 1
    while depth:
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return source[match.start():end]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/weapon-regression")
    parser.add_argument("--arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--configuration", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--msvc-version")
    parser.add_argument("--compiler", choices=["auto", "gcc", "clang"], default="auto")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source_root = args.source_root.resolve()
    sources = {}
    for relative in ("src/game/bg_pmove.c", "src/game/bg_misc.c", "src/game/g_items.c", "src/cgame/cg_weapons.c", "src/cgame/cg_view.c"):
        path = source_root / relative
        if not path.exists():
            path = source_root / Path(relative).name
        sources[relative] = path.read_text(encoding="utf-8-sig")
    movement_names = ["PM_TCEAddWeaponCounter", "PM_TCEElapsedWeaponCounter", "PM_BeginWeaponChange", "PM_FinishWeaponChange", "PM_SwitchIfEmpty", "PM_TCEWeaponEligibility837",
                      "PM_TCEUpdateWeaponCycle", "PM_TCEUpdateObjectiveAction",
                      "PM_TCEUpdateWeaponActions", "PM_TCEUpdateWeaponTrigger"]
    if "static qboolean PM_TCECancelInterruptedObjective(" in sources["src/game/bg_pmove.c"]:
        movement_names.insert(movement_names.index("PM_TCEUpdateObjectiveAction"),
                              "PM_TCECancelInterruptedObjective")
    units = [function(sources["src/game/bg_pmove.c"], name) for name in movement_names]
    if "PM_TCECancelInterruptedObjective" not in movement_names:
        units.insert(0, "static qboolean PM_TCECancelInterruptedObjective(void) { return qfalse; }")
    # Exercise the actual caller's early-exit hook, including before snapshots.
    main_weapon = function(sources["src/game/bg_pmove.c"], "PM_Weapon")
    cycle_start = main_weapon.index("if (PM_TCEUpdateWeaponCycle(")
    cycle_end = main_weapon.index("if (PM_TCEUpdateWeaponActions(", cycle_start)
    caller_cancels = "PM_TCECancelInterruptedObjective();" in main_weapon[cycle_start:cycle_end]
    units.append(function(sources["src/game/g_items.c"], "G_TCEReleaseObjectives"))
    if "qboolean G_TCEManualDropBomb(" in sources["src/game/g_items.c"]:
        units.append(function(sources["src/game/g_items.c"], "G_TCEManualDropBomb"))
    else:
        units.append("qboolean G_TCEManualDropBomb(gentity_t *ent) { return qfalse; }")
    cg_names = ["CG_WeaponHasAmmo", "CG_WeaponSelectable", "CG_WeaponIndex", "CG_OutOfAmmoChange"]
    units += [function(sources["src/cgame/cg_weapons.c"], name) for name in cg_names]
    banks = re.search(r"int weapBanksMultiPlayer\[MAX_WEAP_BANKS_MP\]\[MAX_WEAPS_IN_BANK_MP\] =\s*\{.*?\n\};",
                      sources["src/game/bg_misc.c"], re.S).group(0)
    banks = banks.replace("int weapBanksMultiPlayer", "static const int actualWeaponBanks", 1)
    view = sources["src/cgame/cg_view.c"]
    start = view.index("#if defined(__linux__)\n\tif (TCE_SubInt32(cg.time, cg.tceAimSyncTime) > 750)")
    end = view.index("\tCG_Zoom();", start)
    units.append("static void reconcileAim(void) { const playerState_t *aimState = &cg.predictedPlayerState;\n"
                 + view[start:end] + "}\n")
    fixture = (ROOT / "tools/modernize/weapon_regression_fixture.c").read_text(encoding="utf-8")
    generated = fixture.replace("/* PRODUCTION_FUNCTIONS */", "\n\n".join(units)).replace("/* ACTUAL_WEAPON_BANKS */",banks)
    generated = ("#define TCE_CALLER_CANCELS_OBJECTIVE " + str(int(caller_cancels)) + "\n" + generated)
    source = out / "weapon_regression.c"
    source.write_text(generated, encoding="utf-8", newline="\n")
    if sys.platform == "win32":
        env, compiler = msvc_environment(out, args.arch, args.msvc_version)
        exe = out / "weapon_regression.exe"
        command = [compiler, "/nologo", "/TC", "/Od" if args.configuration == "Debug" else "/O2", "/W3", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS",
                   "/I" + str(ROOT), str(source), "/Fe:" + str(exe), "/Fo:" + str(out / "weapon_regression.obj")]
        if args.sanitize:
            command.insert(1, "/fsanitize=address")
    else:
        env = None
        compiler = ((shutil.which("clang") or shutil.which("gcc")) if args.compiler == "auto"
                    else shutil.which(args.compiler))
        if not compiler:
            raise RuntimeError("No C compiler available")
        exe = out / "weapon_regression"
        command = [compiler, "-std=c99", "-O0" if args.configuration == "Debug" else "-O2", "-I", str(ROOT), str(source), "-lm", "-o", str(exe)]
        if args.sanitize:
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    compile_result = subprocess.run(command, capture_output=True, text=True, env=env, cwd=out)
    (out / "compile.log").write_text(compile_result.stdout + compile_result.stderr, encoding="utf-8")
    if compile_result.returncode:
        print(compile_result.stdout + compile_result.stderr)
        return compile_result.returncode
    run = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
    report = {"arch": args.arch, "configuration": args.configuration, "sanitizer": args.sanitize,
              "source_root": str(source_root), "compiler": compiler, "command": command,
              "functions": movement_names + ["G_TCEReleaseObjectives", "G_TCEManualDropBomb"] + cg_names + ["CG_CalcFov aim reconciliation"],
              "source_hashes": {key: hashlib.sha256(value.encode()).hexdigest() for key, value in sources.items()},
              "objective_early_exit_caller_hook": caller_cancels,
              "fixture_sha256": hashlib.sha256(fixture.encode()).hexdigest(),
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "generated_sha256": hashlib.sha256(generated.encode()).hexdigest(),
              "unit_hashes": [hashlib.sha256(unit.encode()).hexdigest() for unit in units],
              "stdout": run.stdout, "stderr": run.stderr, "exit_code": run.returncode,
              "limits": ["Deterministic service stubs; no live engine or BSP collision", "Extracts production function bodies without rewriting state decisions"]}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(run.stdout, end="")
    print(run.stderr, end="", file=sys.stderr)
    return run.returncode


if __name__ == "__main__":
    raise SystemExit(main())
