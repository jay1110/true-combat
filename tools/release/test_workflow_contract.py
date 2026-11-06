"""Final mod ZIPs must never be offered after a failed closure check."""
import unittest

from package import ROOT
from validate import release_upload_policy


class ReleaseUploadPolicy(unittest.TestCase):
    def setUp(self):
        self.source = (ROOT / '.github/workflows/release.yml').read_text(encoding='utf8')

    def test_current_workflow_requires_success_before_mod_zip_upload(self):
        release_upload_policy(self.source)

    def test_always_upload_after_failed_closure_is_rejected(self):
        unsafe = self.source.replace('if: success()', 'if: always()')
        self.assertNotEqual(unsafe, self.source)
        with self.assertRaisesRegex(ValueError, 'successful release closure'):
            release_upload_policy(unsafe)

    def test_diagnostic_upload_cannot_reintroduce_failed_zip(self):
        unsafe = self.source.replace('out/closure/*.json', 'out/closure/*.zip')
        self.assertNotEqual(unsafe, self.source)
        with self.assertRaisesRegex(ValueError, 'must not upload installable ZIPs'):
            release_upload_policy(unsafe)


if __name__ == '__main__':
    unittest.main()
