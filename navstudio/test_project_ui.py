"""Project wizard/save/reopen/spawn UI regression checks, no live game."""
import copy
import json
from pathlib import Path
import unittest
from urllib.parse import urlparse

import test_mouse_ui as legacy


class ProjectWorkflowTests(unittest.TestCase):
    xy = legacy.MouseEditorTests.xy
    @classmethod
    def setUpClass(cls):
        legacy.MouseEditorTests.setUpClass.__func__(cls)

    @classmethod
    def tearDownClass(cls):
        legacy.MouseEditorTests.tearDownClass.__func__(cls)

    def setUp(self):
        self.projects, self.active, self.jobs = [], None, []
        self.scene_reads=0;self.live_save=False;self.spawn_failure=False
        self.project_requests = []
        self.package_requests=[];self.package_error=None;self.package_abort=False
        self.package_bytes=legacy.NavBundles.archive_project('obj_test',b'unchanged WAY',
            {'obj_test_gt2.gm':b'global Map={};','obj_test_gt2_goals.gm':b'global _MG={};'},
            {'obj_test_gt2.script':b'game_manager {}'},dict(mod='tce2'))
        self.spawn_document = dict(revision=0, enabled=True, origin_above_feet=30,
            nodes=[dict(id='bsp-1', kind='bsp', editable=False, classname='team_CTF_redspawn',
                        position=[80, 320, 24], angle=0),
                   dict(id='generated-1', kind='generated', editable=True, classname='info_player_deathmatch',
                        position=[320, 320, 30], angle=90, origin_above_feet=30)])
        legacy.MouseEditorTests.setUp(self)
        self.page.evaluate("document.body.classList.add('project-inactive');document.querySelector('#project-welcome').hidden=false;document.querySelector('#map-select').parentElement.hidden=true")
        self.requests.clear()

    def tearDown(self):
        legacy.MouseEditorTests.tearDown(self)

    def project_state(self):
        return dict(active=self.active, projects=self.projects,
                    catalog=[dict(source_id='tce-source', map='obj_test', mod='tce2', filename='obj_test.pk3', location='all_maps', size=1024),
                             dict(source_id='et-source', map='radar', mod='et', filename='pak0.pk3', location='installed', size=2048)])

    def route(self, route):
        path = urlparse(route.request.url).path
        if path=='/api/scene':self.scene_reads+=1
        if path=='/api/project-package.zip':
            self.package_requests.append(route.request.url)
            if self.package_abort:route.abort('connectionreset');return
            if self.package_error:
                route.fulfill(body=json.dumps({'error':self.package_error}),content_type='application/json',status=409)
            else:route.fulfill(body=self.package_bytes,content_type='application/zip')
            return
        if path == '/api/projects':
            result = self.project_state()
        elif path == '/api/state':
            result = dict(token='isolated-test-token', maps=['obj_test', 'obj_other'], selected_map='obj_test',
                          game_mode=self.active['mod'] if self.active else 'tce2',
                          active_project=self.active, jobs=copy.deepcopy(self.jobs), game={'state': 'offline'})
        elif path == '/api/project':
            payload = route.request.post_data_json
            self.project_requests.append(payload)
            self.assertEqual(route.request.headers['x-navstudio-token'], 'isolated-test-token')
            op = payload['op']
            if op == 'create':
                self.active = dict(id='project-' + str(len(self.projects)), name=payload['name'], map=payload['map'],
                                   mod=payload['mod'], gametype=payload['gametype'], path=str(Path(self.tmp.name) / 'project'))
                self.projects.append(self.active)
            elif op == 'open':
                self.active = next(p for p in self.projects if p['id'] == payload['id'])
            elif op == 'save_close':
                draft=self.store.get('obj_test')
                if draft:self.store.save('obj_test',draft['revision'],draft['base']['environment'])
                self.active = None
            elif op == 'save' and self.active:
                draft=self.store.get('obj_test')
                if draft:self.store.save('obj_test',draft['revision'],draft['base']['environment'])
                self.active['gametype'] = payload.get('gametype', self.active['gametype'])
            result = self.project_state()
            if op == 'import_bundle':
                result['imported'] = [{'kind': 'way', 'sha256': 'fixture'}]
        elif path == '/api/spawns':
            if route.request.method == 'POST':
                if self.spawn_failure:
                    route.fulfill(body=json.dumps({'error':'Simulierter Spawn-Schreibfehler'}),content_type='application/json',status=409);return
                payload = route.request.post_data_json
                self.requests.append(payload)
                self.assertEqual(payload['revision'], self.spawn_document['revision'])
                if payload['op'] == 'move':
                    node = next(n for n in self.spawn_document['nodes'] if n['id'] == payload['id'])
                    self.assertTrue(node['editable'])
                    node['position'] = payload['position']; node['angle'] = payload['angle']
                elif payload['op'] == 'delete':
                    self.spawn_document['nodes'] = [n for n in self.spawn_document['nodes'] if n['id'] != payload['id']]
                elif payload['op'] == 'enabled':
                    self.spawn_document['enabled'] = payload['enabled']
                self.spawn_document['revision'] += 1
            result = self.spawn_document
        elif path == '/api/edit' and route.request.post_data_json.get('op') == 'save':
            payload=route.request.post_data_json;self.requests.append(payload)
            self.store.save('obj_test',payload['revision'],self.environment)
            result={'draft':self.store.view('obj_test',self.environment),'note':'Gespeichert'}
        elif path=='/api/botmenu' and route.request.method=='POST' and self.live_save:
            payload=route.request.post_data_json;self.requests.append(payload)
            result={'command':'bot goal_save','reply':'2 Goals Saved Successfully, 0 Goals could not save',
                    'map':'obj_test','saved':str(self.bundles.root/'obj_test_goals.gm')}
        elif path == '/api/action':
            payload = route.request.post_data_json
            self.requests.append(payload)
            job = dict(id='project-job-' + str(len(self.jobs)), status='queued', action=payload['action'], map=payload['map'])
            self.jobs.append(job); result = {'job': job}
        else:
            return legacy.MouseEditorTests.route(self, route)
        route.fulfill(body=json.dumps(result), content_type='application/json')

    def create(self, operation='import'):
        self.page.click('#welcome-new')
        self.page.fill('#wizard-name', 'Meine Testmap')
        self.page.click('#wizard-next'); self.page.click('#wizard-next')
        self.page.select_option('#wizard-map', 'tce-source')
        self.page.click('#wizard-next')
        self.page.check('[name=wizard-navigation][value=' + operation + ']')
        self.page.click('#wizard-next')
        self.page.wait_for_function("!document.body.classList.contains('project-inactive') && !document.querySelector('#project-wizard').open && window.navstudioClient.app.scene && !window.navstudioClient.app.loading && !window.navstudioProjects.model.busy")

    def test_start_requires_explicit_project_and_never_starts_game(self):
        self.assertTrue(self.page.is_visible('#project-welcome'))
        self.assertFalse(self.page.is_visible('#workflow-toolbar'))
        self.assertFalse(self.requests)
        self.assertFalse(self.project_requests)

    def test_wizard_filters_mod_and_imports_into_named_project(self):
        self.create()
        create = self.project_requests[0]
        self.assertEqual({k: create[k] for k in ('name','map','mod','gametype','source_id')},
                         dict(name='Meine Testmap', map='obj_test', mod='tce2', gametype=2, source_id='tce-source'))
        self.assertEqual([p['op'] for p in self.project_requests], ['create', 'import_bundle'])
        self.assertIn('Meine Testmap', self.page.inner_text('#project-title'))
        for selector in ('#project-save', '[data-action=generate]', '[data-action=native_check]', '#apply-draft', '#tree-goals-2', '#tree-goals-5'):
            self.assertTrue(self.page.is_visible(selector), selector)
        self.assertFalse(any(r.get('action') in ('load_map', 'generate') for r in self.requests))

    def test_et_map_filter_has_no_obj_maps(self):
        self.page.click('#welcome-new'); self.page.fill('#wizard-name', 'ET-Test')
        self.page.click('#wizard-next'); self.page.check('[name=wizard-mod][value=et]'); self.page.click('#wizard-next')
        self.assertEqual(self.page.locator('#wizard-map option').all_text_contents(), ['radar · installiert'])
        self.assertTrue(self.page.is_disabled('[name=wizard-navigation][value=generate]'))
        self.assertTrue(self.page.is_disabled('#wizard-gametype'))
        self.assertEqual(self.page.input_value('#wizard-gametype'),'2')

    def test_project_save_close_reopen_preserves_deleted_node(self):
        self.create(); legacy.MouseEditorTests.begin(self)
        legacy.MouseEditorTests.point(self, 150)
        self.page.keyboard.press('Delete'); legacy.MouseEditorTests.revision(self, 1)
        self.page.click('#project-save')
        self.page.wait_for_function("window.navstudioClient.app.draft?.saved_revision === 1")
        self.assertNotIn(1, {n['id'] for n in self.store.get('obj_test')['nodes']})
        self.page.click('#project-close'); self.page.wait_for_selector('#project-welcome')
        self.page.locator('#project-recent button').click()
        self.page.wait_for_function('window.navstudioClient.app.draft !== null && !window.navstudioClient.app.loading')
        self.assertNotIn(1, self.page.evaluate('window.navstudioClient.app.draft.nodes.map(n=>n.id)'))

    def test_generated_spawn_edit_delete_and_immutable_bsp(self):
        self.create()
        self.page.locator('[data-spawn-id="bsp-1"]').click()
        self.assertTrue(self.page.is_disabled('#spawn-delete'))
        self.page.locator('[data-spawn-id="generated-1"]').click()
        self.page.fill('#spawn-x', '400'); self.page.fill('#spawn-angle', '180'); self.page.click('#spawn-move')
        self.page.wait_for_function('window.navstudioProjects.model.spawns.revision === 1')
        own = next(n for n in self.spawn_document['nodes'] if n['editable'])
        self.assertEqual(own['position'], [400, 320, 30]); self.assertEqual(own['angle'], 180)
        self.page.click('#spawn-delete')
        self.page.wait_for_function('window.navstudioProjects.model.spawns.revision === 2')
        self.assertEqual([n['id'] for n in self.spawn_document['nodes']], ['bsp-1'])

    def test_generating_project_uses_one_complete_server_pipeline(self):
        self.create('generate')
        self.assertEqual([r.get('action') for r in self.requests], ['generate'])
        self.assertTrue(self.requests[0]['project_goals'])
        self.assertEqual(self.requests[0]['goal_gametypes'],[2,5])
        self.jobs[-1]['status'] = 'complete'
        self.page.evaluate('window.navstudioClient.refresh(false)')
        self.page.wait_for_function("window.navstudioClient.app.watchedJob === null && !window.navstudioClient.app.loading")
        self.assertEqual([r.get('action') for r in self.requests], ['generate'])

    def test_package_export_visible_gated_and_success_is_one_zip_download(self):
        self.create()
        self.assertTrue(self.page.is_visible('#project-package-export'))
        self.assertTrue(self.page.is_disabled('#project-package-export'))
        self.scene_extra['package_status']=dict(ready=False,reasons=['GT5-Goals fehlen.'])
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.assertIn('GT5-Goals fehlen.',self.page.text_content('#project-package-status'))
        self.scene_extra['package_status']=dict(ready=True,reasons=[])
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.assertFalse(self.page.is_disabled('#project-package-export'))
        with self.page.expect_download() as pending:
            self.page.click('#project-package-export')
        download=pending.value
        self.assertEqual(download.suggested_filename,'obj_test-navigation.zip')
        self.assertEqual(Path(download.path()).read_bytes(),self.package_bytes)
        self.assertEqual(len(self.package_requests),1)
        self.assertFalse(self.requests)

    def test_package_http_error_is_visible_and_never_downloaded(self):
        self.create();self.scene_extra['package_status']=dict(ready=True,reasons=[])
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        downloads=[];self.page.on('download',lambda item:downloads.append(item))
        self.package_error='GT5-Prüfbeleg ist veraltet; bitte automatisch neu erstellen.'
        self.page.click('#project-package-export')
        self.page.wait_for_function("document.querySelector('#notice').textContent.includes('GT5-Prüfbeleg')")
        self.assertEqual(downloads,[]);self.assertEqual(len(self.package_requests),1)
        self.assertFalse(self.page.is_disabled('#project-package-export'))

    def test_package_busy_job_blocks_export_without_hidden_menu(self):
        self.create();self.scene_extra['package_status']=dict(ready=True,reasons=[])
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.jobs=[dict(id='busy-package',status='running',action='generate',map='obj_test')]
        self.page.evaluate('window.navstudioClient.refresh(false)')
        self.assertTrue(self.page.is_disabled('#project-package-export'))
        self.assertIn('Auftrag läuft',self.page.text_content('#project-package-status'))
        self.assertFalse(self.package_requests)

    def test_package_network_disconnect_is_visible_without_download_or_retry(self):
        self.create();self.scene_extra['package_status']=dict(ready=True,reasons=[])
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.package_abort=True
        downloads=[];self.page.on('download',lambda item:downloads.append(item))
        self.page.click('#project-package-export')
        self.page.wait_for_function("document.querySelector('#notice').textContent.includes('nicht erreichbar')")
        self.assertEqual(downloads,[]);self.assertEqual(len(self.package_requests),1)
        self.assertFalse(self.page.is_disabled('#project-package-export'))

    def test_scene_mapscripts_choose_available_gt_and_keep_valid_project_preference(self):
        self.create();self.active['gametype']=7
        self.scene_extra['gametype_availability']=dict(available=[5,7],gametypes={
            '2':dict(gametype=2,available=False,reason='obj_test_gt2.script fehlt.'),
            '5':dict(gametype=5,available=True,reason=None),
            '7':dict(gametype=7,available=True,reason=None)})
        self.page.evaluate('window.navstudioClient.refresh(false)')
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.assertEqual(self.page.input_value('#test-gametype'),'7')
        self.assertTrue(self.page.is_disabled('#test-gametype option[value="2"]'))
        self.assertIn('fehlt',self.page.get_attribute('#test-gametype option[value="2"]','title'))
        self.page.select_option('#test-gametype','5')
        self.page.click('#project-save')
        self.page.wait_for_function('!window.navstudioClient.app.loading && !window.navstudioProjects.model.busy')
        self.assertEqual(self.page.input_value('#test-gametype'),'5')
        self.assertEqual(self.project_requests[-1]['gametype'],5)
        self.page.click('[data-action="generate"]')
        self.page.wait_for_function('window.navstudioClient.app.watchedJob!==null')
        self.assertEqual(self.requests[-1]['gametype'],5)

    def test_no_available_gt_blocks_generation_and_english_export_label(self):
        self.create();self.scene_extra['gametype_availability']=dict(available=[],gametypes={
            str(gt):dict(gametype=gt,available=False,reason='Kein Mapscript.')for gt in (2,5,7)})
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.page.evaluate('window.navstudioClient.refresh(false)')
        self.assertEqual(self.page.input_value('#test-gametype'),'')
        self.assertTrue(self.page.is_disabled('#test-gametype'))
        self.assertTrue(self.page.is_disabled('[data-action="generate"]'))
        self.page.click('#language-toggle')
        self.page.wait_for_function("document.querySelector('#project-package-export').textContent.startsWith('Export waypoints')")
        self.assertIn('Automatically create',self.page.text_content('[data-action="generate"]'))
        self.assertEqual(self.requests,[])

    def test_spawn_mouse_drag_keeps_player_origin_offset(self):
        self.create()
        self.page.mouse.move(*self.xy(320,320));self.page.mouse.down()
        self.page.mouse.move(*self.xy(400,350),steps=4);self.page.mouse.up()
        self.page.wait_for_function('window.navstudioProjects.model.spawns.revision === 1')
        node=next(n for n in self.spawn_document['nodes'] if n['editable'])
        self.assertEqual(node['position'],[400,350,30.125])

    def test_failed_spawn_drag_restores_actual_project_position(self):
        self.create();self.spawn_failure=True
        self.page.mouse.move(*self.xy(320,320));self.page.mouse.down()
        self.page.mouse.move(*self.xy(400,350),steps=4);self.page.mouse.up()
        self.page.wait_for_function("document.querySelector('#spawn-status').textContent.includes('Simulierter Spawn-Schreibfehler')")
        self.assertEqual(self.page.evaluate("window.fixtureViewer.spawns.find(s=>s.id==='generated-1').position"),[320,320,30])
        self.assertEqual(self.spawn_document['revision'],0)

    def test_spawn_inputs_survive_status_poll_and_job_blocks_drag(self):
        self.create();self.page.locator('[data-spawn-id="generated-1"]').click()
        self.page.fill('#spawn-x','407');self.page.wait_for_timeout(1100)
        self.assertEqual(self.page.input_value('#spawn-x'),'407')
        self.jobs=[dict(id='busy-job',map='obj_test',action='native_check',status='running')]
        self.page.wait_for_selector('#spawn-move[disabled]')
        self.page.mouse.move(*self.xy(320,320));self.page.mouse.down()
        self.page.mouse.move(*self.xy(400,350),steps=4);self.page.mouse.up()
        self.assertEqual(self.spawn_document['revision'],0)

    def test_unsaved_goal_text_blocks_window_close_until_saved(self):
        self.create();self.page.click('#tree-goals-2')
        self.page.wait_for_selector('#navbundle-text:not([disabled])')
        authored='global _MG = { Authored = true };\n'
        self.page.fill('#navbundle-text',authored)
        self.assertFalse(self.page.evaluate('window.navstudioBundle.canClose()'))
        self.assertEqual(self.page.input_value('#navbundle-text'),authored)
        self.assertIn('Ungespeichert',self.page.text_content('#navbundle-status'))
        self.page.click('#navbundle-save')
        self.page.wait_for_function("document.querySelector('#navbundle-status').textContent.includes('Saved on disk')")
        self.assertEqual(self.bundles.path('obj_test','goals').read_text(),authored)
        self.assertTrue(self.page.evaluate('window.navstudioBundle.canClose()'))

    def test_edit_hides_old_errors_and_native_check_has_no_prerequisite_popup(self):
        self.create();legacy.MouseEditorTests.begin(self)
        legacy.MouseEditorTests.point(self,310);self.page.keyboard.press('Delete');legacy.MouseEditorTests.revision(self,1)
        self.assertEqual(self.page.locator('#problem-list button').count(),0)
        self.assertIn('Alte Fehler ausgeblendet',self.page.inner_text('#problem-summary'))
        self.page.click('[data-action=native_check]')
        self.page.wait_for_function("window.navstudioClient.app.watchedJob !== null")
        self.assertFalse(self.page.is_visible('#prerequisite-dialog'))
        self.assertEqual(self.requests[-1]['action'],'native_check')

    def test_f5_reloads_ui_and_keeps_open_project_and_edit(self):
        self.create();legacy.MouseEditorTests.begin(self)
        legacy.MouseEditorTests.point(self,150);self.page.keyboard.press('Delete');legacy.MouseEditorTests.revision(self,1)
        self.page.keyboard.press('F5')
        self.page.wait_for_function('window.navstudioClient?.app.draft?.revision===1 && !window.navstudioClient.app.loading')
        self.assertFalse(self.page.is_visible('#project-welcome'))
        self.assertNotIn(1,self.page.evaluate('window.navstudioClient.app.draft.nodes.map(n=>n.id)'))
        self.assertEqual([p['op'] for p in self.project_requests],['create','import_bundle'])

    def test_new_project_labels_translate_without_changing_map_identity(self):
        self.page.click('#language-toggle')
        self.page.wait_for_function("document.querySelector('#welcome-new').textContent==='Create new project'")
        self.assertIn('New project',self.page.text_content('#project-new'))
        self.page.click('#welcome-new');self.page.fill('#wizard-name','Mein Projekt')
        self.page.click('#wizard-next');self.page.click('#wizard-next')
        self.assertEqual(self.page.locator('#wizard-map option').all_text_contents(),['obj_test · all_maps'])

    def test_late_batch_project_appears_in_welcome_without_auto_opening(self):
        self.active=dict(id='batch-1',name='Batch Test',map='obj_test',mod='tce2',gametype=2,path=str(Path(self.tmp.name)/'batch'))
        self.projects=[self.active]
        self.page.wait_for_selector('#welcome-current:not([hidden])')
        self.assertTrue(self.page.is_visible('#project-welcome'))
        self.page.click('#welcome-current')
        self.page.wait_for_function("window.navstudioProjects.model.enteredId==='batch-1'")
        self.assertFalse(self.project_requests)

    def test_stale_draft_can_be_saved_and_closed_without_export(self):
        self.create();legacy.MouseEditorTests.begin(self)
        self.store.get('obj_test')['base']['environment']={'older':'source'}
        self.page.click('#project-save')
        self.page.wait_for_function('window.navstudioClient.app.draft?.stale && window.navstudioClient.app.draft.saved_revision===0')
        self.page.click('#project-close');self.page.wait_for_selector('#project-welcome')
        self.assertEqual(self.project_requests[-1]['op'],'save_close')

    def test_goal_apply_requires_current_proof_and_sends_selected_gt(self):
        self.create()
        self.assertTrue(self.page.is_disabled('#project-apply-goals'))
        self.page.evaluate("Object.assign(window.navstudioClient.app.scene,{proof_status:{valid:true},report_status:{current:true,generation_complete:true},audit_state:'current'})")
        self.page.select_option('#test-gametype','5')
        self.assertIn('GT5',self.page.text_content('#project-apply-goals'))
        self.assertFalse(self.page.is_disabled('#project-apply-goals'))
        self.assertIn('Gametype neu',self.page.text_content('#project-goals-restart'))
        self.page.click('#project-apply-goals')
        self.page.wait_for_function("window.navstudioClient.app.watchedJob!==null")
        self.assertEqual(self.requests[-1]['action'],'apply_goals');self.assertEqual(self.requests[-1]['gametype'],5)

    def test_confirmed_live_save_refreshes_project_view(self):
        self.create();self.live_save=True;reads=self.scene_reads
        self.page.click('#tree-live')
        self.page.wait_for_selector('#botmenu-type option',state='attached')
        self.page.get_by_role('button',name='Save Goals',exact=True).click()
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('Saved Successfully')")
        self.page.wait_for_timeout(200)
        self.assertGreater(self.scene_reads,reads)

    def test_et_project_has_objective2_and_common_goal_controls(self):
        self.page.click('#welcome-new');self.page.fill('#wizard-name','ET Objective')
        self.page.click('#wizard-next');self.page.check('[name=wizard-mod][value=et]');self.page.click('#wizard-next')
        self.page.select_option('#wizard-map','et-source');self.page.click('#wizard-next');self.page.click('#wizard-next')
        self.page.wait_for_function("window.navstudioProjects.model.enteredId!==undefined && !document.querySelector('#project-wizard').open")
        self.assertEqual(self.project_requests[0]['gametype'],2)
        self.assertTrue(self.page.is_disabled('#test-gametype'));self.assertEqual(self.page.input_value('#test-gametype'),'2')
        self.assertIn('Objective GT2',self.page.text_content('#project-map'))
        self.assertEqual(self.page.text_content('#tree-goals-2'),'⚑ET-Goals')
        self.assertFalse(self.page.is_visible('#tree-goals-5'));self.assertFalse(self.page.is_visible('#project-generate-goals'))
        for selector in ('#spawn-generate','#spawn-apply','#spawn-enabled'):
            self.assertTrue(self.page.is_disabled(selector),selector)


    def test_problem_list_grows_below_map_without_inner_scrolling(self):
        self.create()
        before = self.page.locator('#viewer').bounding_box()['height']
        self.scene_extra = dict(audit={'unresolved_nodes':[
            dict(source_id=i,position=[150,220,0],incoming=1,outgoing=0) for i in range(150)]})
        self.page.evaluate("window.navstudioClient.loadScene('obj_test',false)")
        self.page.wait_for_function("document.querySelectorAll('#problem-list .reason-button').length===150")
        layout = self.page.evaluate("""() => {
          const dock=document.querySelector('#results-dock'), list=document.querySelector('#problem-list'),
              map=document.querySelector('.viewer-area'), canvas=document.querySelector('#viewer');
          return {dockY:dock.getBoundingClientRect().top,mapEnd:map.getBoundingClientRect().bottom,
              canvasHeight:canvas.clientHeight,listHeight:list.clientHeight,listFull:list.scrollHeight,
              dockHeight:dock.clientHeight,dockFull:dock.scrollHeight,
              listOverflow:getComputedStyle(list).overflowY,dockOverflow:getComputedStyle(dock).overflowY,
              pageHeight:document.documentElement.scrollHeight,viewHeight:innerHeight};
        }""")
        self.assertGreaterEqual(layout['dockY'],layout['mapEnd'])
        self.assertAlmostEqual(layout['canvasHeight'],before,delta=1)
        self.assertEqual(layout['listHeight'],layout['listFull'])
        self.assertEqual(layout['dockHeight'],layout['dockFull'])
        self.assertEqual(layout['listOverflow'],'visible')
        self.assertEqual(layout['dockOverflow'],'visible')
        self.assertGreater(layout['pageHeight'],layout['viewHeight']+500)
        self.page.locator('#problem-list .reason-button').last.scroll_into_view_if_needed()
        self.assertTrue(self.page.locator('#problem-list .reason-button').last.is_visible())

    def test_game_lock_controls_are_absent_in_both_languages(self):
        self.create()
        for selector in ('[data-action=watch_on]','[data-action=watch_off]'):
            self.assertEqual(self.page.locator(selector).count(),0)
        self.assertNotIn('Nur zuschauen',self.page.inner_text('body'))
        self.assertNotIn('Bedienung freigeben',self.page.inner_text('body'))



if __name__ == '__main__':
    unittest.main()
