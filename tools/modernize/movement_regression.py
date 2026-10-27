#!/usr/bin/env python3
"""Run semantic contracts on unmodified production movement function bodies.

The collision fixtures are synthetic, not a claim about a particular BSP.
--source-root accepts archived bg_pmove.c/bg_slidemove.c for failing baselines.
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


def function(source: str, name: str, index: int = 0) -> str:
    matches = list(re.finditer(r"^(?:static )?[^\n;{}]*\b" + re.escape(name)
                              + r"\([^;{}]*\)\s*\{", source, re.M))
    if index >= len(matches):
        raise ValueError(f"Function not found: {name}[{index}]")
    match = matches[index]
    start = source.index("{", match.start())
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: " " * len(m.group()), source, flags=re.S)
    depth, end = 1, start + 1
    while depth:
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return source[match.start():end]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/movement-regression")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--arch", choices=["x86", "x64"], default="x86")
    parser.add_argument("--configuration", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--msvc-version", help="Select installed MSVC toolset")
    parser.add_argument("--compiler", default="auto", help="Linux compiler name or path (auto prefers Clang)")
    parser.add_argument("--footsteps", action="store_true", help="Also check audible sprint/lean gait")
    parser.add_argument("--implementation", choices=["target", "native", "portable"], default="target")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sources = {}
    for name in ("bg_pmove.c", "bg_slidemove.c"):
        path = args.source_root / "src/game" / name
        if not path.is_file():
            path = args.source_root / name
        sources[name] = path.read_text(encoding="utf-8-sig")
    move, slide = sources["bg_pmove.c"], sources["bg_slidemove.c"]
    portable = args.implementation == "portable"
    index = (0 if sys.platform.startswith("linux") or args.arch == "x86" else 1) if args.implementation == "target" else int(portable)
    slide_index = int(portable)
    units = [function(slide, name) for name in ("PM_PreserveTimedVelocity", "PM_TCEStepImprovesMove")
             if re.search(r"\b" + name + r"\(", slide)]
    units += [function(move, name, index) for name in
             ("PM_ClipVelocity", "PM_CheckJump", "PM_ClimbSlideMove", "PM_CrashLand")]
    units += [function(move, "PM_ViewHeightInteger"), function(move, "PM_CheckDuck")]
    units += [function(move, "PM_CmdScale", index), function(move, "PM_Friction"),
              function(move, "PM_Accelerate")]
    if index == 0:
        units.append("#if defined(__linux__)\n" + function(move, "PM_AccelerateHeld") + "\n#endif")
    units.append("#define PM_AirMove PM_ActualAirMove\n" + function(move, "PM_AirMove", index)
                 + "\n#undef PM_AirMove")
    units.append(function(move, "PM_WalkMove", index))
    units += [function(move, "PM_GroundTraceMissed", index), function(move, "PM_GroundTrace", index)]
    if args.footsteps:
        units.append(function(move, "PM_TCELadderFootstepForSurface"))
        if "PM_TCEFootstepQuiet(" in move:
            units.append(function(move, "PM_TCEFootstepQuiet"))
        units.append(function(move, "PM_Footsteps", index))
    if "PM_SlideDotProductYXZ(" in slide:
        units.append("#if defined(__linux__)\n" + function(slide, "PM_SlideDotProductYXZ") + "\n#endif")
    # Test the real collision solver separately; a callback recorder controls
    # the raised/normal candidates in the step-controller contracts.
    units += ["#define PM_SlideMove PM_RealSlideMove\n" + function(slide, "PM_SlideMove", slide_index)
              + "\n#undef PM_SlideMove"]
    units += [function(slide, "PM_TCEProneProbe" if portable else "PM_ProneLegProbeNative"),
              function(slide, "PM_SlideMoveProne", slide_index)]
    step = function(slide, "PM_TCEStepSlideMove") if portable else function(slide, "PM_StepSlideMove")
    if portable:
        units.append(step)
        units.append("void PM_StepSlideMove(qboolean gravity) { PM_TCEStepSlideMove(gravity); }")
    else:
        units.append(step)
    source = out / "movement_regression.c"
    template = (ROOT / "tools/modernize/movement_regression_fixture.c").read_text(encoding="utf-8")
    source.write_text(("#define MOVEMENT_TEST_FOOTSTEPS 1\n" if args.footsteps else "") + template.replace("/* PRODUCTION_FUNCTIONS */", "\n\n".join(units)),
                      encoding="utf-8", newline="\n")
    if sys.platform == "win32":
        env, compiler = msvc_environment(out, args.arch, args.msvc_version)
        exe = out / "movement_regression.exe"
        command = [compiler, "/nologo", "/TC", "/Od" if args.configuration == "Debug" else "/O2",
                   "/W3", "/DWIN32", "/DGAMEDLL", "/D_CRT_SECURE_NO_WARNINGS",
                   "/DMOVEMENT_TEST_POINTER_BITS=" + ("32" if args.arch == "x86" else "64"), "/I" + str(ROOT),
                   str(source), str(ROOT / "src/game/q_math.c"), str(ROOT / "src/game/tce_movement.c"),
                   "/Fe:" + str(exe), "/Fo:" + str(out) + "\\"]
        if args.sanitize:
            command.insert(1, "/fsanitize=address")
    else:
        env = None
        compiler = (shutil.which("clang") or shutil.which("gcc")) if args.compiler == "auto" else shutil.which(args.compiler)
        if not compiler:
            raise RuntimeError("No C compiler")
        exe = out / "movement_regression"
        command = [compiler, "-m32" if args.arch == "x86" else "-m64", "-std=c99",
                   "-O0" if args.configuration == "Debug" else "-O2", "-DGAMEDLL",
                   "-DMOVEMENT_TEST_POINTER_BITS=" + ("32" if args.arch == "x86" else "64"),
                   "-I", str(ROOT), str(source), str(ROOT / "src/game/q_math.c"),
                   str(ROOT / "src/game/tce_movement.c"), "-lm", "-o", str(exe)]
        if args.sanitize:
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    compiled = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
        return compiled.returncode
    result = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    report = {"arch": args.arch, "configuration": args.configuration, "sanitize": args.sanitize,
              "implementation": args.implementation, "source_root": str(args.source_root.resolve()),
              "movement_body_index": index, "slide_body_index": slide_index,
              "portable_tc_step_selected": portable,
              "footstep_contract": args.footsteps,
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "fixture_sha256": hashlib.sha256(template.encode()).hexdigest(),
              "generated_source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
              "compiler": compiler, "command": command,
              "source_hashes": {key: hashlib.sha256(value.encode()).hexdigest() for key, value in sources.items()},
              "stdout": result.stdout, "stderr": result.stderr, "exit_code": result.returncode,
              "limits": ["Synthetic collision worlds and recorded candidate responses; no map-specific proof",
                         "Water move and animation services stubbed; ground/air replay uses actual movement functions",
                         "Ground allsolid recovery stubbed as failure; synthetic scenes start outside solids",
                         "Portable run selects the TC gear step path, not the unused SDK dispatcher"]}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
