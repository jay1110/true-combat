"""Negative/contract tests only; synthetic ZIPs do not establish release closure."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import closure_report as closure


class ClosureContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'flat.zip'

    def records(self):
        rows = []
        with zipfile.ZipFile(self.path, 'w') as zipped:
            for target in sorted(closure.target_table()):
                for role in ('cgame', 'ui', 'qagame', 'omnibot'):
                    name = target + '.' + role + '.so'
                    data = (target + '/' + role).encode()
                    zipped.writestr(name, data)
                    rows.append({'target': target, 'role': role, 'name': name,
                                 'archive_name': name, 'bytes': len(data),
                                 'sha256': hashlib.sha256(data).hexdigest()})
        return rows

    def test_flat_contract_counts_exact_entries_and_binds_bytes(self):
        rows = self.records()
        result = closure.flat_zip(self.path, rows)
        self.assertEqual(result['sha256'], closure.hash_file(self.path))
        rows[0]['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'byte mismatch'):
            closure.flat_zip(self.path, rows)

    def test_missing_target_role_is_not_green_status(self):
        rows = self.records()
        with self.assertRaisesRegex(ValueError, 'forty'):
            closure.flat_zip(self.path, rows[:-1])
        rows[0] = dict(rows[1])
        with self.assertRaisesRegex(ValueError, 'forty'):
            closure.flat_zip(self.path, rows)

    def test_extra_directory_or_metadata_rejected(self):
        rows = self.records()
        with zipfile.ZipFile(self.path, 'a') as zipped:
            zipped.writestr('metadata.json', '{}')
        with self.assertRaisesRegex(ValueError, 'closure mismatch'):
            closure.flat_zip(self.path, rows)

    def test_directory_prefixes_rejected(self):
        rows = self.records()
        rows[0]['archive_name'] = 'nested/' + rows[0]['archive_name']
        with self.assertRaisesRegex(ValueError, 'Noncanonical/nonflat'):
            closure.flat_zip(self.path, rows)

    def test_duplicate_zip_entry_rejected(self):
        rows = self.records()
        import warnings
        with warnings.catch_warnings(), zipfile.ZipFile(self.path, 'a') as zipped:
            warnings.simplefilter('ignore', UserWarning)
            zipped.writestr(rows[0]['archive_name'], b'duplicate')
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            closure.flat_zip(self.path, rows)

    def test_source_digest_is_not_only_git_head(self):
        root = Path(self.temp.name)
        (root / 'required.c').write_bytes(b'actual source')
        with patch.object(closure, 'release_paths', return_value=['required.c']):
            first = closure.source_identity(root)
            (root / 'required.c').write_bytes(b'changed source')
            second = closure.source_identity(root)
        self.assertNotEqual(first['sha256'], second['sha256'])

    def test_receipt_cannot_be_created_outside_manual_workflow(self):
        with patch.dict(closure.os.environ, {'GITHUB_EVENT_NAME': 'push'}):
            with self.assertRaisesRegex(ValueError, 'actual manual'):
                closure.ci_identity('release')

    def test_artifact_token_removed_on_redirect(self):
        import urllib.request
        request = urllib.request.Request('https://api.github.com/download',
                                         headers={'Authorization': 'Bearer do-not-forward'})
        redirected = closure.PublicRedirect().redirect_request(
            request, None, 302, 'Found', {}, 'https://storage.example/signed?x=1')
        self.assertIsNone(redirected.get_header('Authorization'))
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            closure.PublicRedirect().redirect_request(request, None, 302, 'Found', {}, 'http://storage.example/')


if __name__ == '__main__':
    unittest.main()
