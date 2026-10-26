#!/usr/bin/env python3
"""Read-only source/target and disk inventory; never deletes or changes inputs.

Generated project files describe one configured target, not all possible targets.
An unlisted source is only a review candidate, never proof that its code is dead.
Disk totals are logical bytes, not allocated clusters or deduplicated extents.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = {".c", ".cpp", ".cc", ".cxx", ".asm", ".s"}
INTERMEDIATE_SUFFIXES = {".obj", ".o", ".pch", ".ilk", ".iobj", ".ipdb"}
NS = {"ms": "http://schemas.microsoft.com/developer/msbuild/2003"}


def relative(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cache_features(path: Path) -> dict:
    result = {}
    if path.is_file():
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith(("FEATURE_", "TCE_BUILD_", "CMAKE_HOME_DIRECTORY:")) and "=" in line:
                name, value = line.split("=", 1)
                result[name.split(":", 1)[0]] = value
    return result


def project_inventory(root: Path, project_dirs: list[Path]) -> dict:
    projects = []
    membership = defaultdict(list)
    for directory in project_dirs:
        for target in ("qagame", "cgame", "ui"):
            project = directory / (target + ".vcxproj")
            if not project.is_file():
                continue
            tree = ET.parse(project)
            sources = []
            for node in tree.findall(".//ms:ClCompile", NS):
                if "Include" not in node.attrib:
                    continue
                path = Path(node.attrib["Include"])
                if not path.is_absolute():
                    path = project.parent / path
                sources.append(relative(path, root))
                membership[relative(path, root)].append(relative(project, root))
            cache = directory / "CMakeCache.txt"
            if not cache.is_file():
                cache = directory.parent / "CMakeCache.txt"
            projects.append({"project": relative(project, root), "sha256": sha256(project),
                             "target": target, "sources": sorted(set(sources)),
                             "features": cache_features(cache)})
    source_files = []
    for folder in ("game", "cgame", "ui", "botai"):
        source_files.extend(p for p in (root / "src" / folder).rglob("*")
                            if p.is_file() and p.suffix.lower() in SOURCE_SUFFIXES)
    unlisted = [relative(p, root) for p in source_files if relative(p, root) not in membership]
    missing = sorted(name for name in membership if not (root / name).is_file())
    return {"projects": projects, "source_files": len(source_files),
            "membership": dict(sorted(membership.items())),
            "not_in_observed_projects_review_only": sorted(unlisted),
            "configured_inputs_now_missing": missing,
            "limits": ["No text-reference count is used to declare a function dead.",
                       "One configured project cannot exclude other targets or feature flags.",
                       "VM exports, callback tables, script bindings and platform implementations require separate review."]}


def disk_inventory(root: Path) -> dict:
    top = defaultdict(Counter)
    build = defaultdict(Counter)
    skipped = []
    errors = []
    stack = [root]
    while stack:
        folder = stack.pop()
        try:
            with os.scandir(folder) as entries:
                for entry in entries:
                    try:
                        stat = entry.stat(follow_symlinks=False)
                        # Windows junctions/reparse points and symlinks are not followed.
                        if entry.is_symlink() or getattr(stat, "st_file_attributes", 0) & 0x400:
                            skipped.append(relative(Path(entry.path), root))
                            continue
                        if entry.is_dir(follow_symlinks=False):
                            stack.append(Path(entry.path))
                            continue
                        if not entry.is_file(follow_symlinks=False):
                            continue
                        parts = Path(entry.path).relative_to(root).parts
                        keys = [top[parts[0]]]
                        if parts[0] == "build" and len(parts) > 1:
                            keys.append(build[parts[1]])
                        suffix = Path(entry.name).suffix.lower()
                        for item in keys:
                            item["bytes"] += stat.st_size
                            item["files"] += 1
                            if suffix in INTERMEDIATE_SUFFIXES:
                                item["intermediate_bytes"] += stat.st_size
                            if suffix == ".pdb":
                                item["pdb_bytes"] += stat.st_size
                            if suffix in {".lib", ".a"}:
                                item["static_archive_bytes"] += stat.st_size
                            if suffix == ".pk3":
                                item["pk3_files"] += 1
                    except OSError as error:
                        errors.append({"path": entry.path, "error": str(error)})
        except OSError as error:
            errors.append({"path": str(folder), "error": str(error)})

    def rows(table: dict, prefix: str = "") -> list[dict]:
        return sorted([{"path": prefix + name, **dict(count),
                        "GiB": round(count["bytes"] / 2**30, 4)}
                       for name, count in table.items()], key=lambda value: -value["bytes"])

    free = shutil.disk_usage(root)
    return {"top_level": rows(top), "build_children": rows(build, "build/"),
            "filesystem_free_bytes": free.free, "skipped_links_or_reparse_points": skipped,
            "errors": errors,
            "protected_categories": ["build/navgen-overkill sole game installation",
                                     "original binaries, SDK assets and reference sources",
                                     "unique source snapshots, evidence and archives",
                                     "current final packages, module binaries, symbol maps and final PDBs"],
            "limits": ["No deletion is implemented or authorized by this inventory.",
                       "Intermediates may be regenerable, but their enclosing directory can contain unique evidence.",
                       "Sizes count each path's logical bytes; hard links may therefore be counted more than once."]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--project-dir", action="append", type=Path,
                        help="Directory containing configured cgame/qagame/ui .vcxproj files")
    parser.add_argument("--scan-disk", action="store_true", help="Also stat the repository tree without following links")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    defaults = [Path("build/sdk-win32"), Path("build/sdk-win32-navstudio"),
                Path("build/runtime-parity-20261026/windows-x64/src"),
                Path("build/modernization-validation/windows-x86/src")]
    directories = [p if p.is_absolute() else root / p for p in (args.project_dir or defaults)]
    report = {"schema": 1, "generated_utc": datetime.now(timezone.utc).isoformat(),
              "root": str(root), "sources": project_inventory(root, directories)}
    if args.scan_disk:
        report["disk"] = disk_inventory(root)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote read-only inventory: {args.out}")
    print(f"Observed {len(report['sources']['projects'])} module projects; "
          f"{len(report['sources']['not_in_observed_projects_review_only'])} unlisted source candidates.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
