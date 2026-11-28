"""Check the cross-platform ordering of pinned vendor snapshot records."""
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class VendorSnapshotTest(unittest.TestCase):
    def test_mixed_case_component_order_and_modified_bytes(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            package = root / 'vendor' / 'example'
            # The pinned order compares path components case-insensitively.
            # In particular, the 'a' directory precedes its sibling 'a.txt'.
            names = ['a/z.c', 'a.txt', 'B.c']
            records = []
            for name in reversed(names):
                path = package / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(name.encode())
            for name in names:
                digest = hashlib.sha256(name.encode()).hexdigest()
                records.append(f'{name}\0{digest}\n')
            expected = hashlib.sha256(''.join(records).encode()).hexdigest()
            lock = {'packages': [{'path': 'vendor/example', 'snapshot': {
                'files': len(names), 'sha256': expected}}]}
            (root / 'vendor/dependencies.lock.json').write_text(json.dumps(lock))
            output = root / 'result.json'
            command = [sys.executable, str(Path(__file__).with_name('vendor_check.py')),
                       '--root', str(root), '--out', str(output)]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(json.loads(output.read_text())[0]['sha256'], expected)
            (package / 'B.c').write_bytes(b'changed')
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 1)
            self.assertFalse(json.loads(output.read_text())[0]['matches'])


if __name__ == '__main__':
    unittest.main()
