"""Guard address-specific tests and supply original .specs fixtures from PK3."""
import hashlib
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
with tempfile.TemporaryDirectory(prefix='tce-specs-') as folder:
    count = 0
    with zipfile.ZipFile(root / 'tcetest/pak2.pk3') as archive:
        for name in archive.namelist():
            if name.startswith('custom/default/weapons/') and name.endswith('.specs'):
                (Path(folder) / Path(name).name).write_bytes(archive.read(name))
                count += 1
    if not count:
        raise SystemExit('Original weapon specifications missing from pak2.pk3')
    with zipfile.ZipFile(root / 'tcetest/pak3.pk3') as archive:
        gear = [n for n in archive.namelist() if n.startswith('maps/') and n.endswith('.gear')]
        if not gear:
            raise SystemExit('Original gear fixtures missing from pak3.pk3')
        for name in gear:
            (Path(folder) / Path(name).name).write_bytes(archive.read(name))
    result = subprocess.run([sys.argv[1], str(dll), folder], check=False)
    raise SystemExit(result.returncode)
