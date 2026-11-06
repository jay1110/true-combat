import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from prepare_pak import prepare, plan, safe_name
from editor_ui import REGISTRIES, VOICE_FILES, namespace_native, validate_editor_ui


class PreparePakTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.repo = Path(self.directory.name)
        self.write('pak_original_tce/default.cfg', b'// keep\r\nunbindall\r\nbind F10 "toggleconsole"\r\n')
        self.write('pak_original_tce/scripts/base.shader', b'original shader\x80')
        self.write('pak_original_tce/ui/main.menu', b'old menu')
        self.write('pak_original_tce/levelshots/obj_delta_cc.tga', b'old delta')
        self.write('src/game/tce_version.h', b'#define TCE_VERSION "1.0.3"\n')
        stock = {p.relative_to(self.repo/'pak_original_tce').as_posix():
                 {'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}
                 for p in (self.repo/'pak_original_tce').rglob('*') if p.is_file()}
        self.write('tools/assets/base-game-reference.json',json.dumps({'schema':1,'release_version':'1.0.3',
                   'baseline_id':'synthetic-stock','files':stock}).encode())
        self.write('src/assets/levelshots/obj_delta_cc.tga', b'new delta')
        self.write('src/assets/levelshots/obj_northport_cc_blueprint.tga', b'new blueprint')
        self.game = {'ui/main.menu': b'modern menu',
                     **{'ui/'+name:b'{ loadMenu { "ui/main.menu" } }' for name in REGISTRIES},
                     'fonts/portable.dat': b'font', 'tce2_ui/runtime.cfg': b'TCE2_MODERN_UI 1\n'}
        self.editor = dict(self.game, **{'ui/main.menu': b'open tce_omnibot',
                                       'ui/tce_omnibot.menu': b'menuDef { name "tce_omnibot" }'})
        for name in REGISTRIES:
            self.editor['ui/'+name] = b'{ loadMenu { "ui/main.menu" } loadMenu { "ui/tce_omnibot.menu" } }'
        native = {'tce_omnibot.menu':b'menuDef { name "tce_omnibot" action { open tce_ob_goalname ; open tce_ob_groupname ; open tce_ob_waypointname ; open tce_ob_value ; open tce_ob_0_0 ; } }',
                  'tce_ob_0_0.menu':b'menuDef { name "tce_ob_0_0" action { open tce_omnibot ; } }'}
        for goal in ('goalname','groupname','waypointname','value'):
            native['tce_ob_'+goal+'.menu'] = ('menuDef { name "tce_ob_'+goal+'" action { exec "omnibot_'+goal+'" ; open tce_omnibot ; } }').encode()
        for name in VOICE_FILES:
            native[name] = ('menuDef { name "'+name[:-5]+'" action { exec "omnibot_menu" ; } }').encode()
        registry = b'{ loadMenu { "ui/main.menu" } '+b' '.join(('loadMenu { "ui/'+name+'" }').encode() for name in sorted(native))+b' }'
        native.update({name:registry for name in REGISTRIES})
        self.native_names = sorted(native)
        for name,data in native.items(): self.write('navstudio/editor-ui/'+name,data)
        self.write('navstudio/editor-ui/editor-files.txt', ('\n'.join(self.native_names)+'\n').encode())

    def write(self, relative, data):
        path = self.repo / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def collect(self, variant):
        return self.game if variant == 'game' else self.editor

    def run_prepare(self, **kwargs):
        return prepare(self.repo, collector=self.collect, **kwargs)

    def snapshot(self):
        return {p.relative_to(self.repo).as_posix(): (p.read_bytes(), p.stat().st_mtime_ns)
                for p in self.repo.rglob('*') if p.is_file()}

    def test_source_precedence_all_compass_images_and_original_bytes(self):
        original = {p.name: p.read_bytes() for p in (self.repo/'pak_original_tce').rglob('*') if p.is_file()}
        result = self.run_prepare()
        self.assertTrue(result['passed'])
        self.assertFalse((self.repo/'pak/scripts/base.shader').exists())
        self.assertEqual((self.repo/'pak/ui/main.menu').read_bytes(), b'modern menu')
        self.assertEqual((self.repo/'pak/levelshots/obj_delta_cc.tga').read_bytes(), b'new delta')
        self.assertEqual((self.repo/'pak/levelshots/obj_northport_cc_blueprint.tga').read_bytes(), b'new blueprint')
        self.assertEqual(original, {p.name: p.read_bytes() for p in (self.repo/'pak_original_tce').rglob('*') if p.is_file()})
        self.assertNotIn(b'unbindall', (self.repo/'pak/default.cfg').read_bytes())
        self.assertEqual((self.repo/'pak/default.cfg').read_bytes(), b'// keep\r\nbind F10 "toggleconsole"\r\n')

    def test_copies_are_independently_editable_not_hard_links(self):
        self.write('src/assets/scripts/base.shader',b'project changed shader')
        self.run_prepare()
        source = self.repo/'pak_original_tce/scripts/base.shader'
        target = self.repo/'pak/scripts/base.shader'
        self.assertNotEqual(source.stat().st_ino, target.stat().st_ino)
        target.write_bytes(b'manual')
        self.assertEqual(source.read_bytes(), b'original shader\x80')

    def test_editor_overlay_contains_only_different_collect_resources(self):
        self.run_prepare()
        files = {p.relative_to(self.repo/'navstudio/editor-pak').as_posix(): p.read_bytes()
                 for p in (self.repo/'navstudio/editor-pak').rglob('*') if p.is_file()}
        self.assertEqual(files['ui/main.menu'],b'open tce_omnibot')
        self.assertIn(b'open tce_omnibot_dms',files['ui/tce_omnibot.menu'])
        self.assertIn(b'name "tce_omnibot_dms"',files['ui/tce_omnibot_dms.menu'])
        self.assertIn(b'open tce_omnibot_dms',files['ui/tce_ob_goalname.menu'])
        self.assertEqual({name for name in files if name.endswith('.txt')},{'ui/'+name for name in REGISTRIES})
        self.assertNotIn('fonts/portable.dat',files)
        self.assertFalse((self.repo/'pak/ui/tce_omnibot.menu').exists())

    def test_original_archives_binaries_editor_file_are_excluded(self):
        for name in ['old.pk3', 'stale.ZIP', 'ui_mp_x86.dll', 'qagame.mp.x86_64.so', 'old.so.1', 'tool.exe', 'ui/tce_omnibot.menu']:
            self.write('pak_original_tce/'+name, b'ignored')
        self.run_prepare()
        self.assertFalse((self.repo/'pak/old.pk3').exists())
        self.assertFalse((self.repo/'pak/ui/tce_omnibot.menu').exists())
        manifest = json.loads((self.repo/'tools/assets/pak-sources.json').read_text())
        self.assertEqual(len([row for row in manifest['excluded'] if row['reason']=='archive/native/editor-only']), 7)
        self.assertFalse((self.repo/'pak/pak-sources.json').exists())

    def test_known_os_metadata_is_excluded_with_provenance_and_original_preserved(self):
        names = ('ui_obj/assets/.DS_Store', 'textures/Thumbs.db', '__MACOSX/._image.tga',
                 'nested/__macosx/metadata', 'mixed/thumbs.DB')
        originals = {name: self.write('pak_original_tce/'+name, b'original OS metadata') for name in names}
        self.write('src/assets/textures/.DS_Store', b'overlay OS metadata')
        self.write('pak_original_tce/valid/.custom-resource', b'keep unknown hidden asset')
        self.write('pak_original_tce/valid/Thumbs.db.txt', b'keep ordinary filename')
        self.run_prepare()
        for name, path in originals.items():
            self.assertEqual(path.read_bytes(), b'original OS metadata')
            self.assertFalse((self.repo/'pak'/name).exists())
        self.assertFalse((self.repo/'pak/textures/.DS_Store').exists())
        self.assertEqual((self.repo/'pak/valid/.custom-resource').read_bytes(), b'keep unknown hidden asset')
        self.assertEqual((self.repo/'pak/valid/Thumbs.db.txt').read_bytes(), b'keep ordinary filename')
        metadata = json.loads((self.repo/'tools/assets/pak-sources.json').read_text())
        metadata_rows=[row for row in metadata['excluded'] if row['reason'].startswith('operating-system metadata')]
        self.assertEqual(len(metadata_rows), 6)

    def test_theme_metadata_is_rejected_before_writing_either_output(self):
        for variant in (self.game, self.editor):
            variant['textures/.DS_Store'] = b'theme metadata'
            with self.assertRaisesRegex(ValueError, 'operating-system metadata'):
                self.run_prepare()
            del variant['textures/.DS_Store']
        self.assertFalse((self.repo/'pak').exists())
        self.assertFalse((self.repo/'navstudio/editor-pak').exists())

    def test_check_is_read_only_before_and_after_generation(self):
        before = self.snapshot()
        self.assertFalse(self.run_prepare(check=True)['passed'])
        self.assertEqual(self.snapshot(), before)
        self.run_prepare()
        before = self.snapshot()
        self.assertTrue(self.run_prepare(check=True)['passed'])
        self.assertEqual(self.snapshot(), before)

    def test_check_reports_missing_changed_and_extra_paths(self):
        self.run_prepare()
        (self.repo/'pak/fonts/portable.dat').unlink()
        self.write('pak/ui/main.menu', b'manual')
        self.write('pak/extra.txt', b'hand-made')
        result = self.run_prepare(check=True)
        self.assertFalse(result['passed'])
        self.assertEqual(result['differences']['pak'], {'missing': ['fonts/portable.dat'],
                         'changed': ['ui/main.menu'], 'extra': ['extra.txt']})

    def test_normal_run_preserves_manual_changes_and_writes_neither_tree(self):
        self.run_prepare()
        self.write('pak/ui/main.menu', b'manual')
        self.editor['ui/new.menu'] = b'new editor menu'
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, 'preserve your edits'):
            self.run_prepare()
        self.assertEqual(self.snapshot(), before)

    def test_replace_is_explicit_and_removes_stale_files(self):
        self.run_prepare()
        self.write('pak/ui/main.menu', b'manual')
        self.write('pak/stale.txt', b'extra')
        self.assertTrue(self.run_prepare(replace=True)['passed'])
        self.assertFalse((self.repo/'pak/stale.txt').exists())
        self.assertEqual((self.repo/'pak/ui/main.menu').read_bytes(), b'modern menu')

    def test_identical_rerun_retains_asset_mtimes(self):
        self.run_prepare()
        before = {p: p.stat().st_mtime_ns for p in (self.repo/'pak').rglob('*') if p.is_file()}
        self.run_prepare()
        self.assertEqual(before, {p: p.stat().st_mtime_ns for p in before})

    def test_rejects_editor_collector_leak_before_any_write(self):
        self.game['ui/tce_omnibot.menu'] = b'editor'
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, 'editor resource'):
            self.run_prepare()
        self.assertEqual(self.snapshot(), before)

    def test_rejects_authoring_actions_in_final_game_menus(self):
        for text in [b'open tce_omnibot', b'exec "bot waypoint_on"', b'exec "bot goal_save"']:
            self.game['ui/main.menu'] = text
            with self.subTest(text=text), self.assertRaisesRegex(ValueError, 'Authoring'):
                self.run_prepare()
        self.assertFalse((self.repo/'pak').exists())

    def test_rejects_unsafe_or_native_collector_paths(self):
        for name in ['../escape', '/absolute', '.', 'bad\nname', 'bad\\name', 'bad:name', 'ui_mp_x86.dll']:
            self.game[name] = b'bad'
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.run_prepare()
            del self.game[name]
        self.assertFalse((self.repo/'pak').exists())

    def test_rejects_case_colliding_source_overlay(self):
        self.game['SCRIPTS/base.shader'] = b'case collision'
        with self.assertRaisesRegex(ValueError, 'case-colliding'):
            self.run_prepare()

    def test_requires_renamed_original_tree(self):
        other = self.repo/'missing'
        with self.assertRaisesRegex(ValueError, 'Missing read-only'):
            plan(other, self.collect)

    def test_unclassified_reset_does_not_write(self):
        self.write('pak_original_tce/default.cfg', b'unbindall; bind F10 toggleconsole\n')
        with self.assertRaisesRegex(ValueError, 'Unclassified unbindall'):
            self.run_prepare()
        self.assertFalse((self.repo/'pak').exists())

    def test_provenance_hashes_and_sources_are_relative(self):
        self.run_prepare()
        metadata = json.loads((self.repo/'tools/assets/pak-sources.json').read_text())
        self.assertEqual(metadata['unbindall_commands_removed'], 1)
        item = metadata['game_files']['levelshots/obj_northport_cc_blueprint.tga']
        self.assertEqual(item, {'bytes': 13, 'sha256': hashlib.sha256(b'new blueprint').hexdigest(),
                              'source': 'src/assets/levelshots/obj_northport_cc_blueprint.tga'})
        self.assertNotIn(str(self.repo), json.dumps(metadata))

    def test_catalog_rejects_unsafe_missing_and_duplicate_declared_files(self):
        manifest=self.repo/'navstudio/editor-ui/editor-files.txt';before=manifest.read_bytes()
        for name in ('../escape.menu','unclassified.exe','tce_missing.menu','tce_ob_goalname.menu'):
            manifest.write_bytes(before+name.encode()+b'\n')
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'Invalid or missing|duplicate'):
                self.run_prepare()
        manifest.write_bytes(before)

    def test_native_goal_menu_cannot_silently_disappear(self):
        manifest=self.repo/'navstudio/editor-ui/editor-files.txt'
        manifest.write_bytes(manifest.read_bytes().replace(b'tce_ob_goalname.menu\n',b''))
        with self.assertRaisesRegex(ValueError,'Missing Editor goal/root feature'):
            self.run_prepare()
        self.assertFalse((self.repo/'pak').exists())

    def test_native_registry_cannot_drop_a_declared_goal_loader(self):
        path=self.repo/'navstudio/editor-ui/menus_gt5.txt'
        path.write_bytes(path.read_bytes().replace(b'loadMenu { "ui/tce_ob_goalname.menu" }',b''))
        with self.assertRaisesRegex(ValueError,'omits declared menu'):
            self.run_prepare()

    def test_duplicate_menu_names_are_rejected_per_registry(self):
        path=self.repo/'navstudio/editor-ui/tce_ob_goalname.menu'
        path.write_bytes(path.read_bytes().replace(b'name "tce_ob_goalname"',b'name "tce_ob_groupname"'))
        with self.assertRaisesRegex(ValueError,'Duplicate Editor menu name'):
            self.run_prepare()

    def test_namespace_changes_only_target_tokens_not_strings_comments_or_commands(self):
        source=(b'// open tce_omnibot\nmenuDef { name "tce_omnibot" '
                b'itemDef { text "open tce_omnibot" action { exec "open tce_omnibot" ; open tce_omnibot ; close "tce_omnibot" ; } } }')
        result=namespace_native(source)
        self.assertIn(b'// open tce_omnibot',result)
        self.assertIn(b'text "open tce_omnibot"',result)
        self.assertIn(b'exec "open tce_omnibot"',result)
        self.assertIn(b'name "tce_omnibot_dms"',result)
        self.assertIn(b'; open tce_omnibot_dms ; close "tce_omnibot_dms"',result)

    def test_final_installable_gate_rejects_lost_goal_file_or_unreachable_full_catalog(self):
        game,editor,report=plan(self.repo,self.collect)
        files={name:resource.data() for name,resource in (game|editor).items()}
        self.assertTrue(validate_editor_ui(files)['passed'])
        missing=dict(files);missing.pop('ui/tce_ob_goalname.menu')
        with self.assertRaisesRegex(ValueError,'Missing registered Editor menu'):
            validate_editor_ui(missing)
        unreachable=dict(files)
        unreachable['ui/tce_omnibot.menu']=unreachable['ui/tce_omnibot.menu'].replace(b'open tce_omnibot_dms',b'close tce_omnibot_dms')
        with self.assertRaisesRegex(ValueError,'Unreachable Editor'):
            validate_editor_ui(unreachable)
        self.assertEqual(report['native_editor_catalog_files'],len(self.native_names))

    def test_unlisted_editor_files_are_not_added(self):
        self.write('navstudio/editor-ui/unlisted.exe',b'not an input')
        _game,editor,_report=plan(self.repo,self.collect)
        self.assertFalse(any('unlisted' in name for name in editor))

    def test_only_authenticated_stock_equality_removes_local_reference_resources(self):
        self.write('pak_original_tce/levelshots/user_new_cc.tga',b'user compass missing from genuine stock')
        self.write('pak_original_tce/scripts/base.shader',b'user changed the local reference shader')
        self.run_prepare()
        self.assertEqual((self.repo/'pak/levelshots/user_new_cc.tga').read_bytes(),b'user compass missing from genuine stock')
        self.assertEqual((self.repo/'pak/scripts/base.shader').read_bytes(),b'user changed the local reference shader')

    def test_equal_source_and_theme_overlays_are_also_omitted_from_delta(self):
        self.write('src/assets/scripts/base.shader',b'original shader\x80')
        self.game['scripts/base.shader']=b'original shader\x80'
        self.editor['scripts/base.shader']=b'original shader\x80'
        self.run_prepare()
        self.assertFalse((self.repo/'pak/scripts/base.shader').exists())
        self.assertFalse((self.repo/'navstudio/editor-pak/scripts/base.shader').exists())

    def test_stock_matching_uses_case_insensitive_vfs_names(self):
        path=self.repo/'tools/assets/base-game-reference.json';metadata=json.loads(path.read_bytes())
        metadata['files']['SCRIPTS/BASE.SHADER']=metadata['files'].pop('scripts/base.shader')
        path.write_text(json.dumps(metadata))
        self.run_prepare()
        self.assertFalse((self.repo/'pak/scripts/base.shader').exists())

    def test_stock_reference_is_required_and_version_bound(self):
        path=self.repo/'tools/assets/base-game-reference.json';metadata=json.loads(path.read_bytes())
        metadata['release_version']='1.0.2';path.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError,'version-bound stock'):
            self.run_prepare()
        path.unlink()
        with self.assertRaisesRegex(ValueError,'Missing authenticated stock'):
            self.run_prepare()

    def test_obsolete_serverlist_readme_is_omitted_only_by_exact_fingerprint(self):
        path=self.write('pak_original_tce/readme.txt',b'old serverlist fix readme')
        with mock.patch.dict('prepare_pak.OBSOLETE_RESOURCES',{'readme.txt':hashlib.sha256(path.read_bytes()).hexdigest()}):
            game,_editor,report=plan(self.repo,self.collect)
        self.assertNotIn('readme.txt',game)
        self.assertEqual(path.read_bytes(),b'old serverlist fix readme')
        self.assertTrue(any('ServerListFix' in row['reason'] for row in report['excluded']))
        path.write_bytes(b'new project instructions')
        game,_editor,_report=plan(self.repo,self.collect)
        self.assertIn('readme.txt',game)

    def test_stock_ui_metadata_supports_omitted_unchanged_menu_without_original_bytes(self):
        game,editor,_report=plan(self.repo,self.collect)
        files={name:resource.data() for name,resource in (game|editor).items()}
        for registry in REGISTRIES:
            files['ui/'+registry]=files['ui/'+registry].replace(b'loadMenu { "ui/main.menu" }',b'loadMenu { "ui/main.menu" } loadMenu { "ui/base_public.menu" }')
        reference={'schema':1,'release_version':'1.0.3','files':{'ui/base_public.menu':
                   {'sha256':'1'*64,'bytes':123,'menus':[{'name':'base_public','opens':[]}]}}}
        proof=validate_editor_ui(files,release_version='1.0.3',reference=reference)
        self.assertIn('ui/base_public.menu',proof['registries']['menus.txt']['stock_metadata_loaders'])
        self.assertNotIn('ui/base_public.menu',files)
        with self.assertRaisesRegex(ValueError,'version-bound base UI'):
            validate_editor_ui(files,release_version='1.0.2',reference=reference)
        reference['files']['ui/base_public.menu']['menus'][0]['name']='tce_ob_goalname'
        with self.assertRaisesRegex(ValueError,'cannot fall back'):
            validate_editor_ui(files,release_version='1.0.3',reference=reference)


if __name__ == '__main__':
    unittest.main()
