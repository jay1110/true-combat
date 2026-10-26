#!/usr/bin/env python3
"""Measure bounded original/actual port DLL weapon-position CPU costs.

No engine or game is started. These fixture microseconds are not whole-frame
FPS, and do not measure renderer, map geometry or driver costs.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import statistics
import subprocess

from animation_probe import ORIGINAL_SHA256, ROOT, digest, verify_map
from compat_validate import msvc_environment


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "tcetest/cgame_mp_x86.dll")
    parser.add_argument("--port", type=Path, default=ROOT / "build/modernization-validation/windows-x86/bin/Release/cgame_mp_x86.dll")
    parser.add_argument("--map", type=Path, default=ROOT / "build/modernization-validation/windows-x86/cgame-symbols-Release.map")
    parser.add_argument("--out", type=Path, default=ROOT / "build/runtime-parity-render-cost")
    parser.add_argument("--msvc-version", default="14.44")
    parser.add_argument("--full-weapon-checks", action="store_true", help="Run all296960 existing state/history comparisons instead of timing")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if digest(args.original) != ORIGINAL_SHA256:
        raise ValueError("Original identity differs from authenticated native fixture RVAs")
    verify_map(args.port, args.map)
    env, compiler = msvc_environment(out, "x86", args.msvc_version)
    source = ROOT / "src/tests/tce_render_cost_runtime_test.c"
    executable = out / "render-cost-runtime.exe"
    command = [compiler, "/nologo", "/TC", "/O2", "/DWIN32", "/DCGAMEDLL", "/D_CRT_SECURE_NO_WARNINGS",
               "/D_CRT_NONSTDC_NO_DEPRECATE", "/I" + str(ROOT), "/Fo:" + str(out / "render-cost-runtime.obj"),
               "/Fe:" + str(executable), str(source)]
    before = digest(args.port)
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True, errors="replace")
    (out / "build.log").write_text(built.stdout + built.stderr, encoding="utf-8")
    report = {"schema": 1, "command": command, "original_sha256": ORIGINAL_SHA256, "port_sha256": before,
              "full_weapon_checks": args.full_weapon_checks,
              "map_sha256": digest(args.map), "source_sha256": digest(source), "build_returncode": built.returncode,
              "limits": ["No engine, game or driver rendering is run", "Fixture states and advancing 16ms clock only",
                         "Three short repetitions; host contention can affect timings", "Microseconds do not establish a whole-frame bottleneck"]}
    code = built.returncode
    if not code:
        run_command = [str(executable), str(args.original.resolve()), str(args.port.resolve()), str(args.map.resolve())]
        if args.full_weapon_checks:
            run_command.append("--full-checks")
            report["limits"].append("Weapon-position float tolerance is0.0002+abs(expected)*0.00001; integer/state fields exact")
        result = subprocess.run(run_command, cwd=out, env=env, capture_output=True, text=True, errors="replace", timeout=180)
        (out / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        report.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        timings = {}
        for scene, implementation, value in re.findall(r"weapon_position scene=(\d+).*implementation=(\w+).*us_per_call=([0-9.]+)", result.stdout):
            timings.setdefault((int(scene), implementation), []).append(float(value))
        report["median_us_per_call"] = [{"scene": scene, "original": statistics.median(timings[scene, "original"]),
                                         "port": statistics.median(timings[scene, "port"])}
                                        for scene in range(4) if (scene, "original") in timings and (scene, "port") in timings]
        code = result.returncode
        if digest(args.port) != before:
            report["concurrent_binary_change"] = True
            code = 2
    report["completed"] = code == 0
    (out / "render-cost-probe.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"completed": code == 0, "medians": report.get("median_us_per_call"), "report": str(out / "render-cost-probe.json")}))
    return 0 if code == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
