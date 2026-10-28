#!/usr/bin/env python3
"""Independently verify opt-in UI PK3 closure, semantics and public boundaries."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import stat
import zipfile

import semantic as sem

ROOT = Path(__file__).resolve().parents[2]
OVERRIDES = ('ingame_main.menu', 'options_controls.menu', 'options_customise_hud.menu')
LOAD_LISTS = ('menus.txt', 'menus_gt2.txt', 'menus_gt5.txt', 'menus_gt7.txt')
EDITOR_MENU = 'ui/tce_omnibot.menu'
FORBIDDEN_SUFFIXES = {'.dll', '.so', '.dylib', '.exe', '.qvm', '.wasm', '.bsp', '.way', '.gm',
                      '.zip', '.pk3', '.obj', '.o', '.lib', '.a', '.pdb', '.map'}
RUNTIME_MARKER=('tce2_ui/runtime.cfg',b'TCE2_MODERN_UI 1\n')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(name):
    return (isinstance(name, str) and bool(name) and '\\' not in name and ':' not in name and '\0' not in name
            and not name.endswith('/') and not PurePosixPath(name).is_absolute()
            and all(part not in ('', '.', '..') for part in name.split('/')))


def read_archive(source):
    files = {}
    with zipfile.ZipFile(io.BytesIO(source) if isinstance(source, bytes) else source) as archive:
        entries = archive.infolist()
        if len(entries) > 4096 or sum(e.file_size for e in entries) > 128 * 1024 * 1024:
            raise ValueError('Unbounded UI archive')
        for entry in entries:
            name = entry.filename
            if (entry.orig_filename != name or not safe_path(name) or name.casefold() in {n.casefold() for n in files} or
                    stat.S_ISLNK(entry.external_attr >> 16) or entry.file_size > 32 * 1024 * 1024):
                raise ValueError('Unsafe/duplicate UI archive entry: ' + name)
            data = archive.read(entry)
            if (PurePosixPath(name).suffix.lower() in FORBIDDEN_SUFFIXES or
                    data.startswith((b'MZ', b'\x7fELF', b'\0asm', b'\xfe\xed\xfa\xce', b'\xfe\xed\xfa\xcf',
                                     b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'))):
                raise ValueError('Executable/gameplay payload in UI-only archive: ' + name)
            files[name] = data
    return files


def text_paths(files):
    return {p for p in files if PurePosixPath(p).suffix.lower() in ('.menu', '.h') or
            (p.split('/')[0] in sem.DIRECTORIES and PurePosixPath(p).suffix.lower() == '.txt')}


def remove_editor_load(data):
    # Token policy is independent of package.py's line-removal regexp.
    ts = sem.tokens(sem.strip_comments(data.decode('latin-1')))
    result = []
    i = 0
    pattern = ['loadMenu', '{', '"' + EDITOR_MENU + '"', '}']
    while i < len(ts):
        if ts[i:i + 4] == pattern:
            i += 4
        else:
            result.append(ts[i]); i += 1
    return result


def styled_override(data, relative):
    # Used solely for exact *freshness*. Behavior is separately compared to
    # raw production source by the independent semantic parser below.
    import restyle
    return restyle.menu(data.decode('latin-1'), Path(relative)).encode('latin-1')


def compare_semantics(reference, actual):
    reference=sem.layout_reference(reference,actual)
    expected = text_paths(reference)
    found = text_paths(actual)
    if expected != found:
        raise ValueError('Semantic package coverage differs: missing=' + str(sorted(expected-found)) +
                         ', extra=' + str(sorted(found-expected)))
    before_units = [sem.parse(p, reference[p]) for p in sorted(expected)]
    before, contracts = sem.signatures(before_units)
    names = set()
    for unit in before_units:
        for ts in [unit.body] + [d.body for d in unit.defines]:
            names.update(ts[i+1] for i,t in enumerate(ts[:-1]) if t.lower() == 'name')
    after_units=[sem.parse(p, actual[p]) for p in sorted(found)]
    after, _ = sem.signatures(sem.normalize_layout(after_units,before_units,contracts), contracts, names)
    differences = [p for p in sorted(expected) if before[p] != after[p]]
    if differences:
        raise ValueError('Package actions/cvars/labels/semantic data changed: ' + ', '.join(differences))
    return len(expected)


def verify(path, variant, baseline, repo=ROOT, transform=styled_override):
    repo = Path(repo)
    if variant not in ('game', 'editor'):
        raise ValueError('Unknown UI variant')
    original, edited = repo/'pak', repo/'ui-modern'
    editor_check = sem.compare(baseline, original, edited, require_styled=True)
    if not editor_check['passed']:
        raise ValueError('Edited source not accepted: ' + '; '.join(editor_check['errors']))
    archive_bytes = Path(path).read_bytes()
    files = read_archive(archive_bytes)
    try:
        manifest = json.loads(files['tce2_ui/package.json'])
    except (KeyError, ValueError, UnicodeDecodeError) as error:
        raise ValueError('Missing/invalid package manifest') from error
    if (not isinstance(manifest, dict) or manifest.get('schema') != 1 or manifest.get('variant') != variant or
            manifest.get('theme') != 'TCE2 Modern UI' or manifest.get('version') != '1.0.2' or
            manifest.get('all_original_menu_sources') != baseline['menu_count']):
        raise ValueError('Wrong UI package identity/coverage')
    inventory = manifest.get('files')
    if not isinstance(inventory, dict) or set(inventory) != set(files)-{'tce2_ui/package.json'}:
        raise ValueError('Manifest entry inventory differs')
    for name, row in inventory.items():
        if not isinstance(row, dict) or row.get('bytes') != len(files[name]) or row.get('sha256') != sha(files[name]):
            raise ValueError('Manifest byte fingerprint differs: ' + name)

    original_files = sem.source_files(original)
    theme = sem.source_files(edited, all_files=True)
    semantic_paths = text_paths(original_files)
    expected = {p:theme[p] for p in semantic_paths}
    reference = {p:original_files[p] for p in semantic_paths}
    if sem.PRIVATE_WIDGETS in theme:
        expected[sem.PRIVATE_WIDGETS]=theme[sem.PRIVATE_WIDGETS]
    if sem.APPEARANCE in theme:
        expected[sem.APPEARANCE]=theme[sem.APPEARANCE]
    exception_rows = manifest.get('normal_game_content_sources')
    game_sources = {}
    if variant == 'game':
        expected.pop(EDITOR_MENU); reference.pop(EDITOR_MENU)
        for name in LOAD_LISTS:
            key = 'ui/'+name
            if remove_editor_load(files[key]) != remove_editor_load(theme[key]):
                raise ValueError('Unrelated Game load-list change: ' + key)
            if sem.tokens(sem.strip_comments(files[key].decode('latin-1'))) != remove_editor_load(theme[key]):
                raise ValueError('Game load list still exposes authoring menu: ' + key)
            expected[key] = files[key]
            # These four references contain no script/paint data after removal.
            reference[key] = ' '.join(remove_editor_load(original_files[key])).encode('latin-1')
        if (not isinstance(exception_rows, list) or len(exception_rows) != len(OVERRIDES) or
                any(not isinstance(r, dict) for r in exception_rows) or
                {r.get('path') for r in exception_rows} != {'ui/'+n for n in OVERRIDES}):
            raise ValueError('Wrong/missing Game content exception inventory')
        for row in exception_rows:
            name = PurePosixPath(row['path']).name
            source = 'src/assets/ui/'+name
            if row.get('content_source') != source or not isinstance(row.get('reason'), str) or not row['reason']:
                raise ValueError('Unbound Game content exception: ' + row['path'])
            data = (repo/source).read_bytes()
            game_sources[source] = sha(data)
            reference[row['path']] = data
            expected[row['path']] = transform(data, row['path'])
    elif exception_rows != []:
        raise ValueError('Editor cannot claim Game content exceptions')

    assets = {p.relative_to(edited/'assets').as_posix():p.read_bytes()
              for p in sorted((edited/'assets').rglob('*')) if p.is_file()}
    try:
        art = json.loads(assets['tce2_ui/SOURCE.json'])
        fonts = json.loads(assets['fonts/tce2_ui/SOURCE.json'])
    except (KeyError, ValueError, UnicodeDecodeError) as error:
        raise ValueError('Missing/invalid art or font provenance') from error
    for group in (art, fonts):
        rows = group.get('files')
        if not isinstance(rows, list) or any(not isinstance(r,dict) for r in rows):
            raise ValueError('Wrong UI asset provenance inventory')
        for row in rows:
            name = row.get('path')
            if not safe_path(name) or name not in assets or row.get('sha256') != sha(assets[name]) or row.get('bytes') != len(assets[name]):
                raise ValueError('Missing/stale UI asset provenance: ' + str(name))
    expected.update(assets)
    aliases = art.get('native_aliases')
    if not isinstance(aliases, dict):
        raise ValueError('Missing native widget aliases')
    for target, source in aliases.items():
        if (not safe_path(target) or not safe_path(source) or not target.startswith('ui/assets/') or
                not source.startswith('tce2_ui/') or not target.endswith('.tga') or not source.endswith('.tga') or
                target in expected or source not in assets):
            raise ValueError('Unsafe/unbound native widget alias: ' + str(target))
        expected[target] = assets[source]
    if RUNTIME_MARKER[0] in expected:raise ValueError('Runtime marker must be the fixed package-owned entry')
    expected[RUNTIME_MARKER[0]]=RUNTIME_MARKER[1]
    payload = {p:d for p,d in files.items() if p != 'tce2_ui/package.json'}
    if payload != expected:
        missing = sorted(set(expected)-set(payload)); extra = sorted(set(payload)-set(expected))
        changed = sorted(p for p in set(payload)&set(expected) if payload[p] != expected[p])
        raise ValueError('Stale/unclassified package bytes: missing='+str(missing)+', extra='+str(extra)+', changed='+str(changed))
    semantic_count = compare_semantics(reference, payload)
    if variant == 'game':
        markers = ('tce_omnibot', 'omnibot_menu', 'omnibot_goalproperty', 'tce_navgen', 'wp_add', 'goal_edit', 'navstudio')
        for name in text_paths(payload):
            text = ' '.join(sem.tokens(sem.strip_comments(payload[name].decode('latin-1')))).lower()
            if any(marker in text for marker in markers):
                raise ValueError('Editor authoring reference in Game UI: ' + name)
    if sem.source_files(edited, all_files=True) != theme or sem.source_files(original) != original_files:
        raise ValueError('Theme/original source changed during package review')
    if any(sha((repo/p).read_bytes()) != h for p,h in game_sources.items()):
        raise ValueError('Game production content changed during review')
    if Path(path).read_bytes() != archive_bytes:
        raise ValueError('Archive changed during package review')
    return {'schema':1,'passed':True,'variant':variant,'package':str(Path(path).resolve()),'sha256':sha(archive_bytes),
            'entries':len(files),'menus':sum(p.endswith('.menu') for p in payload),'semantic_files':semantic_count,
            'original_menu_sources':baseline['menu_count'],'editor_source_gate':True,'game_content_sources':game_sources,
            'package_manifest_sha256':sha(files['tce2_ui/package.json']),
            'payload_sha256':{p:sha(data) for p,data in sorted(payload.items())},'input_stable':True,
            'limits':['Independent source semantics plus exact current bytes; no live input/rendering/engine preprocessing proof.',
                      'Game has exactly three explicit current production-source exceptions and one excluded authoring menu.']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--variant', choices=('game','editor'), required=True)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--repo', type=Path, default=ROOT)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    if any(args.out.resolve().is_relative_to((args.repo/folder).resolve()) for folder in ('pak','ui-modern')):
        p.error('Package receipt must be outside original/theme sources')
    if args.out.resolve() in (args.package.resolve(), args.baseline.resolve()):
        p.error('Package receipt cannot overwrite package/baseline input')
    try:
        result = verify(args.package, args.variant, json.loads(args.baseline.read_text()), args.repo)
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        result = {'passed':False,'errors':[str(error)]}
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('passed','variant','entries','menus','sha256','errors') if k in result}))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
