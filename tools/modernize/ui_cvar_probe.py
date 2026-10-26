#!/usr/bin/env python3
"""Compare the actual Win32 UI registration rows to the original TC:E UI DLL.

This is a bounded static data-table check, not UI controller/runtime parity.
Duplicate registrations are compared by row, including VM-binding presence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

import pefile

from animation_probe import ROOT, verify_map

ORIGINAL_UI_SHA256 = "785ee90d00952e8d86a5a7c8f8688e1bed868a3b00d93321604590cf93282781"


def rows(image: pefile.PE, address: int, count: int) -> list[dict]:
    if not 0 < count <= 4096:
        raise ValueError("UI registration count outside bounded inspection scope")
    result = []
    for index in range(count):
        vm, name, value, flags, _ = struct.unpack("<IIIII", image.get_data(address + index * 20, 20))
        def text(pointer: int) -> str:
            rva = pointer - image.OPTIONAL_HEADER.ImageBase
            if not 0 <= rva < image.OPTIONAL_HEADER.SizeOfImage:
                raise ValueError("Invalid UI registration string RVA")
            return image.get_string_at_rva(rva, max_length=4096).decode("ascii")
        result.append({"index": index, "vm": bool(vm), "name": text(name), "default": text(value), "flags": flags})
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "tcetest/ui_mp_x86.dll")
    parser.add_argument("--port", type=Path, default=ROOT / "build/sdk-win32/bin/Release/ui_mp_x86.dll")
    parser.add_argument("--map", type=Path, default=ROOT / "build/sdk-win32/ui-symbols-Release.map")
    parser.add_argument("--out", type=Path, default=ROOT / "build/runtime-parity-ui-cvars/ui-cvar-rows.json")
    args = parser.parse_args()
    original_digest = hashlib.sha256(args.original.read_bytes()).hexdigest()
    if original_digest != ORIGINAL_UI_SHA256:
        raise ValueError("Original UI identity differs from authenticated table RVA")
    verify_map(args.port, args.map)
    mapping = args.map.read_text(encoding="utf-8", errors="replace")
    preferred = int(re.search(r"Preferred load address is ([0-9a-fA-F]+)", mapping)[1], 16)
    def symbol(name: str) -> int:
        match = re.search(r"\s" + re.escape(name) + r"\s+([0-9a-fA-F]+)", mapping)
        if not match:
            raise ValueError("Required UI table symbol missing: " + name)
        return int(match[1], 16) - preferred
    native = pefile.PE(str(args.original))
    port = pefile.PE(str(args.port))
    count = struct.unpack("<I", port.get_data(symbol("_cvarTableSize"), 4))[0]
    expected = rows(native, 0x33260, 196)
    actual = rows(port, symbol("_cvarTable"), count)
    differences = [{"original": a, "port": b} for a, b in zip(expected, actual) if a != b]
    passed = len(expected) == len(actual) and not differences
    result = {"schema": 1, "passed_exact_rows": passed, "original_sha256": original_digest,
              "port_sha256": hashlib.sha256(args.port.read_bytes()).hexdigest(),
              "map_sha256": hashlib.sha256(args.map.read_bytes()).hexdigest(),
              "original_rows": expected, "port_rows": actual, "row_by_row_differences": differences,
              "limits": ["Static rows only; registration and consumer behavior require differential/runtime tests",
                         "Duplicate names are retained; symbol addresses and VM object addresses need not match"]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed_exact_rows": passed, "original_count": len(expected), "port_count": len(actual),
                      "differences": differences, "report": str(args.out)}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
