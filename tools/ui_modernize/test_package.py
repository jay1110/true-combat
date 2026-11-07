"""Canonical UI resources, exact archives and public/editor boundaries."""
import json
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
import zipfile

import verify_package as v

MENU=b'''menuDef { name "main" rect 1 2 100 80
 itemDef { name "choice" visible 1 text "Choice" cvar "ui_choice"
 cvarFloatList { "Off" 0 "On" 1 } feeder 4 ownerdraw 17 action { exec "stats" ; } }
}
'''


class Packages(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name);self.pak=self.repo/'pak';self.overlay=self.repo/'navstudio/editor-pak'
        self.overlay.mkdir(parents=True);self.package=self.repo/'result.pk3'
        self.put('ui/main.menu',MENU)
        for name in v.LOAD_LISTS:self.put('ui/'+name,b'loadMenu { "ui/main.menu" }\n')
        self.put('tce2_ui/runtime.cfg',v.RUNTIME_MARKER[1])
        self.put('tce2_ui/button.tga',b'owned pixels')
        self.put('ui/assets/check.tga',b'owned pixels')
        self.put('fonts/tce2_ui/test.ttf',b'\x00\x01\x00\x00owned font')
        self.put('fonts/tce2_ui/LICENSE.txt',b'font license')
        self.put('scripts/tce2_backgrounds.shader',b'tce2_ui/backgrounds/b01 { { map tce2_ui/backgrounds/b01.tga } }')
        self.put('tce2_ui/backgrounds/b01.tga',b'owned background')
        self.put('tce2_ui/SOURCE.json',json.dumps({'files':[self.row('tce2_ui/button.tga',b'owned pixels')],
            'native_aliases':{'ui/assets/check.tga':'tce2_ui/button.tga'}}).encode())
        self.put('fonts/tce2_ui/SOURCE.json',json.dumps({'files':[self.row('fonts/tce2_ui/test.ttf',b'\x00\x01\x00\x00owned font')]}).encode())
        self.put('tce2_ui/package.json',b'{"schema":1,"variant":"game"}')
        self.put('ui/tce_omnibot.menu',MENU.replace(b'"main"',b'"tce_omnibot"'),editor=True)
        self.put('ui/menus.txt',b'loadMenu { "ui/main.menu" }\nloadMenu { "ui/tce_omnibot.menu" }\n',editor=True)
        self.put('tce2_ui/package.json',b'{"schema":1,"variant":"editor"}',editor=True)

    def put(self,name,data,editor=False):
        path=(self.overlay if editor else self.pak)/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)

    @staticmethod
    def row(name,data):return {'path':name,'bytes':len(data),'sha256':v.sha(data)}

    def payload(self,variant='game'):return v.canonical_ui.collect(variant,self.repo)

    def write(self,files=None,variant='game',extra=()):
        files=self.payload(variant) if files is None else files
        with zipfile.ZipFile(self.package,'w',zipfile.ZIP_DEFLATED) as z:
            for name,data in files.items():z.writestr(name,data)
            for name,data in extra:
                if isinstance(name,str) and '\\' in name:
                    value=name;name=zipfile.ZipInfo(value);name.filename=value;name.orig_filename=value
                z.writestr(name,data)

    def check(self,variant='game'):return v.verify(self.package,variant,repo=self.repo)

    def test_both_variants_use_only_canonical_bytes_and_final_editor_overlay(self):
        for variant in ('game','editor'):
            self.write(variant=variant);result=self.check(variant)
            self.assertTrue(result['passed']);self.assertEqual(v.EDITOR_MENU in result['payload_sha256'],variant=='editor')
            self.assertIn('scripts/tce2_backgrounds.shader',result['payload_sha256'])
            self.assertIn('tce2_ui/backgrounds/b01.tga',result['payload_sha256'])
        self.assertEqual(self.payload('editor')['ui/tce_omnibot.menu'],(self.overlay/'ui/tce_omnibot.menu').read_bytes())

    def test_missing_or_changed_canonical_file_is_rejected(self):
        for kind in ('missing','menu','asset','alias','font','extra','manifest','marker'):
            files=self.payload()
            if kind=='missing':del files['ui/main.menu']
            elif kind=='extra':files['unexpected/data.txt']=b'unclassified'
            else:
                name={'menu':'ui/main.menu','asset':'tce2_ui/button.tga','alias':'ui/assets/check.tga','font':'fonts/tce2_ui/test.ttf','manifest':'tce2_ui/package.json','marker':'tce2_ui/runtime.cfg'}[kind]
                files[name]+=b'changed'
            self.write(files)
            with self.subTest(kind=kind),self.assertRaisesRegex(ValueError,'Stale/unclassified'):self.check()

    def test_honest_archive_fingerprints_cannot_authorize_changed_commands(self):
        files=self.payload();files['ui/main.menu']=MENU.replace(b'exec "stats"',b'exec "quit"')
        files['tce2_ui/package.json']=json.dumps({'files':{n:self.row(n,d) for n,d in files.items()}}).encode();self.write(files)
        with self.assertRaisesRegex(ValueError,'Stale/unclassified'):self.check()

    def test_fresh_canonical_edits_are_required_without_any_original_fallback(self):
        self.write();self.put('ui/main.menu',MENU.replace(b'exec "stats"',b'exec "wstats"'))
        with self.assertRaisesRegex(ValueError,'Stale/unclassified'):self.check()
        self.write();self.assertTrue(self.check()['passed'])
        old=self.repo/'pak_original_tce';shutil.copytree(self.pak,old);shutil.rmtree(self.pak)
        with self.assertRaisesRegex(ValueError,'canonical UI tree'):self.check()

    def test_stale_asset_provenance_and_alias_cannot_be_rehashed_into_archive(self):
        for name in ('tce2_ui/button.tga','ui/assets/check.tga'):
            previous=(self.pak/name).read_bytes();self.put(name,b'changed');self.write()
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'provenance|widget alias'):self.check()
            self.put(name,previous)

    def test_game_authoring_file_loader_or_action_rejected(self):
        for kind in ('file','load','action'):
            if kind=='file':name='ui/tce_omnibot.menu';data=MENU
            elif kind=='load':name='ui/menus.txt';data=b'loadMenu { "ui/tce_omnibot.menu" }'
            else:name='ui/main.menu';data=MENU.replace(b'exec "stats"',b'exec "wp_add"')
            previous=(self.pak/name).read_bytes() if (self.pak/name).exists() else None;self.put(name,data)
            with self.subTest(kind=kind),self.assertRaisesRegex(ValueError,'Editor|authoring'):
                self.write();self.check()
            if previous is None:(self.pak/name).unlink()
            else:self.put(name,previous)

    def test_game_and_editor_archives_are_not_interchangeable(self):
        self.write(variant='editor')
        with self.assertRaisesRegex(ValueError,'Stale/unclassified'):self.check('game')
        self.write(variant='game')
        with self.assertRaisesRegex(ValueError,'Stale/unclassified'):self.check('editor')

    def test_original_semantics_still_protect_actions_cvars_lists_labels_and_constants(self):
        for old,new in ((b'exec "stats"',b'exec "quit"'),(b'"ui_choice"',b'"ui_other"'),(b'text "Choice"',b'text "Other"'),(b'"On" 1',b'"On" 2'),(b'feeder 4',b'feeder 9'),(b'ownerdraw 17',b'ownerdraw 18')):
            with self.subTest(old=old),self.assertRaisesRegex(ValueError,'semantic data changed'):
                v.compare_semantics({'ui/main.menu':MENU},{'ui/main.menu':MENU.replace(old,new)})

    def test_unsafe_paths_binary_headers_archives_and_case_collisions(self):
        for name,data in (('../escape.txt',b'x'),('/absolute',b'x'),('ui\\bad',b'x'),('ui//bad',b'x'),('C:/bad',b'x'),('UI/main.menu',b'x'),('hidden.dll',b'x'),('nested.pk3',b'x'),('art.data',b'MZbinary'),('font.data',b'\x7fELFbinary'),('code.data',b'\0asmbinary')):
            self.write(extra=[(name,data)])
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'Unsafe/duplicate|Executable/gameplay'):self.check()
        link=zipfile.ZipInfo('link');link.create_system=3;link.external_attr=(stat.S_IFLNK|0o777)<<16
        self.write(extra=[(link,b'target')])
        with self.assertRaisesRegex(ValueError,'Unsafe/duplicate'):self.check()

    def test_no_implicit_resource_generation_or_output(self):
        before={str(p.relative_to(self.repo)):v.sha(p.read_bytes()) for p in self.repo.rglob('*') if p.is_file()}
        for variant in ('game','editor'):self.payload(variant)
        after={str(p.relative_to(self.repo)):v.sha(p.read_bytes()) for p in self.repo.rglob('*') if p.is_file()}
        self.assertEqual(before,after)

    def test_cli_validates_without_originals_and_protects_runtime_sources(self):
        self.write();receipt=self.repo/'receipt.json'
        args=[sys.executable,str(Path(v.__file__)), '--package',str(self.package),'--variant','game','--repo',str(self.repo)]
        result=subprocess.run(args+['--out',str(receipt)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertTrue(json.loads(receipt.read_text())['passed'])
        for target in (self.pak/'ui/main.menu',self.overlay/'ui/menus.txt',self.package):
            before=target.read_bytes()
            result=subprocess.run(args+['--out',str(target)],capture_output=True,text=True)
            self.assertEqual(result.returncode,2,result.stdout+result.stderr)
            self.assertEqual(target.read_bytes(),before)

if __name__=='__main__':unittest.main()
