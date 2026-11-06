#!/usr/bin/env python3
"""Deliberately prepare the editable resource delta, independently of builds.

The original tree is read-only input. This tool is never an implicit build step:
once prepared, pak is the editable source consumed by the release packager.
"""
import argparse
from dataclasses import dataclass
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tempfile

from editor_ui import read_catalog, merge_editor

ROOT = Path(__file__).resolve().parents[2]
EDITOR_FILE = 'ui/tce_omnibot.menu'
NATIVE_OR_ARCHIVE = re.compile(r'\.(?:pk3|zip|7z|rar|dll|exe|dylib|wasm|so(?:\.[0-9.]+)?)$', re.I)
AUTHORING_UI = re.compile(rb'\b(?:tce_omnibot|tce_ob_[0-9]+|omnibot_menu|omnibot_goalname|omnibot_goalproperty)\b|\bbot\s+(?:waypoint\w*|goal_(?:create|save|edit)\w*|drawgoals)\b', re.I)
OBSOLETE_RESOURCES = {'readme.txt':'1b4eb7877216255e4c6d7751bc569c97a183841fc34bc1442ad6d1a22838ea54'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def safe_name(name):
    if (not isinstance(name, str) or not name or '\\' in name or ':' in name or
            any(ord(char) < 32 for char in name)):
        raise ValueError('Unsafe resource path: ' + repr(name))
    path = PurePosixPath(name)
    if not path.parts or path.is_absolute() or '..' in path.parts or name != path.as_posix():
        raise ValueError('Unsafe resource path: ' + repr(name))
    return name


def os_metadata(name):
    """Only known Finder/Explorer metadata, not arbitrary hidden resources."""
    parts = PurePosixPath(name).parts
    return (parts[-1].casefold() in ('.ds_store', 'thumbs.db') or
            any(part.casefold() == '__macosx' for part in parts))


def tree_files(root):
    root = Path(root)
    if root.is_symlink() or getattr(root, 'is_junction', lambda: False)():
        raise ValueError('Resource tree cannot be a link: ' + str(root))
    if not root.exists():
        return {}
    if not root.is_dir():
        raise ValueError('Resource tree is not a directory: ' + str(root))
    files, folded = {}, set()
    for directory, dirs, names in os.walk(root, followlinks=False):
        for name in dirs + names:
            child = Path(directory) / name
            if child.is_symlink() or getattr(child, 'is_junction', lambda: False)():
                raise ValueError('Linked resource is not a canonical input: ' + str(Path(directory) / name))
        for name in names:
            path = Path(directory) / name
            relative = safe_name(path.relative_to(root).as_posix())
            if relative.casefold() in folded:
                raise ValueError('Case-colliding resource: ' + relative)
            folded.add(relative.casefold())
            files[relative] = path
    return files


@dataclass
class Resource:
    value: object
    source: str
    checksum: str = None

    def data(self):
        return self.value if isinstance(self.value, bytes) else self.value.read_bytes()

    def fingerprint(self):
        if self.checksum is None:
            self.checksum = sha(self.value) if isinstance(self.value, bytes) else digest(self.value)
        return {'source': self.source, 'bytes': len(self.value) if isinstance(self.value, bytes) else self.value.stat().st_size,
                'sha256': self.checksum}


def collect_theme(variant, repo):
    """Use the same validated Game/editor transformations as theme packaging."""
    tools = repo / 'tools/ui_modernize'
    sys.path.insert(0, str(tools))
    try:
        spec = importlib.util.spec_from_file_location('tce_prepare_pak_theme', tools / 'package.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        if not callable(getattr(module, 'collect', None)):
            raise ValueError('Theme package.py must expose collect(variant) before preparing pak')
        return module.collect(variant)
    finally:
        sys.path.pop(0)


def without_unbindall(data):
    """Remove the original standalone reset, preserving every other byte."""
    clean, count = re.subn(rb'(?mi)^[\t ]*unbindall[\t ]*(?://[^\r\n]*)?(?:\r?\n|$)', b'', data)
    if re.search(rb'(?mi)(?:^|;)[\t ]*unbindall\b', clean):
        raise ValueError('Unclassified unbindall command in default.cfg')
    return clean, count


def validate_public(files):
    for name, resource in files.items():
        if name.casefold() == EDITOR_FILE or NATIVE_OR_ARCHIVE.search(name):
            raise ValueError('Native/archive/editor resource in public pak: ' + name)
        if name.split('/')[0].casefold() in ('ui', 'ui_obj', 'ui_robj', 'ui_tdm') and name.lower().endswith(('.menu', '.txt')):
            if AUTHORING_UI.search(resource.data()):
                raise ValueError('Authoring menu entry in public pak: ' + name)


def stock_reference(repo):
    path = repo/'tools/assets/base-game-reference.json'
    if not path.is_file(): raise ValueError('Missing authenticated stock resource reference: '+str(path))
    value = json.loads(path.read_bytes())
    match = re.search(r'^#define TCE_VERSION "([^"]+)"', (repo/'src/game/tce_version.h').read_text(), re.M)
    if (value.get('schema') != 1 or not match or value.get('release_version') != match[1]
            or not isinstance(value.get('files'), dict) or not value.get('baseline_id')):
        raise ValueError('Wrong version-bound stock resource reference')
    result = {}
    for name, item in value['files'].items():
        safe_name(name)
        if (name.casefold() in result or not re.fullmatch(r'[0-9a-f]{64}',item.get('sha256','')) or
                not isinstance(item.get('bytes'),int) or item['bytes'] < 0):
            raise ValueError('Invalid/case-colliding stock fingerprint: '+name)
        result[name.casefold()] = item
    return result, {'path':'tools/assets/base-game-reference.json','sha256':digest(path),
                    'baseline_id':value['baseline_id'],'release_version':value['release_version']}


def same_stock(name, resource, stock):
    original = stock.get(name.casefold())
    if original is None: return False
    actual = resource.fingerprint()
    return actual['bytes'] == original['bytes'] and actual['sha256'] == original['sha256']


def plan(repo=ROOT, collector=None):
    """Return frozen source records without writing or building anything."""
    repo = Path(repo).resolve()
    original = repo / 'pak_original_tce'
    if not original.is_dir():
        raise ValueError('Missing read-only original input: ' + str(original))
    stock, stock_identity = stock_reference(repo)
    collector = collector or (lambda variant: collect_theme(variant, repo))
    result, skipped, overrides = {}, [], []
    for folder in ('pak_original_tce', 'src/assets'):
        for name, path in sorted(tree_files(repo / folder).items()):
            if os_metadata(name):
                skipped.append({'path': folder + '/' + name, 'reason': 'operating-system metadata (.DS_Store/Thumbs.db/__MACOSX)'})
                continue
            if name.casefold() in OBSOLETE_RESOURCES and digest(path) == OBSOLETE_RESOURCES[name.casefold()]:
                skipped.append({'path': folder+'/'+name, 'reason':'obsolete hash-identified ServerListFix accompanying readme'})
                continue
            if name.casefold() == EDITOR_FILE or NATIVE_OR_ARCHIVE.search(name):
                skipped.append({'path': folder + '/' + name, 'reason': 'archive/native/editor-only'})
                continue
            if name in result:
                overrides.append({'path': name, 'previous': result[name].source, 'source': folder + '/' + name})
            result[name] = Resource(path, folder + '/' + name)
    theme = {}
    for variant in ('game', 'editor'):
        files = collector(variant)
        if not isinstance(files, dict):
            raise ValueError('Theme collector must return resource bytes')
        theme[variant] = {}
        folded = set()
        for name, data in files.items():
            safe_name(name)
            if os_metadata(name):
                raise ValueError('Theme collector emitted operating-system metadata: ' + name)
            if not isinstance(data, bytes) or name.casefold() in folded or NATIVE_OR_ARCHIVE.search(name):
                raise ValueError('Invalid/colliding theme payload: ' + name)
            folded.add(name.casefold())
            theme[variant][name] = data
    for name, data in theme['game'].items():
        source = 'ui-modern via package.collect(game)'
        if name in result:
            overrides.append({'path': name, 'previous': result[name].source, 'source': source})
        result[name] = Resource(data, source)
    resets = 0
    if 'default.cfg' in result:
        before = result['default.cfg']
        data, resets = without_unbindall(before.data())
        result['default.cfg'] = Resource(data, before.source + ' (standalone unbindall removed)')
    if len(result) != len({name.casefold() for name in result}):
        raise ValueError('Overlay creates case-colliding pak resources')
    validate_public(result)
    # Only UI files and the already-frozen theme are needed for the menu merge;
    # do not materialize the complete original map/texture tree in memory.
    game_ui = {name: resource.data() for name, resource in result.items()
               if name.startswith(('ui/', 'ui_obj/', 'ui_robj/', 'ui_tdm/'))}
    for name, data in theme['game'].items():
        if name not in game_ui: game_ui[name] = data
    catalog = read_catalog(repo)
    editor_bytes, editor_origins, editor_closure = merge_editor(game_ui, theme['game'], theme['editor'], catalog)
    editor = {name: Resource(data, editor_origins[name]) for name, data in editor_bytes.items()
              if (name not in result or data != result[name].data()) and
              not same_stock(name, Resource(data, editor_origins[name]), stock)}
    # This local reference is not pure stock: retain its project additions and
    # user-supplied replacements. Only authenticated base-pak byte equality can
    # remove a resource from the installable delta.
    unchanged = {name:resource for name,resource in result.items() if same_stock(name,resource,stock)}
    result = {name:resource for name,resource in result.items() if name not in unchanged}
    for name,resource in sorted(unchanged.items()):
        skipped.append({'path':name,'reason':'byte-identical authenticated stock resource','source':resource.source})
    # Recheck installable deltas against metadata, without borrowing original
    # menu bytes. This is the same closure gate used by CI/release packaging.
    from editor_ui import validate_editor_ui
    effective_delta_ui = {name:resource.data() for name,resource in (result|editor).items()
                          if name.startswith(('ui/','ui_obj/','ui_robj/','ui_tdm/'))}
    reference_path = repo/'tools/assets/base-ui-reference.json'
    reference = json.loads(reference_path.read_bytes()) if reference_path.is_file() else None
    editor_closure = validate_editor_ui(effective_delta_ui,release_version=stock_identity['release_version'],reference=reference)
    report = {'schema': 2, 'purpose': 'Explicit delta regeneration only; release builds package editable pak without reapplying overlays.',
              'original': 'pak_original_tce', 'game': 'pak', 'editor_overlay': 'navstudio/editor-pak',
              'game_files': {name: resource.fingerprint() for name, resource in sorted(result.items())},
              'editor_files': {name: resource.fingerprint() for name, resource in sorted(editor.items())},
              'overrides': overrides, 'excluded': skipped, 'unbindall_commands_removed': resets,
              'native_editor_catalog_files':len(catalog), 'editor_ui_closure':editor_closure,
              'stock_reference':stock_identity, 'policy':'Publish new/changed project resources only; local reference bytes are not the stock baseline.',
              'unchanged_stock_resources_omitted':len(unchanged)}
    return result, editor, report


def differences(root, expected):
    actual = tree_files(root)
    return {'missing': sorted(set(expected) - set(actual)), 'extra': sorted(set(actual) - set(expected)),
            'changed': sorted(name for name in set(expected) & set(actual)
                              if digest(actual[name]) != expected[name].fingerprint()['sha256'])}


def changed(diff):
    return any(diff.values())


def write_resource(destination, resource):
    """Atomic replacement also breaks any pre-existing accidental hard link."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix='.tce-pak-', dir=destination.parent)
    try:
        with os.fdopen(handle, 'wb') as stream:
            if isinstance(resource.value, bytes):
                stream.write(resource.value)
            else:
                with resource.value.open('rb') as source:
                    shutil.copyfileobj(source, stream, 1024 * 1024)
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def prepare(repo=ROOT, check=False, replace=False, collector=None):
    repo = Path(repo).resolve()
    game, editor, report = plan(repo, collector)
    targets = ((repo / 'pak', game), (repo / 'navstudio/editor-pak', editor))
    for target, _ in targets:
        if target.is_symlink() or target.resolve() != target.absolute():
            raise ValueError('Canonical output cannot resolve through a link: ' + str(target))
        if not target.resolve().is_relative_to(repo):
            raise ValueError('Canonical output outside repository')
    before = {target.relative_to(repo).as_posix(): differences(target, expected) for target, expected in targets}
    if check:
        return {'passed': not any(changed(diff) for diff in before.values()), 'check_only': True, 'differences': before,
                'game_files': len(game), 'editor_files': len(editor)}
    # Make the destructive/replacement choice before writing either output.
    for target, _ in targets:
        if tree_files(target) and changed(before[target.relative_to(repo).as_posix()]) and not replace:
            raise ValueError('Existing editable files differ: ' + str(target) + '; preserve your edits or deliberately use --replace')
    for target, expected in targets:
        diff = before[target.relative_to(repo).as_posix()]
        for name in diff['extra']:
            (target / name).unlink()  # Only explicit --replace reaches an existing mismatching tree.
        for name in diff['missing'] + diff['changed']:
            write_resource(target / name, expected[name])
        after = differences(target, expected)
        if changed(after):
            raise ValueError('Prepared resource fingerprint mismatch: ' + str(after))
    report_path = repo / 'tools/assets/pak-sources.json'
    data = (json.dumps(report, indent=2, sort_keys=True) + '\n').encode('utf-8')
    write_resource(report_path, Resource(data, 'prepare_pak'))
    return {'passed': True, 'check_only': False, 'game_files': len(game), 'editor_files': len(editor),
            'provenance': 'tools/assets/pak-sources.json', 'differences_before': before}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Compare paths/hashes only; never write')
    mode.add_argument('--replace', action='store_true', help='Deliberately replace edited/stale canonical files with the source overlays')
    args = parser.parse_args()
    try:
        result = prepare(check=args.check, replace=args.replace)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
