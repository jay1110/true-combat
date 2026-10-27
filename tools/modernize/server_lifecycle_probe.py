#!/usr/bin/env python3
"""Compile and exercise the actual bounded server lifecycle functions.

Engine traps are deterministic fakes; this does not certify a live map cycle.
No original binary patches or game installation are needed.
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
FUNCTIONS = {
    "g_vote.c": ["G_Map_v"],
    "g_session.c": ["G_ReadSessionData"],
    "g_match.c": ["G_deleteStats"],
    "g_character.c": ["G_UpdateCharacter"],
    "g_team.c": ["TeamplayInfoMessage"],
    "tce_bg.c": ["BG_CheckUTWeapon", "TCE_BG_WeaponInWolfMP", "BG_WeaponIsAvailable"],
    "g_client.c": ["TCE_SpawnWeaponAvailable"],
}


def extract(source, name):
    match = re.search(r"^(?:static )?(?:void|int|qboolean) " + re.escape(name) + r"\([^)]*\)\n\{.*?^\}", source, re.M | re.S)
    if not match:
        raise ValueError("Missing complete source function: " + name)
    return match.group(0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/server-lifecycle-probe")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--compiler", choices=("auto", "gcc", "clang"), default="auto",
                        help="Native POSIX compiler; Windows retains the existing MSVC path")
    args = parser.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    parts, hashes = [], {}
    for filename, names in FUNCTIONS.items():
        path = ROOT / "src/game" / filename
        source = path.read_text(encoding="utf8")
        hashes[filename] = hashlib.sha256(path.read_bytes()).hexdigest()
        parts.extend(extract(source, name) for name in names)
    client = (ROOT / "src/game/g_client.c").read_text(encoding="utf8")
    begin = extract(client, "ClientBegin")
    assert begin.index("ClientSpawn(") < begin.index("level.tinfoAxis[0] = '\\0'")
    assert "level.tinfoAllies[0] = '\\0'" in begin
    userinfo = extract(client, "ClientUserinfoChanged")
    assert "G_UpdateCharacter(client);\n\tcharacterIndex = client->pers.characterIndex;" in userinfo
    hashes["g_client.c"] = hashlib.sha256((ROOT / "src/game/g_client.c").read_bytes()).hexdigest()
    for header in ("g_tce_pose.h", "g_local.h", "bg_public.h", "q_shared.h"):
        hashes["src/game/" + header] = hashlib.sha256((ROOT / "src/game" / header).read_bytes()).hexdigest()
    harness = Path(__file__).with_name("server_lifecycle_probe.c").read_text(encoding="utf8")
    generated = args.out / "server_lifecycle_probe.c"
    generated.write_text(harness.replace("/* ACTUAL_FUNCTIONS */", "\n\n".join(parts)), encoding="utf8", newline="\n")
    exe = args.out / "server_lifecycle_probe.exe"
    if os.name != "nt":
        command = [os.environ.get("CC", "cc") if args.compiler == "auto" else args.compiler,
                   "-std=c99", "-O2" if args.configuration == "Release" else "-O0", "-DGAMEDLL", "-I" + str(ROOT),
                   str(generated), str(ROOT / "src/game/q_shared.c"), "-lm", "-o", str(exe)]
        if args.sanitize:
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    else:
        if args.compiler != "auto":
            raise ValueError("--compiler selects a native POSIX compiler; Windows uses MSVC")
        vcvars = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
        batch = args.out / "compile.cmd"
        flags = ("/O2 " if args.configuration == "Release" else "/Od ") + ("/fsanitize=address /Zi" if args.sanitize else "")
        batch.write_text('@echo off\ncall "' + str(vcvars) + '" -vcvars_ver=14.44 >nul\nif errorlevel 1 exit /b %errorlevel%\n'
            + 'cl /nologo /W3 /MD /DWIN32 /DGAMEDLL /D_CRT_SECURE_NO_WARNINGS ' + flags
            + ' /I"' + str(ROOT) + '" "' + str(generated) + '" "' + str(ROOT / 'src/game/q_shared.c')
            + '" /Fe:"' + str(exe) + '"\n', encoding="utf8")
        command = ["cmd", "/d", "/c", str(batch)]
    compiled = subprocess.run(command, cwd=args.out, capture_output=True, text=True)
    (args.out / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf8")
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
        return compiled.returncode
    # ASan's runtime is next to cl.exe; it is not copied into the workspace.
    env = os.environ.copy()
    if os.name == "nt" and args.sanitize:
        runtimes = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*/bin/Hostx64/x64"))
        env["PATH"] = str(runtimes[-1]) + os.pathsep + env.get("PATH", "")
    run = subprocess.run([str(exe)], cwd=args.out, capture_output=True, text=True, env=env)
    result = dict(passed=run.returncode == 0, exit_code=run.returncode, sanitizer=args.sanitize,
                  configuration=args.configuration, platform=sys.platform, command=command,
                  generated_sha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
                  fixture_sha256=hashlib.sha256(Path(__file__).with_name("server_lifecycle_probe.c").read_bytes()).hexdigest(),
                  runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  functions=[name for names in FUNCTIONS.values() for name in names],
                  unit_sha256=[hashlib.sha256(part.encode()).hexdigest() for part in parts],
                  sources=hashes, stdout=run.stdout, stderr=run.stderr,
                  limits=["Engine traps are deterministic fakes; live map transitions are separately required.",
                          "ClientBegin cache invalidation and published character index are source integration checks.",
                          "Pose character-change callback records reset/identity boundaries; its real body is tested by pose_server_regression."])
    (args.out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(run.stdout + run.stderr)
    return run.returncode


if __name__ == "__main__":
    raise SystemExit(main())
