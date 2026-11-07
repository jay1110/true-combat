#!/usr/bin/env python3
"""Validate the authoritative pak delta and optional Editor overlay.

This command never regenerates or rewrites installable resources. The original
game supplies stock files; its local reference is not a validation input.
Only --write-provenance updates the separate resource inventory metadata.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile

from editor_ui import validate_editor_ui

ROOT = Path(__file__).resolve().parents[2]
EDITOR_FILE = 'ui/tce_omnibot.menu'
NATIVE_OR_ARCHIVE = re.compile(r'\.(?:pk3|zip|7z|rar|dll|exe|dylib|wasm|so(?:\.[0-9.]+)?)$', re.I)
AUTHORING_UI = re.compile(rb'\b(?:tce_omnibot|tce_ob_[0-9]+|omnibot_menu|omnibot_goalname|omnibot_goalproperty)\b|\bbot\s+(?:waypoint\w*|goal_(?:create|save|edit)\w*|drawgoals)\b', re.I)
UI_FOLDERS = ('ui', 'ui_obj', 'ui_robj', 'ui_tdm')


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


def validate_public(files):
    for name, resource in files.items():
        if name.casefold() == EDITOR_FILE or NATIVE_OR_ARCHIVE.search(name):
            raise ValueError('Native/archive/editor resource in public pak: ' + name)
        if name.split('/')[0].casefold() in UI_FOLDERS and name.lower().endswith(('.menu', '.txt')):
            if AUTHORING_UI.search(resource.data()):
                raise ValueError('Authoring menu entry in public pak: ' + name)
        if name.casefold() == 'default.cfg' and re.search(rb'(?mi)(?:^|;)[\t ]*unbindall\b', resource.data()):
            raise ValueError('unbindall reset in canonical default.cfg')


def _release_reader():
    folder = str(ROOT / 'tools/release')
    if folder not in sys.path: sys.path.insert(0, folder)
    import resource_assets
    return resource_assets


def _identity(paths):
    return {name: (str(path), path.stat().st_dev, path.stat().st_ino,
                   path.stat().st_size, path.stat().st_mtime_ns, path.stat().st_ctime_ns)
            for name, path in paths.items()}


def plan(repo=ROOT):
    """Freeze and validate current canonical files without importing bytes."""
    repo = Path(repo).resolve()
    game_paths = tree_files(repo / 'pak')
    editor_paths = tree_files(repo / 'navstudio/editor-pak')
    identities = (_identity(game_paths), _identity(editor_paths))
    reader = _release_reader()
    stock, stock_sha = reader.base_game_reference(repo)
    release_version = stock['release_version']
    # Same path/native/stock-copy rules as production. No original, src/assets,
    # theme collector or DMS merge is consulted in this canonical reader.
    checked_game = reader.resource_paths('game', repo)
    checked_editor = reader.resource_paths('navstudio', repo)
    if checked_game != game_paths or checked_editor != game_paths | editor_paths:
        raise ValueError('Canonical resource paths changed during validation')
    game = {name: Resource(path, 'pak/' + name) for name, path in game_paths.items()}
    editor = {name: Resource(path, 'navstudio/editor-pak/' + name) for name, path in editor_paths.items()}
    validate_public(game)
    for name in editor:
        if NATIVE_OR_ARCHIVE.search(name):
            raise ValueError('Native/archive resource in Editor overlay: ' + name)
    reference_path = repo / 'tools/assets/base-ui-reference.json'
    if not reference_path.is_file() or reference_path.is_symlink():
        raise ValueError('Missing checked-in stock UI signature manifest')
    reference_bytes = reference_path.read_bytes()
    reference = json.loads(reference_bytes)
    ui = {name: path.read_bytes() for name, path in checked_editor.items()
          if name.split('/')[0] in UI_FOLDERS}
    closure = validate_editor_ui(ui, release_version=release_version, reference=reference)
    report = {'schema': 3, 'purpose': 'Canonical resource validation and provenance; no resource generation.',
              'game': 'pak', 'editor_overlay': 'navstudio/editor-pak', 'release_version': release_version,
              'game_files': {name: resource.fingerprint() for name, resource in sorted(game.items())},
              'editor_files': {name: resource.fingerprint() for name, resource in sorted(editor.items())},
              'editor_ui_closure': closure,
              'stock_reference': {'path': 'tools/assets/base-game-reference.json', 'sha256': stock_sha,
                                  'baseline_id': stock['baseline_id'], 'release_version': release_version},
              'stock_ui_reference': {'path': 'tools/assets/base-ui-reference.json', 'sha256': sha(reference_bytes)},
              'policy': 'Publish canonical new/changed resources only. Installed stock TCE is the base; local originals and historical authoring copies are never loaded.',
              'resource_generation': False}
    if identities != (_identity(tree_files(repo / 'pak')), _identity(tree_files(repo / 'navstudio/editor-pak'))):
        raise ValueError('Canonical resource files changed during validation')
    if reader.base_game_reference(repo)[1] != stock_sha or digest(reference_path) != sha(reference_bytes):
        raise ValueError('Stock signature metadata changed during validation')
    return game, editor, report


def prepare(repo=ROOT, check=False, replace=False, write_provenance=False):
    """Compatibility entry point: validate; optionally write metadata only."""
    if replace:
        raise ValueError('--replace was removed: edit authoritative pak/ or navstudio/editor-pak/ directly; resources are never regenerated')
    if check and write_provenance:
        raise ValueError('--check cannot write provenance')
    repo = Path(repo).resolve()
    game, editor, report = plan(repo)
    updated = False
    if write_provenance:
        target = repo / 'tools/assets/pak-sources.json'
        if target.is_symlink() or target.parent.resolve() != (repo / 'tools/assets').absolute():
            raise ValueError('Provenance path cannot resolve through a link')
        data = (json.dumps(report, indent=2, sort_keys=True) + '\n').encode('utf-8')
        if not target.is_file() or target.read_bytes() != data:
            handle, temporary = tempfile.mkstemp(prefix='.tce-provenance-', dir=target.parent)
            try:
                with os.fdopen(handle, 'wb') as stream: stream.write(data)
                os.replace(temporary, target)
            finally:
                if os.path.exists(temporary): os.unlink(temporary)
            updated = True
    return {'passed': True, 'check_only': not write_provenance, 'runtime_files_unchanged': True,
            'game_files': len(game), 'editor_files': len(editor), 'editor_ui_closure': report['editor_ui_closure'],
            'provenance': 'tools/assets/pak-sources.json' if write_provenance else None,
            'provenance_updated': updated}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=ROOT, help='Repository whose canonical resources are checked')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Validate without writing (the default)')
    mode.add_argument('--write-provenance', action='store_true', help='Update only tools/assets/pak-sources.json after successful validation')
    parser.add_argument('--replace', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        result = prepare(args.repo, check=args.check, replace=args.replace, write_provenance=args.write_provenance)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
