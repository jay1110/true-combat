"""Exercise compact side modules with nonzero imported memory and an old start."""
import base64
import shutil
import subprocess
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'cmake'))
from compact_wasm import compact, leb


def section(tag, payload):
    return bytes([tag]) + leb(len(payload)) + payload


class CompactWasm(unittest.TestCase):
    @unittest.skipUnless(shutil.which('node'), 'Node required for WebAssembly execution')
    def test_dirty_memory_and_start_order(self):
        imports = b'\x02\x01m\x03mem\x02\x00\x01\x01m\x04base\x03\x7f\x00'
        for with_start in (False, True):
            # An existing start writes 42 to the first omitted byte.
            body = bytes.fromhex('00 23 00 41 2a 3a 00 01 0b')
            original = (b'\0asm\1\0\0\0' + section(1, bytes.fromhex('01 60 00 00')) +
                        section(2, imports) + section(3, b'\1\0') +
                        (section(8, b'\0') if with_start else b'') +
                        section(10, b'\1' + leb(len(body)) + body) +
                        section(11, b'\1\0\x23\0\x0b' + leb(8193) + b'x' + bytes(8192)))
            small = compact(original)
            self.assertLess(len(small), 256)
            self.assertEqual(compact(small), small)
            script = """
              function run(data) {
                const mem = new WebAssembly.Memory({initial:1});
                const bytes = new Uint8Array(mem.buffer);
                bytes.fill(165);
                new WebAssembly.Instance(new WebAssembly.Module(Buffer.from(data,'base64')),
                                         {m:{mem,base:128}});
                return Buffer.from(bytes);
              }
              if (!run(process.argv[1]).equals(run(process.argv[2]))) process.exit(1);
            """
            subprocess.run(['node', '-e', script, base64.b64encode(original).decode(),
                            base64.b64encode(small).decode()], check=True)
