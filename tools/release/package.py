"""Package built mod/runtime artifacts without copying a game installation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

from runtime_payload import stage_runtime
from verify_omnibot_boundary import verify_core

ROOT = Path(__file__).resolve().parents[2]


def version(repo=ROOT):
    match = re.search(r'^#define TCE_VERSION "(\d+\.\d+\.\d+)"',
                      (repo / 'src/game/tce_version.h').read_text(), re.M)
    if not match:
        raise ValueError('Missing semantic mod version')
    return match[1]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def copy_file(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def zip_folder(folder, output):
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(folder.rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(folder).as_posix())
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError('Archive CRC failure')
    output.with_suffix(output.suffix + '.sha256').write_text(
        digest(output) + '  ' + output.name + '\n', encoding='ascii')


def module_inputs(modules, config, variant, module_manifest=None):
    """Resolve the exact CMake target outputs, never a guessed directory glob."""
    prefix, suffix = config.get('prefix', ''), config['suffix']
    if module_manifest is not None:
        metadata = json.loads(Path(module_manifest).read_text(encoding='utf-8'))
        if (metadata.get('schema') != 1 or metadata.get('variant') != variant or
                set(metadata.get('modules', {})) != {'cgame', 'ui', 'qagame'}):
            raise ValueError('Invalid module TARGET_FILE discovery manifest')
        paths = {name: Path(path).resolve() for name, path in metadata['modules'].items()}
    elif modules is not None:
        paths = {name: Path(modules) / (prefix + name + suffix)
                 for name in ('cgame', 'ui', 'qagame')}
    else:
        raise ValueError('Supply a CMake module manifest or an explicit module directory')
    for name, path in paths.items():
        expected = prefix + name + suffix
        if path.name != expected or not path.is_file():
            raise ValueError('Missing/wrong module target output: ' + name + ': ' + str(path))
    return paths


def package_modules(modules, output, target, variant, core_manifest=None, geoip=None,
                    repo=ROOT, module_manifest=None):
    release_version = version(repo)
    targets = json.loads((repo / 'tools/release/targets.json').read_text())['targets']
    config = next(t for t in targets if t['id'] == target)
    module_paths = module_inputs(modules, config, variant, module_manifest)
    name = f'TCE2-{release_version}-{target}-{variant}'
    if core_manifest is None:
        name += '-Modules-Only'
    stage = output / name
    if stage.exists():
        raise ValueError('Refusing to mix release outputs: ' + str(stage))
    mod = stage / 'tce2'
    mod.mkdir(parents=True)
    for path in module_paths.values():
        copy_file(path, mod / path.name)
    (mod / 'tce-build-variant.txt').write_text(variant + '\n', encoding='ascii')
    subprocess.run(['cmake', f'-DMODULE_DIRECTORY={mod}', f'-DBUILD_VARIANT={variant}',
                    '-P', str(repo / 'src/cmake/package_modules.cmake')], check=True)
    (mod / 'tce-build-variant.txt').unlink()
    if variant == 'navstudio':
        addon = stage / 'editor-addon'
        subprocess.run(['cmake', f'-DOUTPUT_DIRECTORY={addon}', '-P',
                        str(repo / 'src/cmake/package_editor.cmake')], check=True)
        copy_file(addon / 'zz_tce2_navstudio_editor.pk3', mod / 'zz_tce2_navstudio_editor.pk3')
        # Only the final addon belongs in the release, never its loose stage.
        if not addon.resolve().is_relative_to(stage.resolve()):
            raise ValueError('Editor staging path escaped the release directory')
        for path in sorted(addon.rglob('*'), reverse=True):
            if path.is_file():
                path.unlink()
            elif path.is_dir():
                path.rmdir()
        addon.rmdir()
    runtime = None
    if core_manifest is not None:
        metadata = json.loads(core_manifest.read_text())
        if metadata['variant'] != variant:
            raise ValueError('Wrong authoring variant in bot core manifest')
        library = Path(metadata['library'])
        map_file = library.with_suffix('.map')
        core_checks = verify_core(library, variant, map_file if map_file.is_file() else None)
        core = mod / 'omni-bot'
        copy_file(library, core / library.name)
        runtime = stage_runtime(repo, core, variant)
    if geoip is not None:
        if not 6 <= geoip.stat().st_size <= 16 * 1024 * 1024:
            raise ValueError('GeoIP.dat must be a compatible existing legacy IPv4 database')
        copy_file(geoip, mod / 'GeoIP.dat')
    # Server Lua and WolfAdmin are optional configuration; no user databases.
    for path in (repo / 'src/lua').rglob('*'):
        if path.is_file() and path.suffix in ('.lua', '.cfg', '.md', '.txt', '.sql'):
            copy_file(path, mod / 'lua' / path.relative_to(repo / 'src/lua'))
    for relative in ('LICENSE', 'EULA_TrueCombat_Elite.txt', 'vendor/dependencies.lock.json',
                     'vendor/lua/IMPORT-NOTICE.txt', 'vendor/lua/lua.h',
                     'vendor/sqlite/IMPORT-NOTICE.txt', 'vendor/luasql/license.html'):
        copy_file(repo / relative, stage / 'licenses' / relative)
    (stage / 'README.txt').write_text(
        f'TCE2 {release_version}: {target}, {variant}.\n'
        'Install into an existing matching ET/ETLegacy installation; no engine or maps are bundled.\n'
        + ('Gameplay Omni-bot core and scripts are included. Execute tce2/omnibot-release.cfg before map startup.\n'
           'Standard-map navigation is listed in the runtime manifest; other maps need their own WAY/goals.\n' if runtime else
           'MODULES ONLY: a matching bot core was not supplied. This archive is not a complete bot runtime.\n')
        + ('LOCAL AUTHORING BUILD: never deploy its DLLs or editor addon on a public server.\n'
           if variant == 'navstudio' else 'Public gameplay build: waypoint/goal authoring is compiled out.\n')
        + 'GeoIP.dat is optional server-only data; place a compatible licensed database loose in tce2.\n'
        'The client PK3 includes the flag atlas and attribution. Lua/WolfAdmin configuration is optional.\n',
        encoding='utf-8', newline='\n')
    files = [{'path': p.relative_to(stage).as_posix(), 'sha256': digest(p), 'bytes': p.stat().st_size}
             for p in sorted(stage.rglob('*')) if p.is_file()]
    (stage / 'manifest.json').write_text(json.dumps({
        'schema': 1, 'version': release_version, 'target': target, 'variant': variant,
        'gameplay_bot_runtime': runtime is not None, 'authoring': variant == 'navstudio',
        'bot_core_boundary': core_checks if runtime else None,
        'geoip_database_included': geoip is not None, 'files': files,
    }, indent=2) + '\n', encoding='utf-8', newline='\n')
    output.mkdir(parents=True, exist_ok=True)
    archive = output / (name + '.zip')
    zip_folder(stage, archive)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--modules', type=Path)
    inputs.add_argument('--module-manifest', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    parser.add_argument('--core-manifest', type=Path)
    parser.add_argument('--geoip-database', type=Path)
    args = parser.parse_args()
    print(package_modules(args.modules.resolve() if args.modules else None,
                          args.output.resolve(), args.target, args.variant,
                          args.core_manifest, args.geoip_database,
                          module_manifest=args.module_manifest))


if __name__ == '__main__':
    main()
