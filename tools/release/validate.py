"""Validate manual-only release inputs and versioned archive closure offline."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import zipfile

from package import ROOT, version
from runtime_payload import command_spans, prepare_runtime, sha256
from verify_omnibot_boundary import (AUTHORING_COMMANDS, RUNTIME_BINDINGS,
                                    NATIVE_PUBLIC_COMMANDS, NATIVE_AUTHORING_COMMANDS,
                                    validate_symbol_evidence)


def workflows(repo):
    for path in sorted((repo / '.github/workflows').glob('*.y*ml')):
        source = path.read_text(encoding='utf-8')
        match = re.search(r'^on:\s*\n((?:[ \t].*\n|\n)+)', source, re.M)
        if not match or set(re.findall(r'^  ([\w_]+):', match[1], re.M)) != {'workflow_dispatch'}:
            raise ValueError('Workflow is not exclusively manual: ' + str(path))


def source_inputs(repo, expected):
    if version(repo) != expected:
        raise ValueError('Expected version ' + expected + ', got ' + version(repo))
    workflows(repo)
    base = repo / 'vendor/omni-bot/runtime'
    source = json.loads((base / 'SOURCE.json').read_text())
    for item in source['files']:
        if hashlib.sha256((base / item['path']).read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Runtime input is missing/stale: ' + item['path'])
    for mapname in source['navigation_scope']['maps']:
        for ending in ('.way', '_gt2.gm', '_gt2_goals.gm', '_gt5.gm', '_gt5_goals.gm'):
            if not (base / 'et/nav' / (mapname + ending)).is_file():
                raise ValueError('Missing standard-map navigation: ' + mapname + ending)
    for relative in ('EULA_TrueCombat_Elite.txt', 'navstudio/requirements-desktop.txt',
                     'src/assets/gfx/flags/world_flags.tga', 'src/assets/gfx/flags/LICENSE.xmod.txt',
                     'src/assets/gfx/flags/SOURCE.json', 'tools/release/runtime-policy.json'):
        if not (repo / relative).is_file():
            raise ValueError('Missing clean-checkout release input: ' + relative)
    game, _, registry, _, diagnostics = prepare_runtime(repo, 'game')
    prepare_runtime(repo, 'navstudio')
    return {'version': expected, 'manual_only': True, 'runtime_inputs': len(source['files']),
            'classified_gm_registrations': sum(len(v['registrations']) for v in registry.values()),
            'public_gm_registrations': sum(len(command_spans(v.decode('latin-1')))
                                           for k, v in game.items() if k.endswith('.gm')),
            'diagnostic_transformations': sum(len(v) for v in diagnostics.values())}


def bot_help(path, variant):
    """Verify actual console output, including all aliases, against reviewed sets."""
    source = Path(path).read_text(encoding='utf-8-sig', errors='replace')
    section, found, sections = None, {'native': set(), 'script': set()}, set()
    for line in source.splitlines():
        line = re.sub(r'\^.', '', line).strip()
        if line == '---- Omni-bot Command Help ----':
            section = 'native'
            sections.add(section)
        elif line == '---- Script Commands ----':
            section = 'script'
            sections.add(section)
        elif line in ('---- End Script Commands ----', '---- End Omni-bot Command Help ----'):
            section = None
        elif section:
            match = re.fullmatch(r'([A-Za-z_][A-Za-z_0-9]*)\s*:\s*(.*)', line)
            if match:
                found[section].add(match[1])
    if sections != {'native', 'script'}:
        raise ValueError('Incomplete /bot help capture')
    policy = json.loads((ROOT / 'tools/release/runtime-policy.json').read_text())
    public = set().union(*(set(v) for v in policy['public_commands'].values()))
    blocked = set().union(*(set(v) for v in policy['editor_commands'].values()))
    if variant == 'game':
        unknown_native = found['native'] - set(NATIVE_PUBLIC_COMMANDS)
        unknown_script = found['script'] - public
        if unknown_native or unknown_script:
            raise ValueError('Editor/unclassified command in Game help: ' +
                             repr({'native': sorted(unknown_native), 'script': sorted(unknown_script)}))
        required_native = set(NATIVE_PUBLIC_COMMANDS) - {'help'}
        required_script = {'ab', 'rollcall', 'moveskill', 'tceclass', 'tceweapon',
                           'difficulty', 'minbots', 'maxbots'}
    else:
        required_native = set(NATIVE_PUBLIC_COMMANDS + NATIVE_AUTHORING_COMMANDS) - {'help'}
        payload, _, registry, _, _ = prepare_runtime(ROOT, 'navstudio')
        allowed_script = set().union(*(set(command_spans(data.decode('latin-1')))
                                      for relative, data in payload.items() if relative.endswith('.gm')))
        unknown = found['script'] - allowed_script
        if found['native'] - required_native or unknown:
            raise ValueError('Unclassified command in Editor help: ' + repr(sorted(unknown)))
        # Optional goal/helper files are loaded on demand, so require representative
        # startup authoring APIs separately from the exhaustive public deny check.
        required_script = {'ab', 'moveskill', 'tceclass', 'tceweapon', 'tceobjectives',
                           'eob', 'cg', 'sg', 'sgi', 'sag', 'wp_check', 'draw_spawns'}
    missing = {'native': sorted(required_native - found['native']),
               'script': sorted(required_script - found['script'])}
    if any(missing.values()):
        raise ValueError('Missing expected /bot help commands: ' + repr(missing))
    return {'capture': str(path), 'variant': variant, 'passed': True,
            'native_commands': sorted(found['native']), 'script_commands': sorted(found['script'])}


def safe_names(archive):
    names = archive.namelist()
    if len(names) != len(set(names)):
        raise ValueError('Duplicate archive entries')
    for name in names:
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
            raise ValueError('Unsafe archive path: ' + name)
    if archive.testzip() is not None:
        raise ValueError('Archive CRC failure')
    return set(names)


def archive(path):
    with zipfile.ZipFile(path) as outer:
        names = safe_names(outer)
        manifest = json.loads(outer.read('manifest.json'))
        expected = {'manifest.json'} | {item['path'] for item in manifest['files']}
        if names != expected:
            raise ValueError('Unmanifested/missing release files: ' + str(names ^ expected))
        for item in manifest['files']:
            if hashlib.sha256(outer.read(item['path'])).hexdigest() != item['sha256']:
                raise ValueError('Release input fingerprint mismatch: ' + item['path'])
        editor = manifest['variant'] == 'navstudio'
        addon = 'tce2/zz_tce2_navstudio_editor.pk3'
        if (addon in names) != editor:
            raise ValueError('Wrong editor addon boundary')
        client = 'tce2/tce2-' + manifest['version'] + '.pk3'
        with zipfile.ZipFile(io.BytesIO(outer.read(client))) as pk3:
            client_names = safe_names(pk3)
            if pk3.read('tce2/' + manifest['version'] + '/client.manifest').decode() != manifest['version']:
                raise ValueError('Wrong client manifest version')
            flags = json.loads(pk3.read('gfx/flags/SOURCE.json'))
            if hashlib.sha256(pk3.read('gfx/flags/world_flags.tga')).hexdigest() != flags['sha256']:
                raise ValueError('Flag atlas provenance mismatch')
            pk3.read('gfx/flags/LICENSE.xmod.txt')
            if any('geoip' in name.lower() or 'qagame' in name.lower() for name in client_names):
                raise ValueError('Server-only file leaked into client PK3')
            if any(name.startswith('ui/') for name in client_names) == editor:
                raise ValueError('Public menu overrides replaced the editor menu list')
        if manifest['gameplay_bot_runtime']:
            boundary = manifest['bot_core_boundary']
            data = outer.read('tce2/omni-bot/' + boundary['library'])
            if boundary.get('variant') != manifest['variant']:
                raise ValueError('Wrong compiled core variant')
            # Revalidate exact method sets and defined-function proof for every
            # architecture, bound to these core bytes. Optimized SSO constants
            # need not remain contiguous strings in a compiled module.
            validate_symbol_evidence(boundary, data)
            if not editor and any(marker.encode() + b'\0' in data for marker in AUTHORING_COMMANDS):
                raise ValueError('Editor command lookup literal in Game core')
            if (b'ExportClientFunctionsFromDLL\0' in data) != editor:
                raise ValueError('Wrong client drawing export lookup boundary')
            diagnostics = boundary.get('navigation_diagnostic_formats')
            if not diagnostics:
                raise ValueError('Missing navigation diagnostic boundary inventory')
            for marker, recorded in diagnostics.items():
                found = marker.encode() + b'\0' in data
                if found != recorded or (found and not editor):
                    raise ValueError('Public navigation diagnostic leaked: ' + marker)
            if not all(s.encode() + b'\0' in data for s in RUNTIME_BINDINGS):
                raise ValueError('Missing compiled bot gameplay bindings')
            runtime = json.loads(outer.read('tce2/omni-bot/runtime-manifest.json'))
            policy = json.loads((ROOT / 'tools/release/runtime-policy.json').read_text())
            if runtime.get('schema') != 2 or runtime.get('policy_sha256') != sha256(ROOT / 'tools/release/runtime-policy.json'):
                raise ValueError('Runtime policy manifest is missing/stale')
            expected_payload, _, registry, _, diagnostics = prepare_runtime(ROOT, manifest['variant'])
            if runtime['command_registry'] != registry or runtime['diagnostics'] != diagnostics:
                raise ValueError('Incomplete command/diagnostic provenance')
            for relative, expected_data in expected_payload.items():
                prefix = 'tce2/' if relative.startswith('release_mapscripts/') else 'tce2/omni-bot/'
                output = ('mapscripts/' + PurePosixPath(relative).name
                          if relative.startswith('release_mapscripts/') else relative)
                if outer.read(prefix + output) != expected_data:
                    raise ValueError('Nonreproducible runtime payload: ' + relative)
            for relative in policy['editor_files']:
                if ('tce2/omni-bot/' + relative in names) != editor:
                    raise ValueError('Wrong editor helper file boundary')
            for relative, blocked in policy['editor_commands'].items():
                commands = command_spans(outer.read('tce2/omni-bot/' + relative).decode('latin-1'))
                expected_commands = set(policy['public_commands'][relative]) | (set(blocked) if editor else set())
                if set(commands) != expected_commands:
                    raise ValueError('Wrong complete script registration set: ' + relative)
            for row in runtime['files']:
                if hashlib.sha256(outer.read('tce2/omni-bot/' + row['path'])).hexdigest() != row['sha256']:
                    raise ValueError('Runtime file mismatch: ' + row['path'])
            for row in runtime['mod_files']:
                if hashlib.sha256(outer.read('tce2/' + row['path'])).hexdigest() != row['sha256']:
                    raise ValueError('Mod map-script/config mismatch: ' + row['path'])
    return {'archive': path.name, 'version': manifest['version'], 'target': manifest['target'],
            'variant': manifest['variant'], 'complete_runtime': manifest['gameplay_bot_runtime'],
            'files': len(names), 'passed': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    operations = parser.add_subparsers(dest='operation', required=True)
    source = operations.add_parser('source')
    source.add_argument('--expect-version', required=True)
    archives = operations.add_parser('archives')
    archives.add_argument('directory', type=Path)
    help_capture = operations.add_parser('bot-help')
    help_capture.add_argument('capture', type=Path)
    help_capture.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    args = parser.parse_args()
    if args.operation == 'source':
        print(json.dumps(source_inputs(ROOT, args.expect_version), indent=2))
    elif args.operation == 'bot-help':
        print(json.dumps(bot_help(args.capture, args.variant), indent=2))
    else:
        paths = sorted(args.directory.glob('*.zip'))
        if not paths:
            raise ValueError('No release ZIPs to validate')
        results = [archive(path) for path in paths]
        (args.directory / 'validation.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
