"""Canonical per-variant client downloads, assembled only after ABI collection."""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import struct
import zipfile

from verify_omnibot_boundary import verify_module_bytes, macho_architectures

ROOT = Path(__file__).resolve().parents[2]
INPUT_PREFIX = 'client-inputs/'
SHARED_UI = ('options_customise_hud.menu', 'options_controls.menu', 'options_system.menu', 'options_quality.menu')
PUBLIC_UI = ('ingame_main.menu', 'menus.txt', 'menus_gt2.txt', 'menus_gt5.txt', 'menus_gt7.txt')
DELTA_IMAGES = ('obj_delta_cc.tga', 'obj_delta_cc_blueprint.tga')
NAV_ICONS = ('player.tga', 'player_self.tga', 'player_dead.tga', 'objective_a.tga', 'objective_b.tga',
             'flag_terror.tga', 'flag_specops.tga', 'SOURCE.json')
FIRETEAM_ICONS = ('assault.tga', 'recon.tga', 'sniper.tga', 'SOURCE.json')
WORLD_MATERIALS = (
    ('asphalt_e1', 'textures/coroner/asphalt_e1', 'textures/coroner/asphalt_e1.jpg', 'asphalt', (8, 8), ''),
    ('cement_s1', 'textures/coroner/cement_s1', 'textures/coroner/cement_s1.jpg', 'concrete', (8, 4), ''),
    ('brick_dirt_7', 'textures/twy_brickstiles/brick_dirt_7', 'textures/twy_brickstiles/brick_dirt_7.jpg', 'brick', (8, 8), ''),
    ('concrete_blue', 'textures/bruce_trim/concrete_blue', 'textures/bruce_crete/concrete_blue.tga', 'concrete', (8, 2), ''),
    ('wood_hout4', 'textures/liq/hout4', 'textures/liq/hout4.tga', 'wood', (4, 4), 'mat_wood'),
)
WORLD_DETAILS = ('asphalt.tga', 'concrete.tga', 'brick.tga', 'wood.tga', 'SOURCE.json')
WORLD_ASSET_PATHS = tuple('materials/tce2/world/' + row[0] + '.shader' for row in WORLD_MATERIALS) + tuple(
    'textures/tce2/details/' + name for name in WORLD_DETAILS)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + '\n').encode('utf-8')


def zip_bytes(files):
    """Stable metadata prevents same-name cross-server checksum disagreements."""
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data, compresslevel=9)
    return stream.getvalue()


def zip_contents(data):
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or len(names) != len({name.casefold() for name in names}):
            raise ValueError('Duplicate/case-colliding client archive entries')
        for name in names:
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name or name.endswith('/'):
                raise ValueError('Unsafe/non-file client archive entry: ' + name)
        return {name: archive.read(name) for name in names}


def target_table(repo=ROOT):
    return {row['id']: row for row in json.loads((repo / 'tools/release/targets.json').read_text())['targets']}


def module_names(target, repo=ROOT):
    row = target_table(repo)[target]
    return {role: row.get('prefix', '') + role + row['suffix'] for role in ('cgame', 'ui')}


def validate_levelshots(files):
    try:
        provenance = json.loads(files['levelshots/SOURCE.json'])
    except (KeyError, ValueError, UnicodeDecodeError) as error:
        raise ValueError('Missing/invalid user-provided Delta image provenance') from error
    rows = provenance.get('files', [])
    if (provenance.get('schema') != 1 or provenance.get('map') != 'obj_delta' or
            len(rows) != len(DELTA_IMAGES) or {row.get('asset') for row in rows} != set(DELTA_IMAGES)):
        raise ValueError('Wrong user-provided Delta image provenance')
    for row in rows:
        data = files.get('levelshots/' + row['asset'])
        if (data is None or sha(data) != row.get('sha256') or len(data) != row.get('bytes') or
                row.get('source_path') != 'pak/levelshots/' + row['asset']):
            raise ValueError('Missing/changed user-provided Delta image: ' + row['asset'])
    return provenance


def levelshot_assets(repo=ROOT):
    files = {'levelshots/' + name: (repo / 'src/assets/levelshots' / name).read_bytes()
             for name in DELTA_IMAGES + ('SOURCE.json',)}
    validate_levelshots(files)
    return files


