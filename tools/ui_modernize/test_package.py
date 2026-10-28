"""Independent synthetic PK3s exercise closure, content exceptions and attacks."""
import json
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
import zipfile

import semantic as sem
import verify_package as v

ROOT = Path(__file__).resolve().parents[2]
MENU = b'''menuDef { name "main" rect 1 2 100 80 backcolor 0 0 0 .8
 onOpen { setcvar "ui_mode" "safe" ; } onESC { close main ; }
 itemDef { name "choice" rect 3 4 90 12 visible 1 text "Choice" cvar "ui_choice"
 cvarFloatList { "Off" 0 "On" 1 } feeder 4 ownerdraw 17 action { exec "stats" ; } }
}
'''


class Packages(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.original = self.repo/'pak'
        self.edited = self.repo/'ui-modern'
        for directory in sem.DIRECTORIES:
            folder = self.original/directory
            folder.mkdir(parents=True)
            (folder/'main.menu').write_bytes(MENU)
        for name in v.OVERRIDES:
            (self.original/'ui'/name).write_bytes(MENU)
        (self.original/v.EDITOR_MENU).write_bytes(MENU.replace(b'"main"', b'"tce_omnibot"'))
        (self.original/'ui/menudef.h').write_bytes(b'#define ITEM_TYPE_BIND 4\n#define UI_FONT_ARIBLK_16 0\n')
        for name in v.LOAD_LISTS:
            (self.original/'ui'/name).write_bytes(b'loadMenu { "ui/main.menu" }\nloadMenu { "ui/tce_omnibot.menu" }\n')
        shutil.copytree(self.original, self.edited)
        for path in self.edited.rglob('*.menu'):
            path.write_bytes(path.read_bytes().replace(b'rect 1 2 100 80', b'rect 8 9 200 100'))
        production = self.repo/'src/assets/ui'
        production.mkdir(parents=True)
        for index, name in enumerate(v.OVERRIDES):
            (production/name).write_bytes(MENU.replace(b'"ui_choice"', ('"existing_game_'+str(index)+'"').encode()))
        assets = self.edited/'assets'
        (assets/'tce2_ui').mkdir(parents=True)
        (assets/'fonts/tce2_ui').mkdir(parents=True)
        pixels = b'original test pixels'
        font = b'\x00\x01\x00\x00original test font'
        (assets/'tce2_ui/button.tga').write_bytes(pixels)
        (assets/'fonts/tce2_ui/test.ttf').write_bytes(font)
        (assets/'fonts/tce2_ui/LICENSE.txt').write_bytes(b'test license')
        (assets/'tce2_ui/SOURCE.json').write_text(json.dumps({
            'files':[self.row('tce2_ui/button.tga',pixels)],
            'native_aliases':{'ui/assets/check.tga':'tce2_ui/button.tga'}}))
        (assets/'fonts/tce2_ui/SOURCE.json').write_text(json.dumps({
            'files':[self.row('fonts/tce2_ui/test.ttf',font)]}))
        self.baseline = sem.inventory(self.original)
        self.package = self.repo/'result.pk3'

    @staticmethod
    def row(path,data):
        return {'path':path,'bytes':len(data),'sha256':v.sha(data)}

    def payload(self, variant='game'):
        files = {p:d for p,d in sem.source_files(self.edited).items() if p in v.text_paths(sem.source_files(self.original))}
        exceptions = []
        if variant == 'game':
            del files[v.EDITOR_MENU]
            for name in v.LOAD_LISTS:
                key = 'ui/'+name
                files[key] = files[key].replace(b'loadMenu { "ui/tce_omnibot.menu" }\n',b'')
            for name in v.OVERRIDES:
                key, source = 'ui/'+name, 'src/assets/ui/'+name
                files[key] = (self.repo/source).read_bytes()
                exceptions.append({'path':key,'content_source':source,'reason':'Preserve real current Game controls.'})
        assets = self.edited/'assets'
        files.update({p.relative_to(assets).as_posix():p.read_bytes() for p in assets.rglob('*') if p.is_file()})
        files['ui/assets/check.tga'] = files['tce2_ui/button.tga']
        manifest = {'schema':1,'theme':'TCE2 Modern UI','version':'1.0.2','variant':variant,
                    'all_original_menu_sources':self.baseline['menu_count'],
                    'normal_game_content_sources':exceptions}
        return files, manifest

    def write(self, files, manifest, extra_entries=()):
        # Recompute honest fingerprints: negative tests must not merely fail CRC/hash.
        manifest = dict(manifest)
        manifest['files'] = {p:{'bytes':len(d),'sha256':v.sha(d)} for p,d in files.items()}
        with zipfile.ZipFile(self.package,'w',zipfile.ZIP_DEFLATED) as archive:
            for path,data in files.items():
                archive.writestr(path,data)
            archive.writestr('tce2_ui/package.json',json.dumps(manifest))
            for info,data in extra_entries:
                if isinstance(info,str) and '\\' in info:
                    # Windows ZipInfo's constructor canonicalizes backslashes;
                    # preserve the hostile stored name to exercise the reader.
                    name=info;info=zipfile.ZipInfo(name)
                    info.filename=name;info.orig_filename=name
                archive.writestr(info,data)

    def check(self, variant='game', transform=lambda data,path:data):
        return v.verify(self.package,variant,self.baseline,self.repo,transform)

    def test_both_variants_full_sources_and_explicit_game_content(self):
        for variant in ('game','editor'):
            with self.subTest(variant=variant):
                files,manifest = self.payload(variant); self.write(files,manifest)
                result = self.check(variant)
                self.assertTrue(result['passed'])
                self.assertEqual(result['menus'],self.baseline['menu_count']-(variant=='game'))
                self.assertEqual(len(result['game_content_sources']),3 if variant=='game' else 0)
                if variant=='editor':
                    self.assertIn(v.EDITOR_MENU,result['payload_sha256'])

    def test_game_exception_inventory_paths_reasons_and_variant_enforced(self):
        for change in ('missing','duplicate','wrong_path','wrong_source','empty_reason','editor_claim'):
            with self.subTest(change=change):
                variant = 'editor' if change=='editor_claim' else 'game'
                files,m = self.payload(variant)
                if change=='missing':m['normal_game_content_sources'].pop()
                elif change=='duplicate':m['normal_game_content_sources'][1]=m['normal_game_content_sources'][0]
                elif change=='wrong_path':m['normal_game_content_sources'][0]['path']='ui/main.menu'
                elif change=='wrong_source':m['normal_game_content_sources'][0]['content_source']='pak/ui/ingame_main.menu'
                elif change=='empty_reason':m['normal_game_content_sources'][0]['reason']=''
                else:m['normal_game_content_sources']=[{'path':'ui/main.menu'}]
                self.write(files,m)
                with self.assertRaisesRegex(ValueError,'exception|Editor cannot'):
                    self.check(variant)

    def test_fresh_game_production_bytes_required(self):
        files,m = self.payload(); self.write(files,m)
        source = self.repo/'src/assets/ui/options_controls.menu'
        source.write_bytes(source.read_bytes().replace(b'exec "stats"',b'exec "wstats"'))
        with self.assertRaisesRegex(ValueError,'Stale/unclassified'):
            self.check()

    def test_game_current_extensions_cannot_be_replaced_by_original_pak(self):
        files,m = self.payload()
        files['ui/options_controls.menu']=(self.original/'ui/options_controls.menu').read_bytes()
        self.write(files,m)
        with self.assertRaisesRegex(ValueError,'Stale/unclassified'):
            self.check()

    def test_independent_semantics_protect_actual_game_content_despite_transform(self):
        files,m = self.payload();self.write(files,m)
        def bad_transform(data,path):
            return data.replace(b'exec "stats"',b'exec "quit"')
        for name in v.OVERRIDES:
            files['ui/'+name] = bad_transform(files['ui/'+name],name)
        self.write(files,m)
        with self.assertRaisesRegex(ValueError,'actions/cvars/labels'):
            self.check(transform=bad_transform)

    def test_independent_projection_rejects_actions_cvars_labels_lists_and_constants(self):
        for old,new in ((b'exec "stats"',b'exec "quit"'),(b'"ui_choice"',b'"ui_other"'),
                        (b'text "Choice"',b'text "Different"'),(b'"On" 1',b'"On" 2'),
                        (b'feeder 4',b'feeder 9'),(b'ownerdraw 17',b'ownerdraw 18')):
            with self.subTest(old=old), self.assertRaisesRegex(ValueError,'semantic data changed'):
                v.compare_semantics({'ui/main.menu':MENU},{'ui/main.menu':MENU.replace(old,new)})
        with self.assertRaisesRegex(ValueError,'semantic data changed'):
            v.compare_semantics({'ui/menudef.h':b'#define ITEM_TYPE_BIND 4\n'},
                                {'ui/menudef.h':b'#define ITEM_TYPE_BIND 5\n'})

    def test_missing_unused_menu_and_extra_unclassified_payload_rejected(self):
        for kind in ('missing','extra'):
            files,m=self.payload('editor')
            if kind=='missing':del files['ui_tdm/main.menu']
            else:files['unexpected/data.txt']=b'not classified'
            self.write(files,m)
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError,'Stale/unclassified'):
                self.check('editor')

    def test_game_authoring_file_or_load_reference_rejected(self):
        for kind in ('file','load','unrelated_load'):
            files,m=self.payload()
            if kind=='file':files[v.EDITOR_MENU]=(self.edited/v.EDITOR_MENU).read_bytes()
            elif kind=='load':files['ui/menus_gt7.txt']+=b'loadMenu { "ui/tce_omnibot.menu" }\n'
            else:files['ui/menus_gt7.txt']+=b'loadMenu { "ui/unrelated.menu" }\n'
            self.write(files,m)
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError,'authoring|load-list|Stale/unclassified'):
                self.check()

    def test_game_authoring_action_in_current_production_source_rejected(self):
        source=self.repo/'src/assets/ui/ingame_main.menu'
        source.write_bytes(source.read_bytes().replace(b'exec "stats"',b'exec "wp_add"'))
        files,m=self.payload();self.write(files,m)
        with self.assertRaisesRegex(ValueError,'Editor authoring reference'):
            self.check()

    def test_missing_changed_asset_alias_and_unmanifested_payload_rejected(self):
        for kind in ('missing','changed','alias','unmanifested'):
            files,m=self.payload()
            if kind=='missing':del files['fonts/tce2_ui/test.ttf']
            elif kind=='changed':files['tce2_ui/button.tga']+=b'changed'
            elif kind=='alias':files['ui/assets/check.tga']=b'not source pixels'
            if kind=='unmanifested':
                self.write(files,m, [('extra.txt',b'not inventoried')])
            else:self.write(files,m)
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError,'Stale/unclassified|inventory differs'):
                self.check()

    def test_stale_source_asset_provenance_cannot_be_rehashed_into_package(self):
        asset=self.edited/'assets/tce2_ui/button.tga'
        asset.write_bytes(b'new pixels without new SOURCE receipt')
        files,m=self.payload();self.write(files,m)
        with self.assertRaisesRegex(ValueError,'stale UI asset provenance'):
            self.check()

    def test_wrong_identity_menu_count_or_manifest_fingerprint_rejected(self):
        for kind in ('variant','schema','version','count','fingerprint'):
            files,m=self.payload()
            if kind=='variant':m['variant']='editor'
            elif kind=='schema':m['schema']=2
            elif kind=='version':m['version']='unknown'
            elif kind=='count':m['all_original_menu_sources']+=1
            self.write(files,m)
            if kind=='fingerprint':
                archive=v.read_archive(self.package)
                metadata=json.loads(archive['tce2_ui/package.json'])
                metadata['files']['ui/main.menu']['sha256']='0'*64
                archive['tce2_ui/package.json']=json.dumps(metadata).encode()
                with zipfile.ZipFile(self.package,'w') as z:
                    for p,d in archive.items():z.writestr(p,d)
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError,'identity/coverage|fingerprint differs'):
                self.check()

    def test_paths_symlinks_case_collisions_binaries_and_nested_archives_rejected(self):
        for name,data in (('../escape.txt',b'x'),('/absolute',b'x'),('ui\\bad',b'x'),
                          ('ui//bad',b'x'),('C:/bad',b'x'),('UI/main.menu',b'x'),
                          ('hidden.dll',b'not a DLL'),('nested.pk3',b'not a ZIP'),
                          ('art.data',b'MZhidden executable'),('font.data',b'\x7fELFhidden executable'),
                          ('code.data',b'\0asmhidden executable')):
            files,m=self.payload();self.write(files,m,[(name,data)])
            with self.subTest(name=name), self.assertRaisesRegex(ValueError,'Unsafe/duplicate|Executable/gameplay'):
                self.check()
        link=zipfile.ZipInfo('link');link.create_system=3;link.external_attr=(stat.S_IFLNK|0o777)<<16
        files,m=self.payload();self.write(files,m,[(link,b'ui/main.menu')])
        with self.assertRaisesRegex(ValueError,'Unsafe/duplicate'):
            self.check()

    def test_original_change_and_archive_change_during_verification_fail_closed(self):
        files,m=self.payload();self.write(files,m)
        original=self.original/'ui/main.menu'
        original.write_bytes(original.read_bytes()+b'// original changed')
        with self.assertRaisesRegex(ValueError,'Original inventory/provenance changed'):
            self.check()
        original.write_bytes(MENU)
        def changing(data,path):
            self.package.write_bytes(self.package.read_bytes()+b'changed during verify')
            return data
        with self.assertRaisesRegex(ValueError,'Archive changed'):
            self.check(transform=changing)

    def test_cli_fail_receipt_and_no_overwrite_of_sources_or_inputs(self):
        baseline=self.repo/'baseline.json';baseline.write_text(json.dumps(self.baseline))
        output=self.repo/'receipt.json'
        command=[sys.executable,str(ROOT/'tools/ui_modernize/verify_package.py'),
                 '--package',str(self.package),'--variant','editor','--baseline',str(baseline),'--repo',str(self.repo)]
        files,m=self.payload('editor');self.write(files,m)
        result=subprocess.run(command+['--out',str(output)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        files['ui/main.menu']=files['ui/main.menu'].replace(b'exec "stats"',b'exec "quit"');self.write(files,m)
        result=subprocess.run(command+['--out',str(output)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1,result.stdout+result.stderr)
        self.assertFalse(json.loads(output.read_text())['passed'])
        for path in (self.package,baseline,self.original/'result.json',self.edited/'result.json'):
            before=path.read_bytes() if path.exists() else None
            result=subprocess.run(command+['--out',str(path)],capture_output=True,text=True)
            self.assertEqual(result.returncode,2,result.stdout+result.stderr)
            self.assertEqual(path.read_bytes() if path.exists() else None,before)


if __name__=='__main__':
    unittest.main()
