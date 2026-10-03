"""Inventory supplied ELF32 symbols and their SDK references (stdlib only).

Names present in the SDK do NOT prove identical implementation or ABI.
Run from anywhere; output defaults to src/reconstruction/inventory/.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def elf_symbols(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x01\x01':
        raise ValueError(f'{path}: expected little-endian ELF32')
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    sections = [struct.unpack_from('<IIIIIIIIII', data, h[6] + i * h[11])
                for i in range(h[12])]
    rows = []
    # Prefer the full symbol table, including static functions and data.
    tables = [s for s in sections if s[1] == 2]
    if not tables:
        tables = [s for s in sections if s[1] == 11]
    for table in tables:
        strings_section = sections[table[6]]
        strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
        source_file = None
        for offset in range(table[4], table[4] + table[5], table[9]):
            name_offset, address, size, info, other, section = struct.unpack_from('<IIIBBH', data, offset)
            name = strings[name_offset:].split(b'\0')[0].decode('utf-8', errors='replace')
            kind = info & 15
            if kind == 4:
                source_file = name
            if not name or not section or kind not in (1, 2):
                continue
            rows.append(dict(name=name, address=f'0x{address:08x}', size=size,
                             kind='function' if kind == 2 else 'object',
                             binding=info >> 4,
                             compilation_unit=source_file if info >> 4 == 0 else None))
    return rows


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=root / 'src/reconstruction/inventory')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    references = {}
    for folder in ('game', 'cgame', 'ui', 'botai'):
        for path in (root / 'src' / folder).glob('*'):
            if path.suffix not in ('.c', '.h') or path.name.startswith('tce_'):
                continue
            for name in set(re.findall(r'\b([A-Za-z_]\w*)\s*\(', path.read_text(errors='replace'))):
                references.setdefault(name, []).append(path.relative_to(root).as_posix())
    summary = {}
    for module in ('qagame', 'cgame', 'ui'):
        elf = root / 'tcetest' / (module + '.mp.i386.so')
        dll = root / 'tcetest' / (module + '_mp_x86.dll')
        symbols = elf_symbols(elf)
        for row in symbols:
            if row['kind'] == 'function':
                row['sdk_name_references'] = sorted(references.get(row['name'], []))
        functions = [s for s in symbols if s['kind'] == 'function']
        absent = [s for s in functions if not s['sdk_name_references']]
        report = dict(module=module,
                      inputs={p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in (elf, dll)}, symbols=symbols)
        (args.output / (module + '.json')).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        summary[module] = dict(functions=len(functions), objects=len(symbols)-len(functions),
                               function_names_absent_from_sdk=len(absent))
        (args.output / (module + '-missing-names.txt')).write_text(
            '\n'.join(sorted(s['name'] for s in absent)) + '\n', encoding='utf-8')
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
