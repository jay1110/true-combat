#!/usr/bin/env python3
"""Collect Clang unused/unreachable warnings without generating object files.

This is a Windows/MSVC-profile observation, not a whole-program dead-code proof.
It preserves each configured module's preprocessor definitions and includes.
Compiler diagnostics never authorize removal of platform or optional code.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-dir", type=Path, default=ROOT / "build/sdk-win32")
    parser.add_argument("--zig", type=Path, default=ROOT / "build/linux-toolchain/ziglang/zig.exe")
    parser.add_argument("--msvc-version", default="14.44")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(ROOT / "tools/modernize"))
    from compat_validate import msvc_environment
    env, _ = msvc_environment(args.out, "x86", args.msvc_version)
    jobs = []
    projects = []
    for target in ("qagame", "cgame", "ui"):
        path = args.project_dir / (target + ".vcxproj")
        tree = ET.parse(path)
        group = next(group for group in tree.findall("m:ItemDefinitionGroup", NS)
                     if "Release|" in group.attrib.get("Condition", ""))
        compile_node = group.find("m:ClCompile", NS)
        definitions = compile_node.findtext("m:PreprocessorDefinitions", default="", namespaces=NS)
        includes = compile_node.findtext("m:AdditionalIncludeDirectories", default="", namespaces=NS)
        flags = ["-D" + item for item in definitions.split(";")
                 if item and "%(" not in item and "CMAKE_INTDIR=" not in item]
        flags += ["-I" + item for item in includes.split(";") if item and "%(" not in item]
        projects.append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                         "definitions": definitions, "includes": includes})
        for node in tree.findall(".//m:ClCompile", NS):
            if "Include" not in node.attrib:
                continue
            source = Path(node.attrib["Include"])
            if source.suffix.lower() != ".c" or not source.is_relative_to(ROOT / "src"):
                continue
            jobs.append((target, source, flags))
    # The packaged Zig exposes its embedded Clang driver through `clang`.
    # Unlike `zig cc -fsyntax-only`, this executes only Clang syntax analysis.
    prefix = [str(args.zig.resolve()), "clang", "--target=i686-pc-windows-msvc",
              "-fms-extensions", "-fsyntax-only", "-fno-color-diagnostics",
              "-isystem", str(args.zig.resolve().parent / "lib/include"),
              "-Wunused-function", "-Wunused-variable", "-Wunreachable-code",
              "-fdiagnostics-show-option"]

    def run(item):
        target, source, flags = item
        command = [*prefix, *flags, "-I" + str(ROOT), str(source)]
        result = subprocess.run(command, cwd=args.out, env=env, capture_output=True,
                                text=True, errors="replace", timeout=120)
        log = result.stdout + result.stderr
        name = target + "-" + source.stem
        (args.out / (name + ".log")).write_text(log, encoding="utf-8")
        diagnostics = re.findall(r"(?m)^.*?:\d+:\d+: (?:warning|error): .*?$", log)
        return {"target": target, "source": str(source.relative_to(ROOT)),
                "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "command": command, "returncode": result.returncode,
                "diagnostics": diagnostics, "log": str(args.out / (name + ".log"))}

    with ThreadPoolExecutor(max_workers=max(1, min(4, args.jobs))) as executor:
        results = list(executor.map(run, jobs))
    version = subprocess.run([str(args.zig.resolve()), "clang", "--version"],
                             capture_output=True, text=True).stdout
    report = {"profile": "i686-pc-windows-msvc configured Release definitions; syntax only",
              "compiler": version, "projects": projects, "runs": results,
              "failures": sum(item["returncode"] != 0 for item in results),
              "limits": ["An unused static function in one preprocessor profile may be used by another target.",
                         "No external/VM/callback/scripting entry is declared dead by this tool.",
                         "No sources are changed and no object/link output is generated."]}
    destination = args.out / "clang-unused.json"
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(results)} syntax runs, {report['failures']} parse failures; {destination}")
    return 1 if report["failures"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
