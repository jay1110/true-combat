#!/usr/bin/env python3
"""Run production rewind and TC damage-volume semantic contracts in small units.

The Windows host executes Windows and Linux source selections; the latter uses
MinGW GCC with 80-bit long double and does not claim Linux engine/ABI validation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/modernize"))
from compat_validate import msvc_environment
from weapon_regression import function


def reference_checks(out, env, compiler, dll):
    """Build the two existing opt-in fixtures without the full game target."""
    reference_out = out / "reference"
    reference_out.mkdir(exist_ok=True)
    support = ['#include "src/game/g_local.h"', '#include <stdio.h>', '#include <stdlib.h>',
               'gentity_t g_entities[MAX_GENTITIES]; level_locals_t level; vmCvar_t g_antilag;',
               'static void unexpected(void) { fprintf(stderr, "Unexpected unrelated engine service in focused reference fixture\\n"); abort(); }',
               'void VectorNormalizeFast(vec3_t v) { (void)v; unexpected(); }',
               'void LerpPosition(vec3_t a, vec3_t b, float f, vec3_t v) { (void)a; (void)b; (void)f; (void)v; unexpected(); }',
               'void G_FreeEntity(gentity_t *e) { (void)e; unexpected(); }',
               'gentity_t *G_BuildHead(gentity_t *e) { (void)e; unexpected(); return NULL; }',
               'gentity_t *G_BuildLeg(gentity_t *e) { (void)e; unexpected(); return NULL; }',
               'void trap_Cvar_VariableStringBuffer(const char *n, char *b, int s) { (void)n; (void)b; (void)s; unexpected(); }',
               'void trap_Trace(trace_t *t, const vec3_t s, const vec3_t mi, const vec3_t ma, const vec3_t e, int p, int m) { (void)t; (void)s; (void)mi; (void)ma; (void)e; (void)p; (void)m; unexpected(); }',
               '__declspec(noinline) int trap_Milliseconds(void) { return level.time; }',
               '__declspec(noinline) void trap_LinkEntity(gentity_t *ent) { ent->r.linked = qtrue; }',
               '/* Pose acquisition is not initialized in these original-DLL contracts. */',
               'void G_TCEPoseCapture(gentity_t *ent) { (void)ent; }',
               'void G_TCEPoseResetClient(gentity_t *ent) { (void)ent; }',
               'void G_TCEPoseResetHistory(gentity_t *ent) { (void)ent; }',
               'static volatile float referenceTouch;',
               '__declspec(noinline) gentity_t *G_TempEntity(vec3_t p, int e) { referenceTouch = p[0] + p[1] + p[2] + e; return &g_entities[70]; }']
    shared = (ROOT / "src/game/q_shared.c").read_text(encoding="utf-8-sig")
    weapons = (ROOT / "src/game/g_weapon.c").read_text(encoding="utf-8-sig")
    support += [function(shared, name) for name in ("Q_stricmpn", "Q_stricmp")]
    support += [function(weapons, name) for name in ("SnapVectorTowards", "EmitterCheck")]
    source = reference_out / "reference_services.c"
    source.write_text("\n\n".join(support), encoding="utf-8", newline="\n")
    results = []
    for name in ("tce_marker_test", "tce_antilag_pose_test"):
        executable = reference_out / (name + ".exe")
        command = [compiler, "/nologo", "/TC", "/W3", "/O2", "/Ob0", "/Gy",
                   "/DWIN32", "/DGAMEDLL", "/D_CRT_SECURE_NO_WARNINGS",
                   "/I" + str(ROOT), "/I" + str(ROOT / "src/game"),
                   str(ROOT / "src/tests" / (name + ".c")), str(source)]
        if name == "tce_marker_test":
            command.append(str(ROOT / "src/game/g_antilag.c"))
        command += ["/Fe:" + str(executable), "/Fo:" + str(reference_out) + os.sep,
                    "/link", "/OPT:REF", "/OPT:NOICF"]
        compiled = subprocess.run(command, cwd=reference_out, env=env, capture_output=True, text=True)
        (reference_out / (name + ".compile.log")).write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
        result = {"name": name, "command": command, "build_exit_code": compiled.returncode}
        if not compiled.returncode:
            run = subprocess.run([str(executable), str(dll)], cwd=reference_out, env=env, capture_output=True, text=True)
            result.update(exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr)
            print(run.stdout + run.stderr, end="")
        else:
            print(compiled.stdout + compiled.stderr)
        results.append(result)
    report = {"reference_dll": str(dll), "reference_sha256": hashlib.sha256(dll.read_bytes()).hexdigest(),
              "fixtures": results, "limits": ["Historical reference addresses require the original Win32 TC:E DLL.",
              "Semantic rewind checks deliberately replace equality with known original bugs; marker prefixes, slot selection, and emitter comparisons remain original-DLL gates."]}
    (reference_out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return any(result.get("exit_code", result["build_exit_code"]) for result in results)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/hitbox-regression")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--path", choices=["windows", "linux"], default="windows")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--optimized", action="store_true")
    parser.add_argument("--reference-dll", type=Path, help="Also run the migrated opt-in Win32 marker/pose fixtures")
    parser.add_argument("--bullets", action="store_true", help="Run actual full bullet traversal and G_Damage integration")
    parser.add_argument("--compiler", help="Override GCC for the Linux source path (native Linux defaults to gcc)")
    args = parser.parse_args()
    if args.sanitize and args.path == "linux" and os.name == "nt":
        parser.error("Linux ASan requires a native Linux host; Windows ASan uses the MSVC source path")
    if args.reference_dll and (args.path != "windows" or args.sanitize):
        parser.error("--reference-dll requires the Windows x86 path without --sanitize")
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    sources = {}
    names = ["g_antilag.c", "tce_damage_controller.inc", "tce_damage.h", "tce_damage_trace.h", "g_match.c"]
    if args.bullets:
        names += ["g_tce_bullet_damage.h", "g_tce_pose.h", "g_tce_pose_presentation.h", "g_weapon.c", "g_combat.c", "tce_bg.c", "tce_bullet.c", "tce_bullet_material.c", "q_math.c", "q_shared.c"]
    for name in names:
        path = args.source_root / "src/game" / name
        if not path.exists(): path = args.source_root / name
        if not path.exists(): path = ROOT / "src/game" / name
        sources[name] = path.read_text(encoding="utf-8-sig")
    anti = sources["g_antilag.c"]
    units = [function(anti, "G_CaptureClientMarker")]
    for helper in ("G_BackupClientRewind", "G_LerpClientAngles"):
        if re.search(r"\b" + helper + r"\([^;{}]*\)\s*\{", anti): units.append(function(anti, helper))
    units += ["#if !defined(__linux__)\n" + function(anti, "G_TCELerpPose") +
              "\n#else\n" + function(anti, "G_TCELerpLinuxPose") + "\n#endif"]
    units += [function(anti, name) for name in ("G_ApplyClientMarker", "G_TimeShiftClient", "G_UnTimeShiftClient", "G_TimeShiftAllClients", "G_UnTimeShiftAllClients")]
    damage = sources["tce_damage_controller.inc"]
    units += [function(damage, "TCE_DamageDebugBox"), function(damage, "TCE_ApplyBodyDamage")]
    stats = sources["g_match.c"]
    units.append(re.search(r"static const struct \{ meansOfDeath_t.*?aWeapMOD\[MOD_NUM_MODS\].*?^\};", stats, re.M | re.S).group(0))
    units += [function(stats, "G_weapStatIndex_MOD"), function(stats, "G_addStatsHeadShot")]
    if args.bullets:
        units += [function(sources["q_shared.c"], name) for name in ("Q_stricmpn", "Q_stricmp")]
        units += [function(sources["g_combat.c"], "IsHeadShotWeapon")]
        if "TCE_DamageController(" in damage:
            if "TCE_PresentationSelectionValid(" in damage:
                units += [function(damage, "TCE_PresentationSelectionValid")]
            units += [function(damage, name) for name in ("TCE_ApplyPresentationDamage", "TCE_DamageController", "G_Damage", "G_TCEPresentationDamage")]
        else:
            units += [function(damage, "G_Damage")]
        units += [re.search(r"const tce_pierceMaterial_t tcePierceTable\[37\] =\s*\{.*?\n\};", sources["tce_bg.c"], re.S)[0],
                  function(sources["tce_bg.c"], "BG_SurfaceFlag2Type"), function(sources["tce_bullet.c"], "TCE_BulletClientDamage")]
        units += [function(sources["q_math.c"], name) for name in ("Q_fabs", "ProjectPointOnPlane", "PerpendicularVector", "CrossProduct", "Q_rand", "Q_random", "Q_crandom")]
        units += [function(sources["tce_bullet_material.c"], "TCE_BulletDeflect")]
        weapons = sources["g_weapon.c"]
        units += [function(weapons, "SnapVectorTowards")]
        if "static int G_TCEBulletExplosiveDamage(" in weapons:
            units += ["#if defined(__linux__)\n" + function(weapons, "G_TCEBulletExplosiveDamage") + "\n#endif"]
        if "static void G_TCETraceBullet(" in weapons:
            units += [re.search(r"typedef struct\s*\{\s*qboolean hit\[MAX_CLIENTS\];.*?\} tceBulletTraceContext_t;", weapons, re.S)[0],
                      function(weapons, "G_TCETraceBullet"), function(weapons, "G_TCEBulletFireExtended")]
        units += [function(weapons, "Bullet_Fire_Extended")]
    fixture = Path(__file__).with_name("hitbox_bullet_fixture.c" if args.bullets else "hitbox_regression_fixture.c").read_text()
    # Use the requested historical volume implementation as well as function bodies.
    fixture = fixture.replace('#include "src/game/tce_damage.h"', sources["tce_damage.h"])
    fixture = fixture.replace('#include "src/game/tce_damage_trace.h"', sources["tce_damage_trace.h"])
    fixture = fixture.replace("/* PRODUCTION_FUNCTIONS */", "\n\n".join(units))
    if "G_addStatsHeadShot" in function(damage, "TCE_ApplyBodyDamage"):
        fixture = "#define HAS_BODY_STATS 1\n" + fixture
    if args.bullets and "TCE_DamageController(" in damage:
        fixture = "#define HAS_TCE_PRESENTATION_DAMAGE 1\n" + fixture
    generated = out / "hitbox_regression.c"; generated.write_text(fixture, encoding="utf-8", newline="\n")
    exe = out / "hitbox_regression.exe"
    flags = []
    if args.path == "windows":
        env, cc = msvc_environment(out, "x86" if not args.sanitize else "x64", "14.44" if args.sanitize else "14.39")
        command = [cc, "/nologo", "/TC", "/W3", "/O2" if args.optimized else "/Od", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(ROOT), "/I" + str(ROOT / "src/game"), str(generated), "/Fe:" + str(exe), "/Fo:" + str(out / "fixture.obj")]
        command += ["/D" + flag[2:] for flag in flags]
        if args.sanitize:
            command[1:1] = ["/fsanitize=address", "/Zi", "/Fd:" + str(out / "fixture.pdb")]
            command += ["/link", "/DEBUG"]
    else:
        env = os.environ.copy(); cc = args.compiler or ("C:/msys64/mingw64/bin/gcc.exe" if os.name == "nt" else "gcc")
        if Path(cc).parent != Path("."):
            env["PATH"] = str(Path(cc).parent) + os.pathsep + env.get("PATH", "")
        command = [cc, "-std=c99", "-O2" if args.optimized else "-O0", "-D__linux__", "-DGAMEDLL", "-I" + str(ROOT), "-I" + str(ROOT / "src/game"), str(generated), "-o", str(exe), "-lm"] + flags
        if args.sanitize:
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
    compiled = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr); return compiled.returncode
    run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    normalize = lambda source: re.sub(r"\s+", "", re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S))
    report = {"source_path": args.path, "source_root": str(args.source_root.resolve()),
              "compiler": cc, "optimized": args.optimized, "asan": args.sanitize,
              "source_hashes": {name: hashlib.sha256(text.encode()).hexdigest() for name, text in sources.items()},
              "normalized_unit_hashes": [hashlib.sha256(normalize(unit).encode()).hexdigest() for unit in units],
              "header_sha256": hashlib.sha256((ROOT / "src/game/g_local.h").read_bytes()).hexdigest(),
              "fixture_sha256": hashlib.sha256(Path(__file__).with_name("hitbox_bullet_fixture.c" if args.bullets else "hitbox_regression_fixture.c").read_bytes()).hexdigest(),
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
              "command": command, "exit_code": run.returncode, "stdout": run.stdout, "stderr": run.stderr,
              "limits": ["Deterministic ray/AABB collision services, no BSP/capsule or live-engine proof.",
                         "Linux source selection is native only on a Linux host; Windows GCC runs are not Linux ABI evidence.",
                         "Historical bodies are compiled with current internal test declarations; the archived baseline header is retained separately.",
                         "Unchanged SDK engine-facing structs; internal gclient marker storage is extended."]}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(run.stdout + run.stderr, end="")
    if args.reference_dll and not run.returncode:
        return int(reference_checks(out, env, cc, args.reference_dll.resolve()))
    return run.returncode


if __name__ == "__main__":
    raise SystemExit(main())
