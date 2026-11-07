"""Synthetic aggregation contracts; real compiler/runtime closure is separate."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import aggregate
from client_packages import target_table
from package import version
from test_shared_clients import fixture_repo,input_archive,synthetic_runtime


class AggregateRerun(unittest.TestCase):
    def run_case(self,variants=('game',),missing=False):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        root=Path(temporary.name);repo=fixture_repo(root/'repo');inputs=root/'collected';output=root/'output'
        for target in target_table(repo):
            for variant in variants:
                if missing and target=='windows-x86' and variant=='game':continue
                input_archive(inputs/(target+'-'+variant),target,variant,repo=repo)
        source=inputs/f'TCE2-{version(repo)}-Source';source.mkdir()
        with zipfile.ZipFile(source/f'TCE2-{version(repo)}-Source.zip','w') as z:z.writestr('fixture.txt','source selection only')
        for suffix in ('Complete-Release','Mod-Packages','Release-Closure','Failed-Packaging-Diagnostics'):
            previous=inputs/f'TCE2-{version(repo)}-{suffix}';previous.mkdir()
            with zipfile.ZipFile(previous/'old-Client-Inputs.zip','w') as z:z.writestr('old.txt','previous aggregate result')
        needs={'plan':{'result':'success','outputs':{'matrix':json.dumps({'include':[
            {'target':target,'variant':variant} for target in target_table(repo) for variant in variants]})}},
            'binaries':{'result':'failure' if missing else 'success'},'navstudio-desktop':{'result':'skipped'}}
        with patch.object(aggregate,'ROOT',repo),patch.object(sys,'argv',['aggregate','--input',str(inputs),'--out',str(output)]), \
                patch.dict(os.environ,{'NEEDS_JSON':json.dumps(needs),'GITHUB_SHA':'synthetic-selection-only'}), \
                patch.object(aggregate,'validate_archive',return_value={'complete_runtime':True}),synthetic_runtime():
            self.assertEqual(aggregate.main(),int(missing))
        return output,json.loads((output/'release-manifest.json').read_text())

    def test_prior_aggregate_excluded_and_only_one_installable_zip_per_variant(self):
        output,manifest=self.run_case()
        self.assertTrue(manifest['complete']);self.assertEqual(set(manifest['mod_packages']),{'game'})
        self.assertEqual([path.name for path in output.glob('*.zip')],[f'TCE2-{version()}-Game.zip'])
        self.assertEqual(len(manifest['collected_targets']),12)
        self.assertFalse((output/'bundle/archives').exists())
        self.assertFalse((output/'bundle/inputs').exists())
        with zipfile.ZipFile(output/f'TCE2-{version()}-Game.zip') as z:
            self.assertEqual(len([n for n in z.namelist() if n.endswith('.pk3')]),1)
            self.assertEqual(len([n for n in z.namelist() if Path(n).name.startswith(('qagame','libqagame'))]),12)

    def test_missing_abi_never_creates_partial_installable_package(self):
        output,manifest=self.run_case(missing=True)
        self.assertFalse(manifest['complete']);self.assertFalse(manifest['client_inputs_promoted'])
        self.assertEqual(manifest['mod_packages'],{});self.assertEqual(list(output.glob('*.zip')),[])
        self.assertEqual(len(list((output/'bundle/inputs').glob('*.zip'))),11)

    def test_editor_is_a_different_optional_zip_not_an_addon(self):
        output,manifest=self.run_case(('game','navstudio'))
        self.assertTrue(manifest['complete']);self.assertEqual(set(manifest['mod_packages']),{'game','navstudio'})
        self.assertEqual(len(list(output.glob('*.zip'))),2)
        self.assertNotEqual(manifest['mod_packages']['game']['pk3_sha256'],manifest['mod_packages']['navstudio']['pk3_sha256'])
        for variant,label in [('game','Game'),('navstudio','Editor')]:
            with zipfile.ZipFile(output/f'TCE2-{version()}-{label}.zip') as z:
                self.assertEqual(len([n for n in z.namelist() if n.endswith('.pk3')]),1)


if __name__=='__main__':unittest.main()
