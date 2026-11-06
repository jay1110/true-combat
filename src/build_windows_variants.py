"""Build Windows modules and the unified single-PK3 mod/server ZIP; never deploy."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r'^#define TCE_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$',
                    (ROOT / "src/game/tce_version.h").read_text(), re.M).group(1)
MODULES = ("cgame", "ui", "qagame")


def run(command, log):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Build failed ({result.returncode}); see {log}")


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

    # Every supported entry point uses the same complete-pak packager. No
    # independently maintained UI whitelist or second editor-addon PK3 remains.
    sys.path.insert(0, str(ROOT / "tools/release"))
    module_manifest = cache / ("tce-modules-" + configuration + ".json")
    target = "windows-x86" if arch == "Win32" else "windows-x64"
    label = "Editor" if variant == "navstudio" else "Game"
    if configuration != "Release" or not enable_omnibot:
        from package import package_modules
        result = package_modules(None, output / "ModuleInputs", target, variant,
                                 core_manifest=(core_cache / ("omnibot-runtime-" + configuration + ".json") if enable_omnibot else None),
                                 geoip=geoip_database, module_manifest=module_manifest,
                                 configuration=configuration)
        print("Development/module-only transfer; not an installable full mod: " + str(result), flush=True)
        return
    inputs = cache / "single-mod-inputs.json"
    entry = {"target": target, "module_manifest": str(module_manifest),
             "core_manifest": str(core_cache / ("omnibot-runtime-Release.json"))}
    if geoip_database:
        entry["geoip_database"] = str(geoip_database)
    inputs.write_text(json.dumps([entry], indent=2) + "\n", encoding="utf-8")
    destination = output / f"TCE2-{VERSION}-{target}-{label}.zip"
    run([sys.executable, str(ROOT / "tools/release/local_bundle.py"),
         "--inputs", str(inputs), "--variant", variant, "--allow-subset",
         "--out", str(destination), "--receipt", str(destination.with_suffix(".json"))],
        cache / "single-mod-package.log")
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