def validate_fireteam_icons(files):
    try:
        provenance = json.loads(files['gfx/tce2/fireteam/SOURCE.json'])
    except (KeyError, ValueError, UnicodeDecodeError) as error:
        raise ValueError('Missing/invalid Fireteam icon provenance') from error
    rows = provenance.get('files', []) if isinstance(provenance, dict) else []
    if (not isinstance(provenance, dict) or provenance.get('schema') != 1 or
            not isinstance(rows, list) or len(rows) != 3 or
            any(not isinstance(row, dict) for row in rows) or
            {row.get('asset') for row in rows} != set(FIRETEAM_ICONS) - {'SOURCE.json'}):
        raise ValueError('Wrong Fireteam icon provenance')
    for row in rows:
        data = files.get('gfx/tce2/fireteam/' + row['asset'])
        if (data is None or sha(data) != row.get('sha256') or
                row.get('width') != 64 or row.get('height') != 64 or
                len(data) < 18 + 64 * 64 * 4 or data[:3] != b'\x00\x00\x02' or
                struct.unpack_from('<HH', data, 12) != (64, 64) or data[16:18] != b'\x20\x28'):
            raise ValueError('Missing/changed/invalid Fireteam icon: ' + row['asset'])
    return provenance


def fireteam_assets(repo=ROOT):
    files = {'gfx/tce2/fireteam/' + name: (repo / 'src/assets/gfx/tce2/fireteam' / name).read_bytes()
             for name in FIRETEAM_ICONS}
    validate_fireteam_icons(files)
    return files


def validate_world_details(files):
    """Require complete original-name materials, not auto-loaded scripts/remaps."""
    try:
        source = json.loads(files['textures/tce2/details/SOURCE.json'])
    except (KeyError, ValueError, UnicodeDecodeError) as error:
        raise ValueError('Missing/invalid world detail provenance') from error
    if not isinstance(source, dict) or source.get('schema') != 1:
        raise ValueError('Wrong world detail provenance')
    textures, materials = source.get('files'), source.get('materials')
    if (not isinstance(textures, list) or len(textures) != 4 or
            any(not isinstance(row, dict) or not isinstance(row.get('asset'), str) for row in textures) or
            {row.get('asset') for row in textures} != set(WORLD_DETAILS) - {'SOURCE.json'} or
            not isinstance(materials, list) or len(materials) != len(WORLD_MATERIALS) or
            any(not isinstance(row, dict) or not isinstance(row.get('path'), str) for row in materials) or
            {row.get('path') for row in materials} != set(WORLD_ASSET_PATHS[:len(WORLD_MATERIALS)])):
        raise ValueError('Wrong world detail texture/material inventory')
    for row in textures:
        path = 'textures/tce2/details/' + row['asset']
        data = files.get(path)
        if (data is None or sha(data) != row.get('sha256') or
                row.get('width') != 128 or row.get('height') != 128 or
                len(data) != 18 + 128 * 128 * 4 + 26 or data[:3] != b'\x00\x00\x02' or
                struct.unpack_from('<HH', data, 12) != (128, 128) or data[16:18] != b'\x20\x28'):
            raise ValueError('Missing/changed/invalid world detail texture: ' + path)
    expected = {row[0] + '.shader': row for row in WORLD_MATERIALS}
    for row in materials:
        path = row['path']
        _, original, diffuse, detail, scale, surface = expected[PurePosixPath(path).name]
        data = files.get(path)
        if (data is None or sha(data) != row.get('sha256') or len(data) != row.get('bytes') or
                row.get('original_name') != original or row.get('diffuse') != diffuse or
                row.get('detail') != detail or row.get('scale') != list(scale) or row.get('surfaceparm') != surface):
            raise ValueError('Missing/changed world detail material: ' + path)
        # Validate the whole shader block/stages even if a damaged file and its
        # manifest are rehashed together. Preserve lightmap, diffuse and flags.
        body = (original + ' { qer_editorimage ' + diffuse +
                (' surfaceparm ' + surface if surface else '') +
                ' { map $lightmap rgbGen identity } { map ' + diffuse +
                ' blendFunc GL_DST_COLOR GL_ZERO rgbGen identity } { map textures/tce2/details/' + detail +
                '.tga blendFunc GL_DST_COLOR GL_SRC_COLOR rgbGen identity tcMod scale ' +
                str(scale[0]) + ' ' + str(scale[1]) + ' detail } }')
        try:
            actual = re.sub(r'/\*.*?\*/|//[^\n]*', '', data.decode('utf-8'), flags=re.S)
        except UnicodeDecodeError as error:
            raise ValueError('Invalid world detail shader encoding: ' + path) from error
        tokens = lambda text: re.findall(r'[{}]|[^\s{}]+', text)
        if tokens(actual) != tokens(body):
            raise ValueError('Incomplete/changed original-name world detail shader: ' + path)
    return source


def world_detail_assets(repo=ROOT):
    files = {name: (repo / 'src/assets' / name).read_bytes() for name in WORLD_ASSET_PATHS}
    validate_world_details(files)
    return files


