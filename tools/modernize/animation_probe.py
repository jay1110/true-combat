#!/usr/bin/env python3
"""Replay actual original/port Win32 animation controllers and real asset parsers.

No game is started, and neither DLL is patched. The probe substitutes only
bounded filesystem/token/model-registration engine imports for common asset
fixtures. Real model geometry, animation-script interpretation and engine
rendering remain outside this deterministic test.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zipfile

from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL_SHA256 = "d85a8e94da7c94956eff560e4cc406b4116f5a6ffd820b59accd9ad7e1e2414c"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_map(binary: Path, mapping: Path) -> None:
    data = binary.read_bytes()
    pe = struct.unpack_from("<I", data, 60)[0]
    if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0" or struct.unpack_from("<H", data, pe + 4)[0] != 0x14c:
        raise ValueError("Animation probe requires a Win32 PE cgame DLL")
    timestamp = struct.unpack_from("<I", data, pe + 8)[0]
    preferred = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    text = mapping.read_text(encoding="utf-8", errors="replace")
    map_timestamp = re.search(r"Timestamp is ([0-9a-fA-F]+)", text)
    map_preferred = re.search(r"Preferred load address is ([0-9a-fA-F]+)", text)
    if not map_timestamp or int(map_timestamp[1], 16) != timestamp:
        raise ValueError("Port DLL and symbol map timestamps differ")
    if not map_preferred or int(map_preferred[1], 16) != preferred:
        raise ValueError("Port DLL and symbol map preferred image bases differ")


def fixtures(base: Path, mod: Path, out: Path) -> tuple[list[Path], list[dict]]:
    archives = []
    sources = {}
    mod_weapons = set()
    evidence = []
    try:
        # Same logical base-then-mod precedence; the actual game VFS is not run.
        for folder in (base, mod):
            for path in sorted(folder.glob("*.pk3")):
                archive = zipfile.ZipFile(path)
                archives.append(archive)
                for name in archive.namelist():
                    if not name.endswith("/"):
                        sources[name] = (archive, path)
                        if folder == mod and name.lower().endswith("/weapon.cfg"):
                            mod_weapons.add(name)

        def read(name: str) -> bytes:
            archive, path = sources[name]
            data = archive.read(name)
            evidence.append({"pk3": str(path), "entry": name,
                             "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
            return data

        def expand(name: str, ancestry: tuple[str, ...] = ()) -> bytes:
            if name in ancestry or len(ancestry) >= 32:
                raise ValueError("Recursive animation include: " + name)
            return re.sub(rb'#include\s+"([^"]+)"',
                          lambda m: expand(m[1].decode("ascii"), ancestry + (name,)), read(name))

        result = []
        for index, name in enumerate(sorted(mod_weapons)):
            data = read(name)
            if len(data) >= 19999:
                raise ValueError("Weapon fixture exceeds original parser capacity: " + name)
            path = out / f"weapon-{index:02d}.cfg"
            path.write_bytes(data)
            result.append(path)
        data = expand("animations/human_base.anim")
        if len(data) >= 19999:
            raise ValueError("Expanded body group exceeds bounded probe fixture capacity")
        path = out / "human-expanded.anim"
        path.write_bytes(data)
        result.append(path)
        return result, evidence
    finally:
        for archive in archives:
            archive.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "tcetest/cgame_mp_x86.dll")
    parser.add_argument("--port", type=Path, default=ROOT / "build/modernization-validation/windows-x86/bin/Release/cgame_mp_x86.dll")
    parser.add_argument("--map", type=Path, default=ROOT / "build/modernization-validation/windows-x86/cgame-symbols-Release.map")
    parser.add_argument("--base-assets", type=Path, default=ROOT / "etmain")
    parser.add_argument("--mod-assets", type=Path, default=ROOT / "tcetest")
    parser.add_argument("--msvc-version", default="14.44")
    parser.add_argument("--out", type=Path, default=ROOT / "build/runtime-parity-animation-probe")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if digest(args.original) != ORIGINAL_SHA256:
        raise ValueError("Original DLL hash differs from the binary authenticated for hardcoded function/global RVAs")
    verify_map(args.port, args.map)
    env, compiler = msvc_environment(out, "x86", args.msvc_version)
    inputs, assets = fixtures(args.base_assets, args.mod_assets, out)
    source = ROOT / "src/tests/tce_player_animation_runtime_test.c"
    executable = out / "player-animation-runtime.exe"
    command = [compiler, "/nologo", "/TC", "/O2", "/DWIN32", "/DCGAMEDLL",
               "/D_CRT_SECURE_NO_WARNINGS", "/D_CRT_NONSTDC_NO_DEPRECATE", "/I" + str(ROOT),
               "/Fo:" + str(out / "player-animation-runtime.obj"), "/Fe:" + str(executable), str(source)]
    before = digest(args.port)
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True, errors="replace")
    (out / "build.log").write_text(built.stdout + built.stderr, encoding="utf-8")
    report = {"schema": 1, "compiler": compiler, "command": command,
              "original_sha256": ORIGINAL_SHA256, "port_sha256": before,
              "map_sha256": digest(args.map), "harness_sha256": digest(source),
              "assets": assets, "build_returncode": built.returncode,
              "limits": ["No game/renderer is started; model registration returns deterministic fixture handles",
                         "Includes are expanded into a common token fixture; the actual engine preprocessor is not tested",
                         "Body frame/model integration covers parsed animation data and non-shuffle transitions",
                         "Animation-script selection and visible geometry require separate runtime validation",
                         "Controller float differences are reported without granting exact parity"]}
    code = built.returncode
    if not code:
        run_command = [str(executable), str(args.original.resolve()), str(args.port.resolve()),
                       str(args.map.resolve()), *[str(path) for path in inputs]]
        result = subprocess.run(run_command, cwd=out, env=env, capture_output=True, text=True,
                                errors="replace", timeout=60)
        report.update(run_command=run_command, returncode=result.returncode,
                      stdout=result.stdout, stderr=result.stderr)
        (out / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        code = result.returncode
        if digest(args.port) != before:
            report["concurrent_binary_change"] = True
            code = 2
    report["passed_exact"] = code == 0
    (out / "animation-probe.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed_exact": code == 0, "build_returncode": built.returncode,
                      "returncode": report.get("returncode"), "report": str(out / "animation-probe.json")}))
    if report.get("stdout"):
        print(report["stdout"], end="")
    return 0 if code == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(2)
