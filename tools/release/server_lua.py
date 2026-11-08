"""Stage the reviewed server-script defaults, never a developer's local state."""
import json
from pathlib import Path, PurePosixPath
import shutil


MANIFEST = 'tools/release/server-lua.json'
SUFFIXES = {'.lua', '.cfg', '.md', '.txt', '.sql', '.toml'}


def payload_paths(repo):
    repo = Path(repo).resolve()
    manifest = json.loads((repo / MANIFEST).read_text(encoding='utf-8'))
    names = manifest.get('files')
    if (manifest.get('schema') != 1 or not isinstance(names, list) or
            not all(isinstance(name, str) for name in names) or
            names != sorted(set(names)) or not names):
        raise ValueError('Invalid reviewed server Lua payload manifest')
    source = (repo / 'src/lua').resolve()
    if not source.is_relative_to(repo):
        raise ValueError('Escaped server Lua source root')
    result = []
    for name in names:
        relative = PurePosixPath(name)
        if (relative.is_absolute() or str(relative) != name or '\\' in name or ':' in name or
                '..' in relative.parts or relative.suffix.lower() not in SUFFIXES):
            raise ValueError('Unsafe server Lua payload path: ' + name)
        path = source / name
        if not path.resolve().is_relative_to(source) or not path.is_file():
            raise ValueError('Missing/escaped reviewed server Lua payload: ' + name)
        result.append((name, path))
    return result


def stage_server_lua(repo, destination):
    paths = payload_paths(repo)  # Validate the entire closure before writing.
    for name, path in paths:
        target = Path(destination) / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    return [name for name, _ in paths]