def client_assets(release_version, variant, repo=ROOT):
    if variant not in ('game', 'navstudio'):
        raise ValueError('Unknown client variant')
    names = ['ui/' + name for name in SHARED_UI + (PUBLIC_UI if variant == 'game' else ())]
    names += ['gfx/misc/' + name for name in ('radar_arrow.tga', 'radar_dot.tga')]
    names += ['gfx/tce2/nav/' + name for name in NAV_ICONS]
    names += ['gfx/flags/' + name for name in ('world_flags.tga', 'LICENSE.xmod.txt', 'SOURCE.json')]
    result = {name: (repo / 'src/assets' / name).read_bytes() for name in names}
    result.update(levelshot_assets(repo))
    result.update(fireteam_assets(repo))
    result.update(world_detail_assets(repo))
    result['tce2/' + release_version + '/client.manifest'] = release_version.encode('ascii')
    return result


def check_module(data, role, target, variant):
    """Assert the advertised client ABI as well as the public/editor boundary."""
    valid = False
    try:
        if target.startswith('windows-') and data[:2] == b'MZ':
            pe = struct.unpack_from('<I', data, 60)[0]
            expected = (0x14c, 0x10b) if target == 'windows-x86' else (0x8664, 0x20b)
            valid = (data[pe:pe + 4] == b'PE\0\0' and
                     (struct.unpack_from('<H', data, pe + 4)[0], struct.unpack_from('<H', data, pe + 24)[0]) == expected and
                     bool(struct.unpack_from('<H', data, pe + 22)[0] & 0x2000))
        elif target.startswith(('linux-', 'android-')):
            expected = {'linux-i386': (1, 3), 'linux-x86_64': (2, 62), 'linux-armv7': (1, 40),
                        'linux-aarch64': (2, 183), 'android-armv7': (1, 40), 'android-arm64': (2, 183)}[target]
            valid = (data[:4] == b'\x7fELF' and data[5] == 1 and
                     (data[4], struct.unpack_from('<H', data, 18)[0]) == expected and
                     struct.unpack_from('<H', data, 16)[0] == 3)
        elif target == 'wasm32':
            valid = data[:8] == b'\0asm\x01\0\0\0'
        elif target == 'macos-universal':
            valid = sorted(macho_architectures(data)) == ['arm64', 'x86_64']
    except (IndexError, KeyError, ValueError, struct.error):
        valid = False
    if not valid:
        raise ValueError('Wrong native client ABI: ' + target + '/' + role)
    verify_module_bytes(role, data, variant)


def check_target_payload(payload, target, variant, release_version, repo=ROOT):
    names = module_names(target, repo)
    assets = client_assets(release_version, variant, repo)
    if set(payload) != set(names.values()) | set(assets):
        raise ValueError('Wrong client inputs/assets for ' + target + '/' + variant)
    for role, name in names.items():
        check_module(payload[name], role, target, variant)
    for name, data in assets.items():
        if payload[name] != data:
            raise ValueError('Different/stale shared client asset: ' + name)
    return names, assets


def read_target(path, release_version, repo=ROOT):
    """Verify immutable transfer bytes before promoting any installable archive."""
    files = zip_contents(Path(path).read_bytes())
    metadata = json.loads(files['manifest.json'])
    target, variant = metadata.get('target'), metadata.get('variant')
    if (not Path(path).name.endswith('-Client-Inputs.zip') or
            metadata.get('schema') != 1 or metadata.get('version') != release_version or
            target not in target_table(repo) or variant not in ('game', 'navstudio') or
            metadata.get('authoring') is not (variant == 'navstudio') or
            metadata.get('configuration') != 'Release' or
            metadata.get('client_package', {}).get('scope') != 'target-input' or
            metadata.get('client_package', {}).get('targets') != [target]):
        raise ValueError('Not a Release client-input archive: ' + str(path))
    rows = metadata.get('files', [])
    recorded = {row['path']: row for row in rows}
    if len(recorded) != len(rows) or set(files) != set(recorded) | {'manifest.json'}:
        raise ValueError('Unmanifested client-input archive entries')
    for name, row in recorded.items():
        if sha(files[name]) != row['sha256'] or len(files[name]) != row['bytes']:
            raise ValueError('Client-input fingerprint mismatch: ' + name)
    if any(name.endswith('.pk3') and name != 'tce2/zz_tce2_navstudio_editor.pk3' for name in files):
        raise ValueError('Provisional canonical PK3 in client-input archive')
    addon = 'tce2/zz_tce2_navstudio_editor.pk3'
    if (addon in files) != (variant == 'navstudio'):
        raise ValueError('Wrong editor addon boundary')
    if variant == 'navstudio':
        editor = repo / 'navstudio/editor-ui'
        expected_addon = {'ui/' + name: (editor / name).read_bytes()
                          for name in (editor / 'editor-files.txt').read_text().splitlines() if name}
        if zip_contents(files[addon]) != expected_addon:
            raise ValueError('Different/stale editor addon assets')
    payload = {name[len(INPUT_PREFIX):]: data for name, data in files.items() if name.startswith(INPUT_PREFIX)}
    names, _ = check_target_payload(payload, target, variant, release_version, repo)
    for role, name in names.items():
        if files.get('tce2/' + name) != payload[name]:
            raise ValueError('Loose/download client module disagreement: ' + name)
    config = target_table(repo)[target]
    server_name = config.get('prefix', '') + 'qagame' + config['suffix']
    if 'tce2/' + server_name not in files:
        raise ValueError('Missing target server module: ' + server_name)
    check_module(files['tce2/' + server_name], 'qagame', target, variant)
    return {'path': Path(path), 'metadata': metadata, 'files': files, 'payload': payload}


