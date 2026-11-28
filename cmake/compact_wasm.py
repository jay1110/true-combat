"""Omit a side module's trailing zero data and explicitly clear it at start."""
import argparse
import io
import os
from pathlib import Path
import tempfile


def read_uint(stream):
    value = 0
    for shift in range(0, 35, 7):
        byte = stream.read(1)
        if not byte:
            break
        value |= (byte[0] & 127) << shift
        if byte[0] < 128:
            return value
    raise ValueError("Invalid wasm32 LEB128")


def leb(value, signed=False):
    out = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        if value == 0 and (not signed or byte < 64):
            return bytes(out) + bytes([byte])
        out.append(byte | 128)


def compact(module):
    stream = io.BytesIO(module)
    header = stream.read(8)
    if header != b'\0asm\1\0\0\0':
        raise ValueError("Expected WebAssembly version 1")
    sections = []
    by_id = {}
    while section := stream.read(1):
        size = read_uint(stream)
        payload = stream.read(size)
        if len(payload) != size:
            raise ValueError("Truncated section")
        sections.append((section, payload))
        by_id[section[0]] = payload

    # LLD emits one active segment relative to the module's imported memory base.
    # Other segment layouts need no transformation here.
    if 11 not in by_id:
        return module
    data = io.BytesIO(by_id[11])
    if read_uint(data) != 1 or read_uint(data) != 0 or data.read(1) != b'\x23':
        return module
    base = read_uint(data)
    if data.read(1) != b'\x0b':
        return module
    prefix = by_id[11][:data.tell()]
    size = read_uint(data)
    raw = data.read()
    if len(raw) != size:
        raise ValueError("Unexpected active data size")
    kept = raw.rstrip(b'\0')
    if size - len(kept) < 4096:
        return module

    # Append a private () -> () start function. All existing function indices,
    # dylink allocation sizes, data addresses and relocation tables stay valid.
    # WebAssembly applies active data before start, including on reused memory.
    types = io.BytesIO(by_id[1])
    void_type = read_uint(types)
    functions = io.BytesIO(by_id[3])
    count = read_uint(functions)
    import_functions = 0
    imports = io.BytesIO(by_id[2])
    for _ in range(read_uint(imports)):
        for _ in range(2):
            imports.read(read_uint(imports))
        kind = imports.read(1)
        if kind == b'\0':
            read_uint(imports)
            import_functions += 1
        elif kind == b'\x03':
            imports.read(2)
        elif kind in (b'\x01', b'\x02'):
            if kind == b'\x01':
                imports.read(1)
            flags = read_uint(imports)
            if flags not in (0, 1, 3):
                raise ValueError("Unsupported wasm32 import limits")
            read_uint(imports)
            if flags & 1:
                read_uint(imports)
        elif kind == b'\x04':
            read_uint(imports)
            read_uint(imports)
        else:
            raise ValueError("Unsupported WebAssembly import")
    body = (b'\0\x23' + leb(base) + b'\x41' + leb(len(kept), True) +
            b'\x6a\x41\0\x41' + leb(size - len(kept), True) + b'\xfc\x0b\0')
    if 8 in by_id:
        body += b'\x10' + by_id[8]
    body += b'\x0b'
    code = io.BytesIO(by_id[10])
    if read_uint(code) != count:
        raise ValueError("Function/code count mismatch")
    changes = {
        1: leb(void_type + 1) + types.read() + b'\x60\0\0',
        3: leb(count + 1) + functions.read() + leb(void_type),
        8: leb(import_functions + count),
        10: leb(count + 1) + code.read() + leb(len(body)) + body,
        11: prefix + leb(len(kept)) + kept,
    }
    if 8 not in by_id:
        index = next(i for i, (tag, _) in enumerate(sections) if tag[0] in (9, 12, 10))
        sections.insert(index, (b'\x08', b''))
    result = bytearray(header)
    for tag, payload in sections:
        payload = changes.get(tag[0], payload)
        result.extend(tag + leb(len(payload)) + payload)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module', type=Path)
    path = parser.parse_args().module
    source = path.read_bytes()
    output = compact(source)
    if len(output) < len(source):
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temp:
            temp.write(output)
        os.chmod(temp.name, path.stat().st_mode)
        os.replace(temp.name, path)
        print(f'{path.name}: {len(source)} -> {len(output)} bytes')
