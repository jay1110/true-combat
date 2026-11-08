"""Guard address-specific tests and supply original .specs fixtures from PK3."""
import hashlib
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

root = Path(__file__).resolve().parents[2]
dll = root / 'tcetest/cgame_mp_x86.dll'
expected = 'd85a8e94da7c94956eff560e4cc406b4116f5a6ffd820b59accd9ad7e1e2414c'
if hashlib.sha256(dll.read_bytes()).hexdigest() != expected:
    raise SystemExit('Reference DLL differs: re-verify Ghidra addresses before testing.')
server = root / 'tcetest/qagame_mp_x86.dll'
if hashlib.sha256(server.read_bytes()).hexdigest() != 'f4f17855f568fcd9be760047943d50cfcb2e3bcf8691471172fe7771c2ae04c7':
    raise SystemExit('Server reference DLL differs; verify addresses before testing.')
def extract_media_fragments(archive, folder):
    # Preserve original bytes; identify nested view blocks without mistaking
    # braces inside comments or quoted paths for structure delimiters.
    token_pattern = re.compile(rb'//[^\r\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+', re.S)
    count = 0
    for name in archive.namelist():
        if not name.startswith('custom/default/weapons/') or not name.endswith('.weap'):
            continue
        data = archive.read(name)
        tokens = [m for m in token_pattern.finditer(data) if not m.group().startswith((b'//', b'/*'))]
        for i, token in enumerate(tokens):
            key = token.group().lower()
            if key not in (b'firstperson', b'thirdperson', b'modmodel'):
                continue
            if key == b'modmodel':
                if i + 2 >= len(tokens):
                    raise ValueError(f'Truncated modModel in {name}')
                start, end = tokens[i+1].start(), tokens[i+2].end()
                suffix = 'mod'
            else:
                if i + 1 >= len(tokens) or tokens[i+1].group() != b'{':
                    raise ValueError(f'Missing view brace in {name}')
                depth = 0
                for closing in tokens[i+1:]:
                    if closing.group() == b'{': depth += 1
                    elif closing.group() == b'}': depth -= 1
                    if depth == 0: break
                else: raise ValueError(f'Unclosed view in {name}')
                start, end = tokens[i+1].start(), closing.end()
                suffix = 'view1' if key == b'firstperson' else 'view0'
            (Path(folder) / f'media_{count:04d}.{suffix}').write_bytes(data[start:end])
            count += 1
    if not count:
        raise ValueError('No original media fragments found')

with tempfile.TemporaryDirectory(prefix='tce-specs-') as folder:
    count = 0
    with zipfile.ZipFile(root / 'tcetest/pak2.pk3') as archive:
        for name in archive.namelist():
            if name.startswith('custom/default/weapons/') and name.endswith('.specs'):
                (Path(folder) / Path(name).name).write_bytes(archive.read(name))
                count += 1
        extract_media_fragments(archive, folder)
        for index,name in enumerate(n for n in archive.namelist() if n.startswith('custom/default/weapons/') and n.endswith('.weap')):
            (Path(folder)/f'weapon_{index:03d}.weaptest').write_bytes(archive.read(name))
    if not count:
        raise SystemExit('Original weapon specifications missing from pak2.pk3')
    # Referenced animation files can live in different numbered PK3s.
    with zipfile.ZipFile(root / 'tcetest/pak2.pk3') as archive:
        config_paths = set()
        for name in archive.namelist():
            if name.startswith('custom/default/weapons/') and name.endswith('.weap'):
                config_paths.update(p.decode() for p in re.findall(rb'weaponConfig\s+"?([^"\s]+)', archive.read(name)))
    configs = {}
    characters = {}
    for pak in sorted((root / 'tcetest').glob('pak[0-9].pk3')):
        with zipfile.ZipFile(pak) as archive:
            for name in archive.namelist():
                if name.startswith('characters/') and name.endswith('.char'):
                    characters[name] = archive.read(name)
            for name in config_paths.intersection(archive.namelist()):
                configs[name] = archive.read(name)
    for index, (name, data) in enumerate(sorted(characters.items())):
        (Path(folder) / f'character_{index:03d}.chartest').write_bytes(data)
    if not characters:
        raise SystemExit('No character fixtures found')
    if not configs:
        raise SystemExit('No original weapon animation configs found')
    for index, (name, data) in enumerate(sorted(configs.items())):
        (Path(folder) / f'animation_{index:03d}.cfgtest').write_bytes(data)
        target=Path(folder)/name
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
    with zipfile.ZipFile(root / 'tcetest/pak3.pk3') as archive:
        gear = [n for n in archive.namelist() if n.startswith('maps/') and n.endswith('.gear')]
        if not gear:
            raise SystemExit('Original gear fixtures missing from pak3.pk3')
        for name in gear:
            (Path(folder) / Path(name).name).write_bytes(archive.read(name))
    result = subprocess.run([sys.argv[1], str(dll), folder, str(server)], check=False)
    raise SystemExit(result.returncode)
