#!/usr/bin/env python3
"""Read-only validation of build_windows_variants.py Windows packages.

Checks manifests, byte hashes, exact ZIP/PK3 entries, PE export tables, and
the public/editor marker policy. Never extracts, installs, or loads a DLL.
String checks establish marker presence/absence only, not function removal,
ABI call correctness, build freshness, or gameplay parity.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
MODULES = ("cgame", "ui", "qagame")
UI_ASSETS = ("ingame_main.menu", "menus.txt", "menus_gt2.txt", "menus_gt5.txt", "menus_gt7.txt")
SHARED_UI_ASSETS = ("options_customise_hud.menu", "options_controls.menu")
ICONS = ("radar_arrow.tga", "radar_dot.tga")
FLAGS = ("world_flags.tga", "LICENSE.xmod.txt", "SOURCE.json")
LEVELSHOTS = ("obj_delta_cc.tga", "obj_delta_cc_blueprint.tga", "SOURCE.json")
NAV_ICONS = ("player.tga", "player_self.tga", "player_dead.tga", "objective_a.tga", "objective_b.tga",
             "flag_terror.tga", "flag_specops.tga", "SOURCE.json")
MAX_ENTRY = 256 * 1024 * 1024
MAX_TOTAL = 512 * 1024 * 1024


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def safe_name(name: str) -> bool:
    p = PurePosixPath(name)
    return bool(name and not p.is_absolute() and "\\" not in name and ":" not in name
                and "\0" not in name and all(x not in (".", "..") for x in name.split("/"))
                and p.as_posix() == name and not name.endswith("/"))


def archive_files(data: bytes) -> dict[str, bytes]:
    result = {}
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        total = 0
        for entry in archive.infolist():
            if not safe_name(entry.filename) or entry.filename in result:
                raise ValueError("Unsafe, duplicate, or directory ZIP entry: " + entry.filename)
            if stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError("ZIP symlink is not allowed: " + entry.filename)
            total += entry.file_size
            if entry.file_size > MAX_ENTRY or total > MAX_TOTAL:
                raise ValueError("ZIP exceeds bounded package-check size")
            result[entry.filename] = archive.read(entry)
    return result


def directory_files(path: Path) -> dict[str, bytes]:
    result = {}
    total = 0
    for item in path.rglob("*"):
        if item.is_symlink():
            raise ValueError("Package symlink is not allowed: " + str(item))
        if item.is_file():
            name = item.relative_to(path).as_posix()
            if not safe_name(name):
                raise ValueError("Unsafe package path: " + name)
            total += item.stat().st_size
            if item.stat().st_size > MAX_ENTRY or total > MAX_TOTAL:
                raise ValueError("Directory exceeds bounded package-check size")
            result[name] = item.read_bytes()
    return result


def pe_info(data: bytes) -> dict:
    def require(offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset > len(data) - size:
            raise ValueError("Truncated PE structure")

    require(0, 64)
    if data[:2] != b"MZ":
        raise ValueError("Missing PE DOS signature")
    pe = struct.unpack_from("<I", data, 60)[0]
    require(pe, 24)
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    machine, sections = struct.unpack_from("<HH", data, pe + 4)
    opt_size, characteristics = struct.unpack_from("<HH", data, pe + 20)
    if not characteristics & 0x2000:
        raise ValueError("PE image is not a DLL")
    opt = pe + 24
    require(opt, opt_size)
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic not in (0x10b, 0x20b):
        raise ValueError("Unsupported PE optional header")
    directory_offset = 112 if magic == 0x20b else 96
    if opt_size < directory_offset + 8:
        raise ValueError("PE has no export data directory")
    export_rva, export_size = struct.unpack_from("<II", data, opt + directory_offset)
    section_start = opt + opt_size
    require(section_start, sections * 40)

    def offset(rva: int, size: int = 1) -> int:
        for i in range(sections):
            header = section_start + i * 40
            virtual_size, address, raw_size, raw = struct.unpack_from("<IIII", data, header + 8)
            if address <= rva and rva - address <= raw_size - size:
                position = raw + rva - address
                require(position, size)
                return position
        raise ValueError(f"Export RVA has no file-backed section: {rva:#x}")

    if not export_rva or export_size < 40:
        raise ValueError("PE DLL has no export directory")
    directory = offset(export_rva, 40)
    functions, count, function_rva, names_rva, ordinals_rva = struct.unpack_from("<IIIII", data, directory + 20)
    if count > 16384 or functions > 16384:
        raise ValueError("Unreasonable PE export table size")
    names = offset(names_rva, count * 4)
    ordinals = offset(ordinals_rva, count * 2)
    addresses = offset(function_rva, functions * 4)
    exports = []
    for i in range(count):
        start = offset(struct.unpack_from("<I", data, names + i * 4)[0])
        end = data.find(b"\0", start, min(len(data), start + 4096))
        if end < 0:
            raise ValueError("Unterminated PE export name")
        name = data[start:end].decode("ascii")
        ordinal = struct.unpack_from("<H", data, ordinals + i * 2)[0]
        if ordinal >= functions:
            raise ValueError("Invalid PE export ordinal")
        address = struct.unpack_from("<I", data, addresses + ordinal * 4)[0]
        if not address or export_rva <= address < export_rva + export_size:
            raise ValueError("Zero or forwarded module export: " + name)
        offset(address)
        exports.append(name)
    return {"machine": hex(machine), "optional_magic": hex(magic), "exports": sorted(exports)}


def current_version() -> str:
    source = (ROOT / "src/game/tce_version.h").read_text(encoding="utf-8")
    match = re.search(r'^#define TCE_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$', source, re.M)
    if not match:
        raise ValueError("Cannot read mod version")
    return match.group(1)


def client_assets(variant: str) -> dict[str, Path]:
    """Exact source resources installed by src/cmake/package_modules.cmake."""
    assets = {"gfx/misc/" + name: ROOT / "src/assets/gfx/misc" / name for name in ICONS}
    assets.update({"gfx/tce2/nav/" + name: ROOT / "src/assets/gfx/tce2/nav" / name for name in NAV_ICONS})
    assets.update({"gfx/flags/" + name: ROOT / "src/assets/gfx/flags" / name for name in FLAGS})
    assets.update({"levelshots/" + name: ROOT / "src/assets/levelshots" / name for name in LEVELSHOTS})
    menus = SHARED_UI_ASSETS + (UI_ASSETS if variant == "game" else ())
    assets.update({"ui/" + name: ROOT / "src/assets/ui" / name for name in menus})
    return assets


def check_contents(files: dict[str, bytes], label: str, version: str) -> dict:
    report = {"package": label, "errors": [], "modules": {}, "markers": {}, "archives": {}}
    errors = report["errors"]

    def exact(actual, expected, context):
        missing, extra = sorted(set(expected) - set(actual)), sorted(set(actual) - set(expected))
        if missing or extra:
            errors.append(f"{context}: missing={missing}; unexpected={extra}")

    try:
        manifest = json.loads(files["manifest.json"].decode("utf-8"))
        variant, arch = manifest["variant"], manifest["architecture"]
        if variant not in ("game", "navstudio") or arch not in ("x86", "x64"):
            raise ValueError("Invalid package variant or architecture")
        report.update(variant=variant, architecture=arch, version=manifest.get("version"))
        if manifest.get("version") != version:
            errors.append(f"Manifest version {manifest.get('version')} != requested {version}")
        if manifest.get("configuration") not in ("Debug", "Release"):
            errors.append("Manifest configuration must be Debug or Release")
        runtime = manifest.get("omnibot_runtime")
        if type(runtime) is not bool:
            errors.append("Manifest must explicitly declare boolean omnibot_runtime (legacy manifest is incomplete)")
            runtime = any(name.startswith("tce2/omni-bot/") for name in files)
        if variant == "navstudio" and not runtime:
            errors.append("NavStudio package requires its explicit Omni-bot runtime")
        core = "omnibot_et.dll" if arch == "x86" else "omnibot_et_x64.dll"
        pk3 = f"tce2-{version}.pk3"
        expected = {f"tce2/{m}_mp_{arch}.dll" for m in MODULES} | {"tce2/" + pk3}
        if runtime:
            expected.add("tce2/omni-bot/" + core)
        if variant == "navstudio":
            expected.add("tce2/zz_tce2_navstudio_editor.pk3")
        exact(files, expected | {"manifest.json", "README.txt"}, "Package files")
        inventory = manifest["files"]
        if not isinstance(inventory, dict):
            raise ValueError("Manifest files must be an object")
        exact(inventory, expected, "Manifest file inventory")
        for name, attributes in inventory.items():
            if not safe_name(name):
                errors.append("Unsafe manifest path: " + name)
                continue
            if name not in files:
                continue
            data = files[name]
            if not isinstance(attributes, dict):
                errors.append("Manifest file attributes must be an object: " + name)
                continue
            if type(attributes.get("bytes")) is not int or attributes.get("bytes") != len(data) or attributes.get("sha256") != sha(data):
                errors.append("Manifest byte count/SHA256 mismatch: " + name)
        markers = {f"cgame_mp_{arch}.dll": ("cg_omnibotdrawing", "ExportClientFunctionsFromDLL"),
                   f"ui_mp_{arch}.dll": ("omnibot_menu", "omnibot_goalproperty"),
                   f"qagame_mp_{arch}.dll": ("tce_navgen", "wp_add")}
        if runtime:
            markers["omni-bot/" + core] = ("waypoint_add", "waypoint_save", "goal_edit")
        recorded = manifest.get("editor_code_checks", {})
        if not isinstance(recorded, dict):
            raise ValueError("Manifest editor checks must be an object")
        exact(recorded, markers, "Manifest editor marker inventory")
        for name, strings in markers.items():
            if "tce2/" + name not in files:
                continue
            data = files["tce2/" + name]
            checks = {text: text.encode("ascii") + b"\0" in data for text in strings}
            report["markers"][name] = checks
            declared = recorded.get(name)
            if not isinstance(declared, dict) or any(type(v) is not bool for v in declared.values()) or checks != declared:
                errors.append("Manifest editor checks differ from actual bytes: " + name)
            if any(present != (variant == "navstudio") for present in checks.values()):
                errors.append("Wrong public/editor marker policy: " + name)
        for module in MODULES:
            name = f"tce2/{module}_mp_{arch}.dll"
            if name not in files:
                continue
            try:
                info = pe_info(files[name])
                info["sha256"] = sha(files[name])
                report["modules"][name] = info
                expected_machine, expected_magic = (("0x14c", "0x10b") if arch == "x86" else ("0x8664", "0x20b"))
                if (info["machine"], info["optional_magic"]) != (expected_machine, expected_magic):
                    errors.append("PE architecture mismatch: " + name)
                exports = {"dllEntry", "vmMain"}
                if variant == "navstudio" and module == "cgame":
                    exports.add("ExportClientFunctionsFromDLL")
                exact(info["exports"], exports, "PE export names " + name)
            except (ValueError, struct.error, UnicodeError) as error:
                errors.append(name + ": " + str(error))
        client_name = "tce2/" + pk3
        if client_name in files:
            client = archive_files(files[client_name])
            client_manifest = f"tce2/{version}/client.manifest"
            allowed = {f"{m}_mp_{arch}.dll" for m in ("cgame", "ui")} | {client_manifest}
            assets = client_assets(variant)
            allowed |= set(assets)
            exact(client, allowed, "Client PK3 entries")
            if client.get(client_manifest) != version.encode("ascii"):
                errors.append("Client manifest must contain exactly the mod version")
            for module in ("cgame", "ui"):
                name = f"{module}_mp_{arch}.dll"
                if client.get(name) != files.get("tce2/" + name):
                    errors.append("Client PK3 DLL differs from loose module: " + name)
            for name, source in assets.items():
                if client.get(name) != source.read_bytes():
                    errors.append("Client asset differs from the current public source: " + name)
            report["archives"][pk3] = {"entries": sorted(client), "sha256": sha(files[client_name])}
        addon_name = "tce2/zz_tce2_navstudio_editor.pk3"
        if addon_name in files:
            if variant != "navstudio":
                errors.append("Editor addon is forbidden in public Game")
            addon = archive_files(files[addon_name])
            editor_dir = ROOT / "navstudio/editor-ui"
            names = (editor_dir / "editor-files.txt").read_text(encoding="utf-8").splitlines()
            if any(not re.fullmatch(r"tce_[A-Za-z0-9_]+\.menu|wm_(?:ft)?quickmessage(?:Alt)?\.menu|menus(?:_gt[257])?\.txt", name) for name in names):
                raise ValueError("Invalid source editor asset allowlist")
            exact(addon, {"ui/" + name for name in names}, "Editor addon entries")
            for name in names:
                if addon.get("ui/" + name) != (editor_dir / name).read_bytes():
                    errors.append("Editor addon asset differs from current authoring source: " + name)
            report["archives"]["zz_tce2_navstudio_editor.pk3"] = {"entries": sorted(addon), "sha256": sha(files[addon_name])}
        report["manifest_sha256"] = sha(files["manifest.json"])
    except (KeyError, TypeError, ValueError, OSError, RuntimeError, NotImplementedError, zipfile.BadZipFile, UnicodeError) as error:
        errors.append("Package structure: " + str(error))
    report["passed"] = not errors
    return report


def check_package(path: Path, version: str) -> dict:
    try:
        files = directory_files(path) if path.is_dir() else archive_files(path.read_bytes())
        report = check_contents(files, str(path), version)
        if "variant" not in report:
            return report
        name = "NavStudio" if report["variant"] == "navstudio" else "Game"
        archive_name = f"TCE2-{version}-Windows-{report['architecture']}-{name}.zip"
        if path.is_dir():
            if path.name != name:
                report["errors"].append("Variant folder must be named " + name)
            outer = path.parent / archive_name
            if not outer.is_file():
                report["errors"].append("Canonical outer ZIP missing: " + str(outer))
            else:
                zipped = archive_files(outer.read_bytes())
                if zipped != files:
                    report["errors"].append("Outer ZIP entries/bytes differ from the loose package")
                report["outer_zip"] = {"path": str(outer), "sha256": sha(outer.read_bytes()), "entries": sorted(zipped)}
        elif path.name != archive_name:
            report["errors"].append("Outer ZIP must be named " + archive_name)
        report["passed"] = not report["errors"]
        return report
    except (OSError, ValueError, RuntimeError, NotImplementedError, zipfile.BadZipFile) as error:
        return {"package": str(path), "passed": False, "errors": [str(error)]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--rootdist", type=Path, help="Writer output with Game/, NavStudio/ and canonical ZIPs")
    selection.add_argument("--package", type=Path, help="One finished variant directory or canonical outer ZIP")
    parser.add_argument("--version", default=current_version())
    parser.add_argument("--out", type=Path, help="Optional JSON evidence output; inputs remain read-only")
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        parser.error("Expected semantic mod version N.N.N")
    report = {"schema": 1, "version": args.version, "packages": [], "errors": [],
              "limits": ["Marker searches do not prove complete function/code removal",
                         "PE export names do not prove calling convention, layout or behavioral parity",
                         "Manifest self-consistency and current asset comparison do not prove build freshness",
                         "Historical/missing runtime declarations fail; no legacy-success fallback"]}
    if args.rootdist:
        rootdist = args.rootdist.resolve()
        report["rootdist"] = str(rootdist)
        report["packages"] = [check_package(rootdist / name, args.version) for name in ("Game", "NavStudio")]
        architectures = {p["architecture"] for p in report["packages"] if "architecture" in p}
        if len(architectures) > 1:
            report["errors"].append("Game and NavStudio in one rootdist must use the same architecture")
        expected = {"Game", "NavStudio"}
        for package in report["packages"]:
            if package.get("architecture"):
                name = "NavStudio" if package["variant"] == "navstudio" else "Game"
                expected.add(f"TCE2-{args.version}-Windows-{package['architecture']}-{name}.zip")
        if rootdist.is_dir():
            extra = {p.name for p in rootdist.iterdir()} - expected
            if extra:
                report["errors"].append("Unexpected rootdist entries: " + ", ".join(sorted(extra)))
    else:
        report["packages"] = [check_package(args.package.resolve(), args.version)]
    report["passed"] = not report["errors"] and all(p["passed"] for p in report["packages"])
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "packages": len(report["packages"]),
                      "errors": report["errors"] + [e for p in report["packages"] for e in p["errors"]],
                      "report": str(args.out) if args.out else None}, ensure_ascii=False))
    return int(not report["passed"])


if __name__ == "__main__":
    raise SystemExit(main())
