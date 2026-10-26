"""Selection-only regression: a rerun must not ingest its prior output."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from aggregate import main
from package import version


class AggregateRerun(unittest.TestCase):
    def test_prior_aggregate_is_excluded(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            inputs, output = root / 'collected', root / 'output'
            for variant in ('game', 'navstudio'):
                name = f'TCE2-{version()}-windows-x86-{variant}'
                folder = inputs / name
                folder.mkdir(parents=True)
                with zipfile.ZipFile(folder / (name + '.zip'), 'w') as archive:
                    archive.writestr('manifest.json', json.dumps({
                        'target': 'windows-x86', 'variant': variant, 'gameplay_bot_runtime': True}))
            source = inputs / f'TCE2-{version()}-Source'
            source.mkdir()
            with zipfile.ZipFile(source / f'TCE2-{version()}-Source.zip', 'w') as archive:
                archive.writestr('selection-fixture.txt', 'No binary or source closure is asserted by this unit test.')
            previous = inputs / f'TCE2-{version()}-Complete-Release'
            previous.mkdir()
            with zipfile.ZipFile(previous / f'TCE2-{version()}-INCOMPLETE-Build-Artifacts.zip', 'w') as archive:
                archive.writestr('old.txt', 'Previous cancelled attempt')
            needs = {'plan': {'result': 'success', 'outputs': {'matrix': json.dumps({'include': [
                {'target': 'windows-x86', 'variant': v} for v in ('game', 'navstudio')]})}},
                'binaries': {'result': 'success'}, 'navstudio-desktop': {'result': 'skipped'}}
            with patch.object(sys, 'argv', ['aggregate', '--input', str(inputs), '--out', str(output)]), \
                    patch.dict(os.environ, {'NEEDS_JSON': json.dumps(needs), 'GITHUB_SHA': 'selection-test'}):
                self.assertEqual(main(), 0)
            manifest = json.loads((output / 'release-manifest.json').read_text())
            self.assertIs(manifest['complete'], True)
            archives = [r['path'] for r in manifest['files'] if r['path'].endswith('.zip')]
            self.assertEqual(len(archives), 3)
            self.assertFalse(any('INCOMPLETE' in p or 'Complete-Release' in p for p in archives))
            self.assertEqual(len(manifest['collected_targets']), 2)


if __name__ == '__main__':
    unittest.main()
