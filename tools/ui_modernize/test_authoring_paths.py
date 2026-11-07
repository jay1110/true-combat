"""Real CLI argument paths keep generation separate from committed resources."""
from pathlib import Path
import unittest
import generate_assets as art
import generate_fonts as fonts
import restyle


class AuthoringPaths(unittest.TestCase):
    def test_no_argument_generation_stages_outside_canonical_resources(self):
        for generator,folder in ((art,'ui-assets'),(fonts,'ui-fonts')):
            with self.subTest(generator=generator.__name__):
                self.assertEqual(generator.arguments([]).out,generator.ROOT/'build'/folder)
                self.assertNotEqual(generator.arguments([]).out,generator.ASSETS)
        self.assertEqual(restyle.DEST,restyle.ROOT/'build/ui-style')

    def test_read_only_check_targets_committed_assets_without_freetype_or_generation(self):
        for generator in (art,fonts):
            with self.subTest(generator=generator.__name__):
                options=generator.arguments(['--check'])
                self.assertTrue(options.check)
                self.assertEqual(options.out,generator.ROOT/'pak')
                self.assertEqual(generator.arguments(['--check','--out','custom']).out,Path('custom'))
                self.assertEqual(generator.arguments(['--out','staging']).out,Path('staging'))


if __name__=='__main__':unittest.main()
