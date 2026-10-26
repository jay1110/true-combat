import tempfile
import threading
import unittest
import struct
import sys
from pathlib import Path
from types import SimpleNamespace
import botmenu
from navbundle import NavBundles
from build_botmenu import render_files
sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))

class BotMenuTest(unittest.TestCase):
    def test_catalog_in_generated_menu(self):
        files=render_files();menu='\n'.join(files.values())
        for item in botmenu.catalog('tce2')['actions']:
            self.assertIn('exec "bot '+item['command']+'"',menu)
        for value in files.values(): self.assertLess(len(value.encode('ascii')),32768)
        root=(Path(__file__).resolve().parents[1] / "src")
        for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt'):
            self.assertIn('ui/tce_omnibot.menu',(root.parent/'navstudio/editor-ui'/name).read_text())

    def test_tce_filters_et_only_goals_properties_and_classes(self):
        tce=botmenu.catalog('tce2');et=botmenu.catalog('et')
        self.assertIn('SWITCH',tce['types']);self.assertNotIn('BUILD',tce['types']);self.assertIn('BUILD',et['types'])
        self.assertNotIn('weapon',tce['properties']);self.assertIn('weapon',et['properties'])
        self.assertNotIn('jump',tce['properties']['stance']['values']);self.assertIn('jump',et['properties']['stance']['values'])
        with self.assertRaises(ValueError): botmenu.command(dict(action='create_BUILD'),'tce2')
        self.assertEqual(botmenu.command(dict(action='create_BUILD'),'et'),'bot goal_create BUILD')
        self.assertEqual(botmenu.command(dict(action='testbot_1_5')),'bot testbot 1 5')

    def test_switch_properties_have_real_bounds_and_allow_aim_without_value(self):
        for prop,value in (('stance','crouch'),('timeout','1500'),('use','1'),('aim','')):
            self.assertEqual(botmenu.command(dict(action='goal_property',property=prop,value=value)),
                             ('bot goal_setproperty '+prop+' '+value).rstrip())
        for prop,value in (('timeout','5001'),('timeout','1.5'),('use','2'),('radius','1000'),('stance','x;quit')):
            with self.assertRaises(ValueError): botmenu.command(dict(action='goal_property',property=prop,value=value))

    def test_voice_entry_preserves_tce_and_no_mainmenu_override(self):
        root=Path(__file__).resolve().parent/'editor-ui'
        self.assertFalse((root/'ingame_main.menu').exists())
        for file,key in (('wm_quickmessage.menu','o'),('wm_quickmessageAlt.menu','8'),
                         ('wm_ftquickmessage.menu','o'),('wm_ftquickmessageAlt.menu','8')):
            text=(root/file).read_text();self.assertIn('Statements',text);self.assertIn('omnibot_menu',text)
            self.assertIn('execKey "'+key+'"',text)
            self.assertIn('name "navstudio_voice_editor"',text)
            self.assertIn('type ITEM_TYPE_BUTTON text "^3'+key.upper()+'. TCE2 Omni-bot DMS"',text)
            self.assertIn('onOpen { setCvar cl_bypassMouseInput "0" }',text)
            self.assertIn('onEsc { uiScript closeingame ; }',text)
        members=(root/'editor-files.txt').read_text().splitlines()
        self.assertNotIn('ingame_main.menu',members)
        self.assertEqual(len(members),len(set(members)))
        for member in members:self.assertTrue((root/member).is_file(),member)
        launcher=(root.parent/'start-test.ps1').read_text()
        self.assertIn('foreach ($navEditorName in $navEditorNames)',launcher)
        self.assertNotIn('foreach ($navFile in Get-ChildItem -LiteralPath $navEditor -File)',launcher)

    def test_all_pages_links_resolve_and_command_rows_do_not_overlap_controls(self):
        import re
        files=render_files(); names=set(re.findall(r'menuDef \{ name "([^"]+)"','\n'.join(files.values())))
        for file,text in files.items():
            for target in re.findall(r'\bopen (tce_[A-Za-z0-9_]+)',text):self.assertIn(target,names,file)
            rows=[int(y) for y in re.findall(r'itemDef \{ name "[^"]+" rect 12 (\d+) 588 22',text)]
            for low,high in zip(sorted(rows),sorted(rows)[1:]):self.assertGreaterEqual(high-low,22,file)
        self.assertLess(len(names)+130,256)

    def test_catalog_ids_unique_and_waypoint_name_preserves_spaces_without_injection(self):
        self.assertEqual(len(botmenu.ACTIONS),len({a['id'] for a in botmenu.ACTIONS}))
        self.assertEqual(botmenu.command(dict(action='waypoint_name',name='Front Door Switch')),
                         'bot waypoint_setname Front Door Switch')
        self.assertEqual(botmenu.command(dict(action='waypoint_name',name='')),'bot waypoint_setname')
        with self.assertRaises(ValueError):botmenu.command(dict(action='waypoint_name',name='front;quit'))
    def test_production_ui_has_no_editor_entry(self):
        root=(Path(__file__).resolve().parents[1] / "src")
        for path in (root/'assets/ui').glob('*'):
            text=path.read_text()
            self.assertNotIn('tce_omnibot',text)
            self.assertNotIn('Omni-bot Editor',text)
        self.assertFalse((root/'assets/ui/tce_omnibot.menu').exists())
    def test_command_injection_rejected(self):
        for value in ['x;quit','x\nquit','"quit','../x']:
            with self.assertRaises(ValueError):
                botmenu.command(dict(action='goal_create_named',type='Attack',name=value))
            with self.assertRaises(ValueError):
                botmenu.command(dict(action='goal_property',property='name',value=value))
    def test_name_is_tagname_not_readonly_name(self):
        self.assertEqual(botmenu.command(dict(action='goal_property',property='name',value='corner')), 'bot goal_setproperty tagname corner')
        self.assertEqual(botmenu.command(dict(action='goal_property',property='group',value='')), 'bot goal_setproperty group')
    def test_save_uses_live_serializer_and_backup(self):
        with tempfile.TemporaryDirectory() as folder:
            bundles=NavBundles(folder); path=bundles.path('obj_test','goals'); path.write_bytes(b'old')
            calls=[]
            def rcon(cmd):
                calls.append(cmd); path.write_bytes(b'global _MG = {};'); return '0 Goals Saved Successfully, 0 Goals could not save, 0 skipped.'
            studio=SimpleNamespace(lock=threading.Lock(),busy=False,game_mode='tce2',require_map=lambda n:n,
                own_test_process=lambda **k: {},require_loaded=lambda n:None,live_gametype=lambda:5,nav_bundles=lambda gt:bundles,rcon=rcon)
            result=botmenu.execute(studio,dict(action='goal_save',map='obj_test',game_mode='tce2'))
            self.assertEqual(calls,['bot goal_save']);self.assertEqual(Path(result['backup']).read_bytes(),b'old')
            self.assertEqual(result['saved'],str(path))
    def test_wrong_mode_no_command(self):
        studio=SimpleNamespace(lock=threading.Lock(),busy=False,game_mode='et')
        with self.assertRaises(RuntimeError): botmenu.execute(studio,dict(action='goal_save',game_mode='tce2'))

    def project_fixture(self, folder, mode='tce2'):
        root=Path(folder);project=NavBundles(root/'project/bundles',5 if mode=='tce2' else None)
        runtime=NavBundles(root/'runtime/et/nav',5 if mode=='tce2' else None)
        project.root.mkdir(parents=True);runtime.root.mkdir(parents=True)
        studio=SimpleNamespace(lock=threading.RLock(),busy=False,game_mode=mode,
            active_project={'id':'project'},require_map=lambda n:n,own_test_process=lambda **k:{},
            require_loaded=lambda n:None,live_gametype=lambda:5,
            nav_bundles=lambda gt:project,runtime_nav_bundles=lambda gt:runtime)
        return studio,project,runtime

    def test_gt5_save_reads_real_runtime_goal_file_then_copies_exact_project_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            studio,project,runtime=self.project_fixture(folder)
            project.path('obj_test','goals').write_bytes(b'old project')
            runtime.path('obj_test','goals').write_bytes(b'old runtime')
            runtime.path('obj_test','script').write_bytes(b'global Map = {};')
            new=b'global _MG = { Version = 1, };'
            def rcon(cmd):
                self.assertEqual(cmd,'bot goal_save');runtime.path('obj_test','goals').write_bytes(new)
                return '^20 Goals Saved Successfully, 0 Goals could not save, 0 skipped.'
            studio.rcon=rcon
            result=botmenu.execute(studio,dict(action='goal_save',map='obj_test',game_mode='tce2',gametype=5))
            self.assertEqual(project.path('obj_test','goals').read_bytes(),new)
            self.assertEqual(Path(result['backup']).read_bytes(),b'old project')
            self.assertEqual(Path(result['runtime_backup']).read_bytes(),b'old runtime')
            self.assertEqual(Path(result['script_imported']).name,'obj_test_gt5.gm')

    def test_original_et_live_goal_save_uses_plain_map_stem_with_gt2_ui_value(self):
        with tempfile.TemporaryDirectory() as folder:
            studio,project,runtime=self.project_fixture(folder,'et')
            def rcon(cmd):
                runtime.path('goldrush','goals').write_bytes(b'global _MG = {};')
                return '0 Goals Saved Successfully, 0 Goals could not save, 0 skipped.'
            studio.rcon=rcon
            result=botmenu.execute(studio,dict(action='goal_save',map='goldrush',game_mode='et',gametype=2))
            self.assertEqual(Path(result['saved']).name,'goldrush_goals.gm')
            self.assertEqual(result['runtime_saved'],str(runtime.path('goldrush','goals')))

    def test_stale_runtime_export_is_not_copied_even_if_reply_says_saved(self):
        with tempfile.TemporaryDirectory() as folder:
            studio,project,runtime=self.project_fixture(folder)
            project.path('obj_test','goals').write_bytes(b'keep project')
            runtime.path('obj_test','goals').write_bytes(b'global _MG = {};')
            studio.rcon=lambda cmd:'0 Goals Saved Successfully, 0 Goals could not save, 0 skipped.'
            with self.assertRaisesRegex(RuntimeError,'nicht neu geschrieben'):
                botmenu.execute(studio,dict(action='goal_save',map='obj_test',game_mode='tce2',gametype=5))
            self.assertEqual(project.path('obj_test','goals').read_bytes(),b'keep project')

    def test_partial_native_goal_save_does_not_replace_project(self):
        with tempfile.TemporaryDirectory() as folder:
            studio,project,runtime=self.project_fixture(folder)
            project.path('obj_test','goals').write_bytes(b'keep project')
            def rcon(cmd):
                runtime.path('obj_test','goals').write_bytes(b'global _MG = {};')
                return '0 Goals Saved Successfully, 1 Goals could not save, 0 skipped.'
            studio.rcon=rcon
            with self.assertRaisesRegex(RuntimeError,'nicht alle Goals'):
                botmenu.execute(studio,dict(action='goal_save',map='obj_test',game_mode='tce2',gametype=5))
            self.assertEqual(project.path('obj_test','goals').read_bytes(),b'keep project')

    def test_unsaved_way_draft_blocks_rcon_before_live_file_or_backup_changes(self):
        with tempfile.TemporaryDirectory() as folder:
            studio,project,runtime=self.project_fixture(folder)
            studio.drafts=SimpleNamespace(get=lambda n:{'revision':3,'saved_revision':2})
            studio.rcon=lambda cmd:self.fail('RCON must not run before conflict is resolved')
            with self.assertRaisesRegex(RuntimeError,'Ungespeicherte Studio'):
                botmenu.execute(studio,dict(action='way_save',map='obj_test',game_mode='tce2',gametype=5))
            self.assertFalse((project.root/'.navstudio-backups').exists())

    def test_fresh_way_save_copies_before_invalidating_old_scene_and_returns_new_draft(self):
        with tempfile.TemporaryDirectory() as folder:
            studio,project,runtime=self.project_fixture(folder)
            # Valid two-node v9 graph, sequential UIDs, no flags/properties.
            header=struct.pack('<BI32s232s6f',9,2,b'',b'',0,0,0,128,0,24)
            binary=header+b'\0'+struct.pack('<3f',0,0,24)+b'\x81\x82'+b'\0'+struct.pack('<3f',128,0,24)+b'\x81\x81'
            studio.drafts=SimpleNamespace(get=lambda n:{'revision':3,'saved_revision':3},
                view=lambda n,e:{'revision':0,'source':'fresh live WAY'})
            studio.draft_environment=lambda n:{}
            calls=[]
            def copied(name):
                calls.append(name);self.assertEqual(project.path(name,'way').read_bytes(),binary)
            studio.live_way_saved=copied
            def rcon(cmd):runtime.path('obj_test','way').write_bytes(binary);return 'Saved Waypoints.'
            studio.rcon=rcon
            result=botmenu.execute(studio,dict(action='way_save',map='obj_test',game_mode='tce2',gametype=5))
            self.assertEqual(calls,['obj_test'])
            self.assertEqual(result['draft']['source'],'fresh live WAY')
            self.assertFalse(result['native_checked'])

if __name__=='__main__': unittest.main()
