"""Snapshot named PE/ELF/WASM exports and the mod's source entry signatures.

PE parsing uses the export directory, not byte-string presence. ELF/WASM use
an explicit nm executable. Source signatures are a separate check; exported
names alone cannot establish ABI or behavioral parity.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def pe_exports(data):
    pe = struct.unpack_from('<I', data, 60)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, sections = struct.unpack_from('<HH', data, pe + 4)
    opt_size = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    export_rva = struct.unpack_from('<I', data, opt + (112 if magic == 0x20b else 96))[0]
    section_start = opt + opt_size

    def offset(rva):
        for i in range(sections):
            header = section_start + i * 40
            size, address, raw_size, raw = struct.unpack_from('<IIII', data, header + 8)
            if address <= rva < address + max(size, raw_size):
                return raw + rva - address
        raise ValueError(f'RVA outside sections: {rva:#x}')

    names = []
    if export_rva:
        directory = offset(export_rva)
        count = struct.unpack_from('<I', data, directory + 24)[0]
        table = offset(struct.unpack_from('<I', data, directory + 32)[0])
        for i in range(count):
            start = offset(struct.unpack_from('<I', data, table + i * 4)[0])
            names.append(data[start:data.index(0, start)].decode('ascii'))
    return {'format': 'PE', 'machine': hex(machine), 'exports': sorted(names)}


def snapshot(root, files, nm=None):
    result = {'schema': 1, 'binaries': {}, 'entry_signatures': {}}
    for filename in files:
        path = Path(filename)
        data = path.read_bytes()
        if data[:2] == b'MZ':
            entry = pe_exports(data)
        elif nm:
            command = [nm, '--defined-only', '--extern-only', str(path)]
            output = subprocess.check_output(command, text=True)
            names = sorted({line.split()[-1] for line in output.splitlines() if line.split()})
            entry = {'format': 'WASM' if data[:4] == b'\0asm' else 'ELF', 'exports': names}
        else:
            raise ValueError(f'An explicit nm tool is required for {path}')
        entry.update(sha256=hashlib.sha256(data).hexdigest(), bytes=len(data))
        result['binaries'][path.name] = entry
    for module in ('cgame', 'ui', 'game'):
        for path in sorted((root / 'src' / module).glob('*.c')):
            text = path.read_text(encoding='utf-8-sig')
            # Exactly the two engine entry definitions; declarations/headers do
            # not satisfy this check. Preserve spelling and pointer width types.
            for match in re.finditer(r'(?m)^([^;{}\n]*\b(?:dllEntry|vmMain)\s*\([^;{}]*\))\s*\{', text):
                signature = re.sub(r'\s+', ' ', match.group(1)).strip()
                key = path.relative_to(root).as_posix() + ':' + re.search(r'\b(dllEntry|vmMain)\s*\(', signature).group(1)
                result['entry_signatures'][key] = signature
    if len(result['entry_signatures']) != 6:
        raise ValueError('Expected six native module entry definitions')
    return result


def compare(before, after):
    changes = []
    if before['entry_signatures'] != after['entry_signatures']:
        changes.append({'kind': 'entry_signature_change', 'before': before['entry_signatures'], 'after': after['entry_signatures']})
    for name, original in before['binaries'].items():
        current = after['binaries'].get(name)
        if current is None:
            changes.append({'binary': name, 'kind': 'missing'})
        elif original['exports'] != current['exports']:
            changes.append({'binary': name, 'kind': 'export_change',
                            'removed': sorted(set(original['exports']) - set(current['exports'])),
                            'added': sorted(set(current['exports']) - set(original['exports']))})
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--nm')
    parser.add_argument('binaries', nargs='+')
    args = parser.parse_args()
    result = snapshot(args.root.resolve(), args.binaries, args.nm)
    changes = compare(json.loads(args.baseline.read_text()), result) if args.baseline else []
    result['differences'] = changes
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'binaries': len(result['binaries']), 'entry_signatures': len(result['entry_signatures']), 'differences': len(changes)}))
    raise SystemExit(bool(changes))


if __name__ == '__main__':
    main()
