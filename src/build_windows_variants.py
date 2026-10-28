"""Build separate game and local NavStudio DLL packages; never deploy a game."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r'^#define TCE_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$',
                    (ROOT / "src/game/tce_version.h").read_text(), re.M).group(1)
MODULES = ("cgame", "ui", "qagame")
SHARED_UI_ASSETS = ("options_customise_hud.menu", "options_controls.menu",
                    "options_system.menu", "options_quality.menu")
UI_ASSETS = ("ingame_main.menu", "menus.txt", "menus_gt2.txt",
             "menus_gt5.txt", "menus_gt7.txt")


def run(command, log):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Build failed ({result.returncode}); see {log}")


def package_world_details(archive):
    """Use the same checked explicit material closure as matrix client PK3s."""
    sys.path.insert(0, str(ROOT / "tools/release"))
    from client_packages import world_detail_assets
    for path, data in world_detail_assets(ROOT).items():
        archive.writestr(path, data)


def build(arch, configuration, variant, output, with_omnibot=True, geoip_database=None):
    enable_omnibot = with_omnibot or variant == "navstudio"
    suffix = "x86" if arch == "Win32" else "x64"
    release_cache = ROOT / "build" / "releases" / VERSION / suffix
    cache = release_cache / variant
    cache.mkdir(parents=True, exist_ok=True)
    run(["cmake", "-S", str(ROOT), "-B", str(cache), "-A", arch,
         "-DBUILD_TESTING=OFF", "-DFEATURE_LUA=ON", "-DTCE_BUILD_OMNIBOT_CORE=OFF",
         "-DFEATURE_OMNIBOT=" + ("ON" if enable_omnibot else "OFF"),
         "-DFEATURE_NAVSTUDIO=" + ("ON" if variant == "navstudio" else "OFF")],
        cache / "variant-configure.log")
    run(["cmake", "--build", str(cache), "--config", configuration,
         "--target", *MODULES, "--parallel", "4"], cache / "variant-build.log")
    binary_dir = cache / "bin" / configuration
    if (binary_dir / "tce-build-variant.txt").read_text().strip() != variant:
        raise RuntimeError("Build variant marker does not match requested package")

    core_name = "omnibot_et.dll" if arch == "Win32" else "omnibot_et_x64.dll"
    if enable_omnibot:
        core_cache = release_cache / ("omnibot-" + variant)
        core_cache.mkdir(parents=True, exist_ok=True)
        core_configure = ["cmake", "-S", str(ROOT / "omnibot"),
                          "-B", str(core_cache), "-A", arch,
                          "-DOMNIBOT_ET=ON", "-DOMNIBOT_RTCW=OFF", "-DOMNIBOT_STD_FILESYSTEM=ON",
                          "-DFEATURE_NAVSTUDIO=" + ("ON" if variant == "navstudio" else "OFF")]
        if Path("C:/boost").is_dir():
            core_configure.append("-DOMNIBOT_BOOST_INCLUDEDIR=C:/boost")
        run(core_configure, core_cache / "variant-configure.log")
        run(["cmake", "--build", str(core_cache), "--config", configuration,
             "--target", "omnibot-et", "--parallel", "4"], core_cache / "variant-build.log")
        if (core_cache / "tce-build-variant.txt").read_text().strip() != variant:
            raise RuntimeError("Omni-bot build variant does not match requested package")

    name = "NavStudio" if variant == "navstudio" else "Game"
    package = output / name
    mod = package / "tce2"
    mod.mkdir(parents=True, exist_ok=True)
    core_directory = mod / "omni-bot"
    if enable_omnibot:
        core_directory.mkdir(exist_ok=True)
        core_metadata = json.loads((core_cache / ("omnibot-runtime-" + configuration + ".json")).read_text())
        if core_metadata["variant"] != variant:
            raise RuntimeError("Omni-bot runtime manifest variant differs")
        core_binary = Path(core_metadata["library"])
        if core_binary.name != core_name:
            raise RuntimeError("Omni-bot runtime manifest architecture differs: " + core_binary.name)
        shutil.copy2(core_binary, core_directory / core_name)
        sys.path.insert(0, str(ROOT / "tools" / "release"))
        from runtime_payload import stage_runtime
        from verify_omnibot_boundary import verify_core
        core_checks = verify_core(core_binary, variant, core_binary.with_suffix(".map"))
        stage_runtime(ROOT, core_directory, variant)
    elif core_directory.exists():
        raise RuntimeError("Old optional bot runtime exists in output; choose a fresh --out directory: " + str(core_directory))
    manifest = {"variant": variant, "architecture": suffix,
                "version": VERSION, "configuration": configuration,
                "omnibot_runtime": enable_omnibot, "files": {}}
    if enable_omnibot:
        manifest["bot_core_code_checks"] = core_checks
    if geoip_database:
        shutil.copy2(geoip_database, mod / "GeoIP.dat")
    markers = {
        f"cgame_mp_{suffix}.dll": ("cg_omnibotdrawing", "ExportClientFunctionsFromDLL"),
        f"ui_mp_{suffix}.dll": ("omnibot_menu", "omnibot_goalproperty"),
        f"qagame_mp_{suffix}.dll": ("tce_navgen", "wp_add"),
    }
    if enable_omnibot:
        markers[f"omni-bot/{core_name}"] = ("waypoint_add", "waypoint_save", "goal_edit")
    # Retire the previous temporary names outside the installable package.
    # Both variants use the mod's canonical PK3 name in separate directories.
    retired = cache / "retired-packages"
    for obsolete in (mod / f"zz_tce2_{variant}_windows_{suffix}.pk3",
                     output / f"TCE2-Windows-{suffix}-{name}.zip"):
        if obsolete.exists():
            retired.mkdir(exist_ok=True)
            shutil.move(str(obsolete), str(retired / f"{time.time_ns()}-{obsolete.name}"))
    for module in MODULES:
        filename = f"{module}_mp_{suffix}.dll"
        data = (binary_dir / filename).read_bytes()
        pe = struct.unpack_from("<I", data, 60)[0]
        machine = struct.unpack_from("<H", data, pe + 4)[0]
        if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0" or machine != (
                0x14c if arch == "Win32" else 0x8664):
            raise RuntimeError("Unexpected module architecture: " + filename)
        shutil.copy2(binary_dir / filename, mod / filename)
    manifest["editor_code_checks"] = {}
    for filename, strings in markers.items():
        data = (mod / filename).read_bytes()
        checks = {text: text.encode("ascii") + b"\0" in data for text in strings}
        if any(present != (variant == "navstudio") for present in checks.values()):
            raise RuntimeError("Wrong editor code in built module: " + filename)
        manifest["editor_code_checks"][filename] = checks
    client_package = mod / f"tce2-{VERSION}.pk3"
    with zipfile.ZipFile(client_package, "w", zipfile.ZIP_DEFLATED) as archive:
        for module in ("cgame", "ui"):
            filename = f"{module}_mp_{suffix}.dll"
            archive.write(binary_dir / filename, filename)
        archive.writestr(f"tce2/{VERSION}/client.manifest", VERSION)
        # The editor addon supplies its own menu lists. A later-sorting client
        # PK3 must not replace them with the public lists that omit the editor.
        for asset in SHARED_UI_ASSETS + (UI_ASSETS if variant == "game" else ()):
            archive.write(ROOT / "src/assets/ui" / asset, "ui/" + asset)
        for asset in ("radar_arrow.tga", "radar_dot.tga"):
            archive.write(ROOT / "src/assets/gfx/misc" / asset, "gfx/misc/" + asset)
        for asset in ("player.tga", "player_self.tga", "player_dead.tga", "objective_a.tga", "objective_b.tga",
                      "flag_terror.tga", "flag_specops.tga", "SOURCE.json"):
            archive.write(ROOT / "src/assets/gfx/tce2/nav" / asset, "gfx/tce2/nav/" + asset)
        for asset in ("assault.tga", "recon.tga", "sniper.tga", "SOURCE.json"):
            archive.write(ROOT / "src/assets/gfx/tce2/fireteam" / asset, "gfx/tce2/fireteam/" + asset)
        for asset in ("world_flags.tga", "LICENSE.xmod.txt", "SOURCE.json"):
            archive.write(ROOT / "src/assets/gfx/flags" / asset, "gfx/flags/" + asset)
        for asset in ("obj_delta_cc.tga", "obj_delta_cc_blueprint.tga", "SOURCE.json"):
            archive.write(ROOT / "src/assets/levelshots" / asset, "levelshots/" + asset)
        package_world_details(archive)
    if variant == "navstudio":
        editor_output = cache / "editor-addon"
        run(["cmake", f"-DOUTPUT_DIRECTORY={editor_output}", "-P",
             str(ROOT / "src/cmake/package_editor.cmake")], cache / "variant-editor-package.log")
        shutil.copy2(editor_output / "zz_tce2_navstudio_editor.pk3", mod)

    allowed = {f"{module}_mp_{suffix}.dll" for module in MODULES}
    allowed.add(client_package.name)
    if geoip_database:
        allowed.add("GeoIP.dat")
    if enable_omnibot:
        allowed.update((core_directory.name, "mapscripts", "omnibot-release.cfg"))
    if variant == "navstudio":
        allowed.add("zz_tce2_navstudio_editor.pk3")
    unexpected = [path.name for path in mod.iterdir() if path.name not in allowed]
    if unexpected:
        raise RuntimeError("Unexpected files in variant output; move them aside: " + ", ".join(unexpected))
    if enable_omnibot:
        if not (core_directory / "global_scripts/utilities.gm").is_file() or not (core_directory / "et/scripts/et_autoexec.gm").is_file():
            raise RuntimeError("Incomplete gameplay bot script closure")
        unexpected_binaries = [p for p in core_directory.rglob("*") if p.is_file() and p.suffix.lower() in (".dll", ".so", ".exe") and p != core_directory / core_name]
        if unexpected_binaries:
            raise RuntimeError("Unexpected binaries in the variant's Omni-bot runtime: " + str(unexpected_binaries))

    note = ("Normal game modules: NavStudio generation/editing is compiled out.\n"
            "Do not install the separate NavStudio modules/editor addon on public servers.\n"
            if variant == "game" else
            "Local NavStudio authoring modules only. Never upload these files to a public server.\n")
    (package / "README.txt").write_text(
        note + "Use the existing ET installation. Copy the tce2 contents into its tce2 folder.\n"
        "Move obsolete client-DLL PK3 overrides outside the mod search path first.\n"
        "Country flags: cg_countryflags 0/1 on clients; g_countryflags 0=off/1=server location/2=random bots on servers (restart map).\n"
        + ("The supplied IPv4 GeoIP.dat is included as server-only loose data.\n"
           "This product includes GeoLite2 data created by MaxMind, available from https://www.maxmind.com/.\n"
           if geoip_database else
           "For public-IP country lookup, provide a compatible IPv4 legacy GeoIP.dat in tce2; local/unknown fallback works without it.\n")
        + ("The matching Omni-bot gameplay runtime and required scripts are included. Enable bots with omnibot_enable 1 before loading a map; the default omnibot_path resolves this mod/omni-bot directory.\n" if enable_omnibot else "No optional Omni-bot runtime is included.\n"),
        encoding="utf-8")
    package_files = sorted(path for path in mod.rglob("*") if path.is_file())
    for path in package_files:
        manifest["files"][path.relative_to(package).as_posix()] = {
            "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    (package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    destination = output / f"TCE2-{VERSION}-Windows-{suffix}-{name}.zip"
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in (package / "README.txt", package / "manifest.json", *package_files):
            archive.write(path, path.relative_to(package))
    print(destination, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--architecture", choices=("Win32", "x64"), default="Win32")
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    parser.add_argument("--variant", choices=("game", "navstudio", "both"), default="game")
    bot_options = parser.add_mutually_exclusive_group()
    bot_options.add_argument("--with-omnibot", dest="with_omnibot", action="store_true",
                             help="Include the normal gameplay bot runtime (default)")
    bot_options.add_argument("--without-omnibot", dest="with_omnibot", action="store_false",
                             help="Explicitly build modules without optional gameplay bot support")
    parser.set_defaults(with_omnibot=True)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--geoip-database", type=Path,
                        help="Explicit existing legacy IPv4 GeoIP.dat input; packaged server-side, never downloaded")
    args = parser.parse_args()
    if not args.with_omnibot and args.variant in ("navstudio", "both"):
        parser.error("NavStudio requires the matching Omni-bot runtime; --without-omnibot is Game-only")
    if args.geoip_database:
        args.geoip_database = args.geoip_database.resolve()
        if not args.geoip_database.is_file() or not 6 <= args.geoip_database.stat().st_size <= 16 * 1024 * 1024:
            parser.error("--geoip-database must be an existing legacy IPv4 database between 6 bytes and 16 MiB")
    output = args.out or ROOT / "dist" / ("TCE2-Windows-x86" if args.architecture == "Win32" else "TCE2-Windows-x64")
    output.mkdir(parents=True, exist_ok=True)
    for variant in (("game", "navstudio") if args.variant == "both" else (args.variant,)):
        build(args.architecture, args.configuration, variant, output.resolve(), args.with_omnibot, args.geoip_database)


if __name__ == "__main__":
    main()
