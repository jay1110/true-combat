"""Exercise broken page, anchor and download detection in the manual checker."""
from pathlib import Path
import tempfile
import unittest
from validate import Page, validate


class DocumentationLinks(unittest.TestCase):
    def test_missing_anchor_is_an_error_and_real_anchor_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'index.html').write_text('<html lang="en"><a href="other.html#target">Next</a>')
            other = root/'other.html'
            other.write_text('<html lang="en"><h1 id="wrong">Heading</h1>')
            errors, _, _ = validate(root)
            self.assertTrue(any('missing anchor' in e for e in errors))
            other.write_text('<html lang="en"><h1 id="target">Heading</h1>')
            self.assertEqual(validate(root)[0], [])

    def test_missing_download_and_language_are_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'index.html').write_text('<html lang="de"><a href="cvars.tsv">Download</a>')
            errors, _, _ = validate(root)
            self.assertTrue(any('Non-English' in e for e in errors))
            self.assertTrue(any('missing local target' in e for e in errors))

    def test_external_links_and_query_strings_are_handled(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'index.html').write_text('<html lang="en"><a href="https://example.org/#x">Web</a>'
                                          '<a href="cvars.tsv?download=1">Local</a>')
            (root/'cvars.tsv').write_text('name\tdefault\n')
            self.assertEqual(validate(root), ([], 1, 1))
        page = Page('<html lang="en"><h1 id="one">1</h1><a name="two"></a>')
        self.assertEqual(page.ids, {'one', 'two'})


if __name__ == '__main__':
    unittest.main()
