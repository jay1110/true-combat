"""Build separate game and local NavStudio DLL packages; never deploy a game."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile


ROOT = Path(__file__).resolve().parents[1]
MODULES = ("cgame", "ui", "qagame")
UI_ASSETS = ("ingame_main.menu", "menus.txt", "menus_gt2.txt",
             "menus_gt5.txt", "menus_gt7.txt")


def run(command, log):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Build failed ({result.returncode}); see {log}")


def build(arch, configuration, variant, output):
    suffix = "x86" if arch == "Win32" else "x64"
    cache = ROOT / "build" / ("sdk-win32" if arch == "Win32" else "sdk-win64")
    if variant == "navstudio":
        cache = cache.with_name(cache.name + "-navstudio")
    cache.mkdir(parents=True, exist_ok=True)
    run(["cmake", "-S", str(ROOT / "src"), "-B", str(cache), "-A", arch,
         "-DBUILD_TESTING=OFF", "-DFEATURE_LUA=ON", "-DFEATURE_OMNIBOT=ON",
         "-DFEATURE_NAVSTUDIO=" + ("ON" if variant == "navstudio" else "OFF")],
        cache / "variant-configure.log")
    run(["cmake", "--build", str(cache), "--config", configuration,
         "--target", *MODULES, "--parallel", "4"], cache / "variant-build.log")
    binary_dir = cache / "bin" / configuration
    if (binary_dir / "tce-build-variant.txt").read_text().strip() != variant:
        raise RuntimeError("Build variant marker does not match requested package")

    name = "NavStudio" if variant == "navstudio" else "Game"
    package = output / name
    mod = package / "tce2"
    mod.mkdir(parents=True, exist_ok=True)
    manifest = {"variant": variant, "architecture": suffix,
                "configuration": configuration, "files": {}}
    for module in MODULES:
        filename = f"{module}_mp_{suffix}.dll"
        data = (binary_dir / filename).read_bytes()
        pe = struct.unpack_from("<I", data, 60)[0]
        machine = struct.unpack_from("<H", data, pe + 4)[0]
        if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0" or machine != (
                0x14c if arch == "Win32" else 0x8664):
            raise RuntimeError("Unexpected module architecture: " + filename)
        shutil.copy2(binary_dir / filename, mod / filename)
    client_package = mod / f"zz_tce2_{variant}_windows_{suffix}.pk3"
    with zipfile.ZipFile(client_package, "w", zipfile.ZIP_DEFLATED) as archive:
        for module in ("cgame", "ui"):
            filename = f"{module}_mp_{suffix}.dll"
            archive.write(binary_dir / filename, filename)
        # The editor addon supplies its own menu lists. A later-sorting client
        # PK3 must not replace them with the public lists that omit the editor.
        for asset in (UI_ASSETS if variant == "game" else ()):
            archive.write(ROOT / "src/assets/ui" / asset, "ui/" + asset)
        for asset in ("radar_arrow.tga", "radar_dot.tga"):
            archive.write(ROOT / "src/assets/gfx/misc" / asset, "gfx/misc/" + asset)
    if variant == "navstudio":
        editor_output = cache / "editor-addon"
        run(["cmake", f"-DOUTPUT_DIRECTORY={editor_output}", "-P",
             str(ROOT / "src/cmake/package_editor.cmake")], cache / "variant-editor-package.log")
        shutil.copy2(editor_output / "zz_tce2_navstudio_editor.pk3", mod)

    allowed = {f"{module}_mp_{suffix}.dll" for module in MODULES}
    allowed.add(client_package.name)
    if variant == "navstudio":
        allowed.add("zz_tce2_navstudio_editor.pk3")
    unexpected = [path.name for path in mod.iterdir() if path.name not in allowed]
    if unexpected:
        raise RuntimeError("Unexpected files in variant output; move them aside: " + ", ".join(unexpected))

    note = ("Normal game modules: NavStudio generation/editing is compiled out.\n"
            "Do not install the separate NavStudio modules/editor addon on public servers.\n"
            if variant == "game" else
            "Local NavStudio authoring modules only. Never upload these files to a public server.\n")
    (package / "README.txt").write_text(
        note + "Use the existing ET installation. Copy the tce2 contents into its tce2 folder.\n"
        "Move obsolete client-DLL PK3 overrides outside the mod search path first.\n"
        "Base assets, maps and the Omni-bot core are not duplicated in this package.\n",
        encoding="utf-8")
    for path in sorted(mod.iterdir()):
        if path.is_file():
            manifest["files"][path.relative_to(package).as_posix()] = {
                "bytes": path.stat().st_size,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    (package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    destination = output / f"TCE2-Windows-{suffix}-{name}.zip"
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in (package / "README.txt", package / "manifest.json", *sorted(mod.iterdir())):
            archive.write(path, path.relative_to(package))
    print(destination, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--architecture", choices=("Win32", "x64"), default="Win32")
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    parser.add_argument("--variant", choices=("game", "navstudio", "both"), default="both")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    output = args.out or ROOT / "dist" / ("TCE2-Windows-x86" if args.architecture == "Win32" else "TCE2-Windows-x64")
    output.mkdir(parents=True, exist_ok=True)
    for variant in (("game", "navstudio") if args.variant == "both" else (args.variant,)):
        build(args.architecture, args.configuration, variant, output.resolve())


if __name__ == "__main__":
    main()