def canonical_client(inputs, expected_targets, release_version, variant, repo=ROOT):
    expected_targets = set(expected_targets)
    if not expected_targets or not expected_targets <= set(target_table(repo)):
        raise ValueError('Unknown/empty expected client ABI set')
    actual = [row['metadata']['target'] for row in inputs]
    if len(actual) != len(set(actual)) or set(actual) != expected_targets:
        raise ValueError('Missing/duplicate client ABI inputs for ' + variant)
    files = client_assets(release_version, variant, repo)
    modules = {}
    for row in sorted(inputs, key=lambda value: value['metadata']['target']):
        metadata = row['metadata']
        if metadata['variant'] != variant or metadata['configuration'] != 'Release':
            raise ValueError('Mixed client variant/configuration')
        names, _ = check_target_payload(row['payload'], metadata['target'], variant, release_version, repo)
        for role, name in names.items():
            if name in files:
                raise ValueError('Colliding client module name: ' + name)
            files[name] = row['payload'][name]
            modules[name] = {'role': role, 'target': metadata['target'], 'sha256': sha(files[name])}
    scope = 'release' if expected_targets == set(target_table(repo)) else 'local-diagnostic'
    metadata = {'schema': 1, 'scope': scope, 'version': release_version, 'variant': variant,
                'configuration': 'Release', 'targets': sorted(expected_targets), 'modules': modules,
                'assets': {name: sha(data) for name, data in files.items() if name not in modules}}
    files['tce2/' + release_version + '/client-abis.json'] = json_bytes(metadata)
    data = zip_bytes(files)
    return data, metadata


def check_canonical(data, release_version, variant, expected_targets=None, repo=ROOT):
    files = zip_contents(data)
    path = 'tce2/' + release_version + '/client-abis.json'
    metadata = json.loads(files.pop(path))
    expected_targets = set(target_table(repo) if expected_targets is None else expected_targets)
    scope = 'release' if expected_targets == set(target_table(repo)) else 'local-diagnostic'
    if (metadata.get('schema') != 1 or metadata.get('scope') != scope or metadata.get('version') != release_version or
            metadata.get('variant') != variant or metadata.get('configuration') != 'Release' or
            metadata.get('targets') != sorted(expected_targets)):
        raise ValueError('Wrong/incomplete canonical client ABI manifest')
    expected_files = client_assets(release_version, variant, repo)
    modules = {}
    for target in sorted(expected_targets):
        for role, name in module_names(target, repo).items():
            if name not in files:
                raise ValueError('Missing canonical client ABI: ' + name)
            check_module(files[name], role, target, variant)
            expected_files[name] = files[name]
            modules[name] = {'role': role, 'target': target, 'sha256': sha(files[name])}
    assets = {name: sha(data) for name, data in client_assets(release_version, variant, repo).items()}
    if files != expected_files or metadata.get('modules') != modules or metadata.get('assets') != assets:
        raise ValueError('Canonical client fingerprint/assets disagreement')
    return metadata


def promote_target(row, client_data, client_metadata, destination, release_version, repo=ROOT):
    metadata = dict(row['metadata'])
    verified = check_canonical(client_data, release_version, metadata['variant'], repo=repo)
    if verified != client_metadata:
        raise ValueError('Different canonical promotion metadata')
    files = {name: data for name, data in row['files'].items()
             if name != 'manifest.json' and not name.startswith(INPUT_PREFIX)}
    if 'README.txt' in files:
        files['README.txt'] = files['README.txt'].replace(
            b'CLIENT INPUTS ONLY: use the aggregate release for installation and server downloads.\n',
            b'Install into an existing matching ET/ETLegacy installation; this variant has the shared release client PK3.\n')
    files['tce2/tce2-' + release_version + '.pk3'] = client_data
    metadata['client_package'] = {'scope': 'shared', 'targets': client_metadata['targets'],
                                  'sha256': sha(client_data), 'configuration': 'Release'}
    metadata['files'] = [{'path': name, 'sha256': sha(data), 'bytes': len(data)}
                         for name, data in sorted(files.items())]
    files['manifest.json'] = json_bytes(metadata)
    Path(destination).write_bytes(zip_bytes(files))
    return destination
