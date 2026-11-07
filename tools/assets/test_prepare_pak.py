"""Canonical-resource validation never re-imports or rewrites asset trees."""
from contextlib import redirect_stderr, redirect_stdout
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import prepare_pak as p
from editor_ui import REGISTRIES, VOICE_FILES, validate_editor_ui


def sha(data): return hashlib.sha256(data).hexdigest()


class PreparePakTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(); self.addCleanup(self.directory.cleanup)
        self.repo = Path(self.directory.name)
        self.write('src/game/tce_version.h', b'#define TCE_VERSION "1.0.3"\n')
        stock = {'schema':1, 'release_version':'1.0.3', 'baseline_id':'tce-0.49b-pak0-4',
                 'files':{'scripts/base.shader':{'sha256':sha(b'original shader'), 'bytes':15}},
                 'source_packages':[{'name':f'pak{i}.pk3', 'sha256':sha(str(i).encode()), 'bytes':1} for i in range(5)]}
        self.write('tools/assets/base-game-reference.json', json.dumps(stock).encode())
        self.write('tools/assets/base-ui-reference.json', json.dumps({'schema':1, 'release_version':'1.0.3',
                   'files':{'ui/base_shared.menu':{'sha256':sha(b'stock menu'), 'bytes':10,
                             'menus':[{'name':'base_shared', 'opens':[]}]}}}).encode())
        self.write('pak/default.cfg', b'// custom bindings\r\nbind F10 "toggleconsole"\r\n')
        self.write('pak/ui/main.menu', b'menuDef { name "main" }')
        self.write('pak/scripts/base.shader', b'custom shader')
        self.write('pak/fonts/portable.dat', b'font bytes')
        self.write('pak/tce2_ui/runtime.cfg', b'TCE2_MODERN_UI 1\n')
        self.write('pak/tce2_ui/package.json', b'{"canonical":"hand edited manifest"}\n')
        for mapname in ('delta','northport','railhouse','snow','stadtrand','village'):
            for suffix in ('_cc.tga','_cc_blueprint.tga'):
                self.write('pak/levelshots/obj_'+mapname+suffix, (mapname+suffix).encode())
        for name in REGISTRIES:
            self.write('pak/ui/'+name, b'{ loadMenu { "ui/main.menu" } }')
        goals = ('tce_ob_goalname','tce_ob_groupname','tce_ob_waypointname','tce_ob_value')
        files = {'ui/tce_omnibot.menu':b'menuDef { name "tce_omnibot" itemDef { action { open tce_omnibot_dms ; } } }',
                 'ui/tce_omnibot_dms.menu':('menuDef { name "tce_omnibot_dms" itemDef { action { '+
                     ' '.join('open '+goal+' ;' for goal in goals)+' open tce_ob_0_0 ; } } }').encode(),
                 'ui/tce_ob_0_0.menu':b'menuDef { name "tce_ob_0_0" action { open tce_omnibot_dms ; } }'}
        files.update({'ui/'+goal+'.menu':('menuDef { name "'+goal+'" }').encode() for goal in goals})
        files.update({'ui/'+voice:('menuDef { name "'+voice[:-5]+'" itemDef { action { exec "omnibot_menu" ; } } }').encode() for voice in VOICE_FILES})
        registry=b'{ loadMenu { "ui/main.menu" } '+b' '.join(('loadMenu { "'+name+'" }').encode() for name in files)+b' }'
        files.update({'ui/'+name:registry for name in REGISTRIES})
        for name,data in files.items(): self.write('navstudio/editor-pak/'+name,data)

    def write(self, relative, data):
        path=self.repo/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data); return path

    def snapshot(self):
        return {file.relative_to(self.repo).as_posix():(file.read_bytes(),file.stat().st_mtime_ns)
                for file in self.repo.rglob('*') if file.is_file()}

    def prepare(self, **kwargs): return p.prepare(self.repo, **kwargs)

    def invalid_unchanged(self, pattern):
        before=self.snapshot()
        with self.assertRaisesRegex(ValueError,pattern): self.prepare(write_provenance=True)
        self.assertEqual(self.snapshot(),before)

    def test_clean_checkout_uses_only_canonical_trees_and_small_stock_metadata(self):
        before=self.snapshot(); result=self.prepare()
        self.assertTrue(result['passed']); self.assertTrue(result['check_only'])
        self.assertEqual(self.snapshot(),before)
        for name in ('pak_original_tce','src/assets','ui-modern','navstudio/editor-ui'):
            self.assertFalse((self.repo/name).exists())
        game,editor,report=p.plan(self.repo)
        self.assertEqual(len([name for name in game if name.startswith('levelshots/')]),12)
        self.assertFalse(report['resource_generation']); self.assertEqual(report['schema'],3)
        self.assertEqual(report['game_files']['ui/main.menu']['source'],'pak/ui/main.menu')
        self.assertEqual(editor['ui/tce_omnibot_dms.menu'].data(),(self.repo/'navstudio/editor-pak/ui/tce_omnibot_dms.menu').read_bytes())

    def test_default_and_check_do_not_write_or_compare_against_old_provenance(self):
        self.write('tools/assets/pak-sources.json', b'old historical provenance')
        self.write('pak/ui/main.menu', b'menuDef { name "main" text "Canonical hand edit" }')
        before=self.snapshot()
        self.assertTrue(self.prepare()['passed']); self.assertTrue(self.prepare(check=True)['passed'])
        self.assertEqual(self.snapshot(),before)

    def test_old_original_theme_source_and_native_catalog_are_never_fallbacks(self):
        for folder in ('pak_original_tce','src/assets','ui-modern/assets','navstudio/editor-ui'):
            self.write(folder+'/ui/main.menu', b'open tce_omnibot ; broken fallback')
            self.write(folder+'/default.cfg', b'unbindall\n')
        before=self.snapshot(); self.assertTrue(self.prepare()['passed']); self.assertEqual(self.snapshot(),before)
        (self.repo/'navstudio/editor-pak/ui/tce_ob_goalname.menu').unlink()
        self.write('navstudio/editor-ui/tce_ob_goalname.menu',b'menuDef { name "tce_ob_goalname" }')
        self.invalid_unchanged('Missing registered Editor menu')

    def test_explicit_provenance_changes_only_metadata_and_is_idempotent(self):
        before=self.snapshot(); result=self.prepare(write_provenance=True)
        self.assertTrue(result['provenance_updated']); self.assertTrue(result['runtime_files_unchanged'])
        after=self.snapshot(); key='tools/assets/pak-sources.json'
        self.assertEqual({name:row for name,row in after.items() if name!=key},before)
        metadata=json.loads(after[key][0]);self.assertEqual(metadata['release_version'],'1.0.3')
        self.assertEqual(metadata['stock_reference']['sha256'],sha((self.repo/'tools/assets/base-game-reference.json').read_bytes()))
        self.assertNotIn(str(self.repo),json.dumps(metadata))
        before=self.snapshot();self.assertFalse(self.prepare(write_provenance=True)['provenance_updated']);self.assertEqual(self.snapshot(),before)

    def test_final_editor_bytes_are_not_namespaced_or_merged_again(self):
        root='navstudio/editor-pak/ui/tce_omnibot.menu'
        self.write(root,(self.repo/root).read_bytes()+b'\n// hand edited final root\n')
        before=self.snapshot(); game,editor,_=p.plan(self.repo)
        self.assertNotIn('ui/tce_omnibot.menu',game)
        self.assertEqual(editor['ui/tce_omnibot.menu'].data(),before[root][0])
        self.assertEqual(self.snapshot(),before)

    def test_obsolete_replace_and_conflicting_check_cannot_mutate_any_file(self):
        for kwargs in ({'replace':True},{'check':True,'write_provenance':True}):
            before=self.snapshot()
            with self.assertRaisesRegex(ValueError,'removed|cannot write'): self.prepare(**kwargs)
            self.assertEqual(self.snapshot(),before)

    def test_cli_default_check_provenance_and_replace(self):
        for args in ([],['--check']):
            before=self.snapshot(); out=io.StringIO()
            with redirect_stdout(out): self.assertEqual(p.main(['--repo',str(self.repo),*args]),0)
            self.assertTrue(json.loads(out.getvalue())['passed']); self.assertEqual(self.snapshot(),before)
        with redirect_stdout(io.StringIO()):self.assertEqual(p.main(['--repo',str(self.repo),'--write-provenance']),0)
        before=self.snapshot()
        with self.assertRaises(SystemExit) as error, redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()): p.main(['--repo',str(self.repo),'--replace'])
        self.assertEqual(error.exception.code,1);self.assertEqual(self.snapshot(),before)

    def test_stock_identical_bytes_are_rejected_not_silently_deleted(self):
        self.write('pak/scripts/base.shader',b'original shader')
        self.invalid_unchanged('Unchanged original TCE resource')

    def test_stock_matching_uses_case_insensitive_engine_paths(self):
        path=self.repo/'tools/assets/base-game-reference.json';data=json.loads(path.read_bytes())
        data['files']['SCRIPTS/BASE.SHADER']=data['files'].pop('scripts/base.shader');path.write_text(json.dumps(data))
        self.write('pak/scripts/base.shader',b'original shader');self.invalid_unchanged('Unchanged original')

    def test_missing_or_wrong_version_stock_metadata_fails(self):
        for key in ('base-game-reference.json','base-ui-reference.json'):
            path=self.repo/'tools/assets'/key; original=path.read_bytes();data=json.loads(original);data['release_version']='1.0.2';path.write_text(json.dumps(data))
            self.invalid_unchanged('version-bound');path.unlink();self.invalid_unchanged('Missing');path.write_bytes(original)

    def test_missing_or_empty_canonical_sources_fail_without_fallback(self):
        path=self.repo/'pak';renamed=self.repo/'saved-pak';path.rename(renamed)
        self.invalid_unchanged('Missing mod delta resource directory');renamed.rename(path)
        path=self.repo/'navstudio/editor-pak';renamed=self.repo/'saved-editor';path.rename(renamed)
        self.invalid_unchanged('Missing mod delta resource directory')

    def test_native_archives_generated_and_os_junk_are_rejected(self):
        for name in ('old.pk3','old.ZIP','old.7z','ui_mp_x86.dll','qagame.mp.x86_64.so','old.so.1','tool.exe','.DS_Store','tce2/1.0.3/client.manifest'):
            for folder in ('pak','navstudio/editor-pak'):
                file=self.write(folder+'/'+name,b'not a resource')
                self.invalid_unchanged('Native|native|Unsafe|generated|archive');file.unlink()

    def test_public_authoring_filename_and_actions_cannot_leak(self):
        for name,data in (('ui/tce_omnibot.menu',b'editor'),('ui/other.menu',b'open tce_omnibot'),
                          ('ui/other.menu',b'exec "bot waypoint_on"'),('ui/other.menu',b'exec "bot goal_save"')):
            file=self.write('pak/'+name,data);self.invalid_unchanged('Editor|Authoring');file.unlink()

    def test_default_config_reset_is_reported_without_rewriting_bindings(self):
        self.write('pak/default.cfg',b'bind F10 toggleconsole\nunbindall; bind n noclip\n')
        self.invalid_unchanged('unbindall')

    def test_unsafe_vfs_paths_and_case_colliding_editor_overlay_fail(self):
        for name in ('../escape','/absolute','.','a\\b','a:b','bad\nname','a//b'):
            with self.subTest(name=name),self.assertRaises(ValueError):p.safe_name(name)
        self.write('navstudio/editor-pak/UI/MAIN.menu',b'case collision')
        self.invalid_unchanged('Case-colliding')

    def test_native_goal_file_feature_duplicate_and_unreachable_root_fail(self):
        goal=self.repo/'navstudio/editor-pak/ui/tce_ob_goalname.menu';original=goal.read_bytes();goal.unlink()
        self.invalid_unchanged('Missing registered');goal.write_bytes(original)
        goal.write_bytes(original.replace(b'tce_ob_goalname',b'tce_ob_groupname'));self.invalid_unchanged('Duplicate Editor');goal.write_bytes(original)
        root=self.repo/'navstudio/editor-pak/ui/tce_omnibot.menu';original=root.read_bytes()
        root.write_bytes(original.replace(b'open tce_omnibot_dms',b'close tce_omnibot_dms'));self.invalid_unchanged('Unreachable Editor');root.write_bytes(original)
        voice=self.repo/'navstudio/editor-pak/ui'/VOICE_FILES[0]
        voice.write_bytes(b'menuDef { name "voice" }');self.invalid_unchanged('voice entry')

    def test_all_four_editor_registries_must_load_goals(self):
        for name in REGISTRIES:
            path=self.repo/'navstudio/editor-pak/ui'/name;original=path.read_bytes()
            path.write_bytes(original.replace(b'loadMenu { "ui/tce_ob_goalname.menu" }',b''))
            self.invalid_unchanged('goal/root feature');path.write_bytes(original)

    def test_stock_ui_is_metadata_only_and_never_authorizes_native_editor(self):
        for name in REGISTRIES:
            path=self.repo/'navstudio/editor-pak/ui'/name
            path.write_bytes(path.read_bytes().replace(b'loadMenu { "ui/main.menu" }',b'loadMenu { "ui/main.menu" } loadMenu { "ui/base_shared.menu" }'))
        _game,_editor,report=p.plan(self.repo)
        self.assertIn('ui/base_shared.menu',report['editor_ui_closure']['registries']['menus.txt']['stock_metadata_loaders'])
        self.assertFalse((self.repo/'pak/ui/base_shared.menu').exists())
        path=self.repo/'tools/assets/base-ui-reference.json';data=json.loads(path.read_bytes())
        data['files']['ui/base_shared.menu']['menus'][0]['name']='tce_ob_goalname';path.write_text(json.dumps(data))
        self.invalid_unchanged('cannot fall back')

    def test_input_changes_during_validation_prevent_provenance_write(self):
        actual=p.validate_public
        def changing(files):
            actual(files); self.write('pak/scripts/new.shader',b'changed while validating')
        with mock.patch.object(p,'validate_public',side_effect=changing):
            with self.assertRaisesRegex(ValueError,'changed during validation'):self.prepare(write_provenance=True)
        self.assertFalse((self.repo/'tools/assets/pak-sources.json').exists())

    def test_symlinked_canonical_root_is_not_followed(self):
        source=self.repo/'pak';saved=self.repo/'saved-pak';source.rename(saved)
        try: source.symlink_to(saved,target_is_directory=True)
        except OSError as error:
            saved.rename(source); self.skipTest('Host does not allow directory symlinks: '+str(error))
        self.invalid_unchanged('cannot be a link')


if __name__ == '__main__': unittest.main()
