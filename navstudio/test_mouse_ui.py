"""Isolated Chromium UI tests. All HTTP is intercepted; no Studio/game/RCON.

Uses a deterministic screen-space viewer fixture for input gestures, the real
Drafts model for requests, and separate checks of real viewer geometry helpers.
Requires Python Playwright and an installed Chrome/Edge (headless only).
"""
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
from urllib.parse import urlparse

from edit import Drafts
from navbundle import NavBundles
from server import Handler
import botmenu
from playwright.sync_api import sync_playwright

HERE = Path(__file__).resolve().parent
VIEWER = r"""
class BSPViewer {
  constructor(canvas) { this.canvas=canvas; window.fixtureViewer=this; this._navigationInspect=true; this.cameraChanges=0;
    canvas.addEventListener('click', e=>{if(this.onNavigationClick&&this.onNavigationClick(e))return;
      if(this.onNavigationPlace){this.onNavigationPlace(this.navigationFloorPoint(e.clientX,e.clientY));return;}this.pickNavigation(e.clientX,e.clientY);}); }
  setNavigation(nav) { const old=this._navigationSelection; this._navigation=nav; this._navigationNodeMap={};
    nav.nodes.forEach(n=>{n._navId=n.id;this._navigationNodeMap[n.id]=n;});
    this._navigationSelection=null; if(old){ if(old.kind==='node')this.selectNavigation('node',old.id);
      else {let i=nav.edges.findIndex(e=>e.from===old.item.from&&e.to===old.item.to);if(i>=0)this.selectNavigation('edge',i);} }this.draw(); }
  setNavigationOptions(o){this.options=o;this.draw();} setAssetMap(){} async load(){} start(){this.draw();} destroy(){}
  stop(){} setNavigationSpawns(spawns){this.spawns=structuredClone(spawns);this.draw();}
  previewNavigationSpawn(id,p){let s=this.spawns.find(s=>s.id===id);s.position=p.slice();this.draw();return true;}
  focusNavigation(target){this.focusedTarget=target;} enterNavigation(){return true;} setNavigationInspect(v){this._navigationInspect=v;}
  getCameraState(){return {position:[200,200,100],mode:this._navigationInspect?'inspect':'fly'};}
  selectNavigation(kind,id){const item=kind==='node'?this._navigationNodeMap[id]:kind==='edge'?this._navigation.edges[id]:kind==='spawn'?(this.spawns||[]).find(s=>s.id===id):null;
    this._navigationSelection=item?{kind,id,item}:null; this.draw(); if(this.onNavigationSelect)this.onNavigationSelect(this._navigationSelection);return this._navigationSelection;}
  projectNavigationPoint(p){const r=this.canvas.getBoundingClientRect();return[r.left+p[0],r.top+p[1]];}
  navigationFloorPoint(x,y){const r=this.canvas.getBoundingClientRect();return x-r.left>600?null:[x-r.left,y-r.top,0.125];}
  previewNavigationNode(id,p){this._navigationNodeMap[id].position=p.slice();this.draw();return true;}
  setNavigationEditPreview(v){this._navigationEditPreview=v;}
  navigationCameraGesture(){this.cameraChanges++;} navigationCameraZoom(){this.cameraChanges++;}
  pickNavigation(x,y,options={}){const r=this.canvas.getBoundingClientRect();x-=r.left;y-=r.top;
    if(!options.nodesOnly){let s=(this.spawns||[]).find(s=>Math.hypot(s.position[0]-x,s.position[1]-y)<15);if(s)return options.select===false?{kind:'spawn',id:s.id,item:s}:this.selectNavigation('spawn',s.id);}
    if(options.spawnsOnly)return null;
    let n=this._navigation.nodes.find(n=>Math.hypot(n.position[0]-x,n.position[1]-y)<15),hit=n?{kind:'node',id:n.id,item:n}:null;
    if(!hit&&!options.nodesOnly){let i=this._navigation.edges.findIndex(e=>{let a=this._navigationNodeMap[e.from].position,b=this._navigationNodeMap[e.to].position;return Math.hypot((a[0]+b[0])/2-x,(a[1]+b[1])/2-y)<15});if(i>=0)hit={kind:'edge',id:i,item:this._navigation.edges[i]};}
    return options.select===false?hit:this.selectNavigation(hit&&hit.kind,hit&&hit.id);}
  draw(){const c=this.canvas,ctx=c.getContext('2d');c.width=c.clientWidth;c.height=c.clientHeight;ctx.fillStyle='#18242b';ctx.fillRect(0,0,c.width,c.height);
    if(!this._navigation)return;ctx.strokeStyle='#3b5059';for(let x=20;x<c.width;x+=40){ctx.beginPath();ctx.moveTo(x,30);ctx.lineTo(x,c.height);ctx.stroke();}
    ctx.fillStyle='#b6cbd2';ctx.font='14px sans-serif';ctx.fillText('Isolierte UI-PrÃ¼fflÃ¤che Â· keine Spielkarte',25,35);
    ctx.strokeStyle='#d3e5ec';this._navigation.edges.forEach(e=>{let a=this._navigationNodeMap[e.from].position,b=this._navigationNodeMap[e.to].position;ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(b[0],b[1]);ctx.stroke();});
    this._navigation.nodes.forEach(n=>{ctx.strokeStyle=n.locked?'#eeaa55':'#ef76e9';ctx.beginPath();ctx.arc(n.position[0],n.position[1],n.radius,0,Math.PI*2);ctx.stroke();ctx.fillStyle=ctx.strokeStyle;ctx.fillRect(n.position[0]-4,n.position[1]-4,8,8);ctx.fillText(String(n.id),n.position[0]+8,n.position[1]-8);}); }
}
window.BSPViewer=BSPViewer;
"""


def encoded_response(value):
    """Use the production HTTP JSON writer, not a permissive fixture encoder."""
    handler=Handler.__new__(Handler);handler.wfile=io.BytesIO()
    handler.send_response=lambda status:None;handler.send_header=lambda key,value:None;handler.end_headers=lambda:None
    handler.send(200,value)
    return handler.wfile.getvalue()


class MouseEditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.playwright = sync_playwright().start()
        candidates = [Path('C:/Program Files/Google/Chrome/Application/chrome.exe'), Path('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe')]
        executable = next((p for p in candidates if p.exists()), None)
        cls.browser = cls.playwright.chromium.launch(headless=True, executable_path=str(executable) if executable else None)

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.playwright.stop()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='navstudio-ui-test-')
        self.store = Drafts(Path(self.tmp.name))
        self.bundles = NavBundles(Path(self.tmp.name) / 'nav')
        self.bundles.root.mkdir()
        (self.bundles.root / 'obj_test.way').write_bytes(b'original way')
        (self.bundles.root / 'obj_test.gm').write_text('global Map = {};\n')
        (self.bundles.root / 'obj_test_goals.gm').write_text('global _MG = {};\n')
        self.environment = {'fixture': 'screen-space'}
        self.scene_extra = {}
        self.nav = dict(nodes=[dict(id=i, position=[x, 220, 0], radius=35) for i, x in [(1, 150), (2, 310), (3, 430)]], edges=[dict(zip(('from', 'to'), pair)) for pair in [(1, 2), (2, 1)]])
        self.base = dict(environment=self.environment, protected_anchor_ids=[3])
        self.requests = []
        self.fail_next = False
        self.settings_failure = False
        self.errors = []
        self.context = self.browser.new_context(viewport={'width': 1600, 'height': 1000})
        self.page = self.context.new_page()
        self.page.on('pageerror', lambda error: self.errors.append(str(error)))
        self.page.route('**/*', self.route)
        self.page.goto('http://navstudio.test/')
        self.page.wait_for_selector('#map-select option[value="obj_test"]', state='attached')
        self.assertEqual(self.page.locator('#map-select').input_value(), '')
        # Legacy gesture tests exercise the graph editor independently of the project wizard.
        self.page.evaluate("document.body.classList.remove('project-inactive'); document.querySelector('#project-welcome').hidden=true; document.querySelector('#map-select').parentElement.hidden=false; document.querySelectorAll('.legacy-edit-control').forEach(b=>b.classList.remove('legacy-edit-control'));document.querySelector('.project-game-panel').classList.remove('project-game-panel')")
        self.page.select_option('#map-select', 'obj_test')
        self.page.wait_for_selector('#mouse-begin:not([disabled])')
        # Legacy interaction tests exercise controls independently of menu grouping.
        self.page.evaluate("document.querySelectorAll('[data-section-panel]').forEach(p=>p.hidden=false);document.querySelector('#diagnostics').open=true")

    def tearDown(self):
        self.context.close()
        self.tmp.cleanup()
        self.assertEqual(self.errors, [], 'uncaught browser error')

    def route(self, route):
        path = urlparse(route.request.url).path
        content_types = {'.js': 'text/javascript', '.css': 'text/css', '.html': 'text/html'}
        if path in ['/', '/index.html', '/app.js', '/flags.js', '/language.js', '/navbundle.js', '/botmenu.js', '/menu.js', '/style.css', '/project-ui.js', '/project-ui.css']:
            file = HERE / ('index.html' if path == '/' else path[1:])
            route.fulfill(body=file.read_bytes(), content_type=content_types[file.suffix]); return
        if path == '/bsp-viewer.js':
            route.fulfill(body=VIEWER, content_type='text/javascript'); return
        result = {}
        status = 200
        if path == '/api/state':
            result = dict(token='isolated-test-token', maps=['obj_test', 'obj_other'], selected_map='obj_test', game_mode='tce2', jobs=[], game={'state':'offline'})
        elif path == '/api/projects':
            result={'active':None,'projects':[],'catalog':[]}
        elif path == '/api/gamecontext':
            result={'map':'obj_test','gametype':5}
        elif path == '/api/botmenu':
            if route.request.method == 'POST':
                data=route.request.post_data_json; self.requests.append(data)
                result={'command':botmenu.command(data),'reply':'fixture response'}
            else: result=botmenu.catalog('tce2')
        elif path == '/api/navbundle':
            if route.request.method == 'POST':
                payload = route.request.post_data_json
                self.assertEqual(route.request.headers['x-navstudio-token'], 'isolated-test-token')
                result = self.bundles.save(payload['map'], payload['kind'], payload['text'], payload['sha256'])
            else:
                result = self.bundles.read('obj_test')
                result.update(names=self.bundles.names(), game_mode='tce2')
        elif path == '/api/scene':
            result = dict(audit={'unresolved_nodes':[{'source_id':2,'position':[310,220,0],'incoming':1,'outgoing':0}]}, navigation=copy.deepcopy(self.nav), status='candidate_not_native_validated', draft=self.store.view('obj_test', self.environment))
            result.update(copy.deepcopy(self.scene_extra))
        elif path == '/api/edit':
            payload = route.request.post_data_json
            self.requests.append(payload)
            self.assertEqual(route.request.headers['x-navstudio-token'], 'isolated-test-token')
            if self.fail_next:
                self.fail_next = False; result = dict(error='Simulierter Schreibfehler'); status = 409
            else:
                try:
                    if payload['op'] == 'begin': self.store.start('obj_test', self.nav, self.base)
                    elif payload['op'] == 'archive': self.store.archive('obj_test', payload['revision'])
                    elif payload['op'] == 'save': self.store.save('obj_test', payload['revision'], self.environment)
                    else: self.store.change('obj_test', payload, self.environment)
                    result = dict(draft=self.store.view('obj_test', self.environment), note='Testentwurf geÃ¤ndert')
                except (ValueError, RuntimeError) as error:
                    result = dict(error=str(error)); status = 409
        elif path == '/api/settings':
            if route.request.method == 'POST':
                self.requests.append(route.request.post_data_json)
                if self.settings_failure: result = dict(error='Spiel lÃ¤uft noch'); status = 409
            else:
                result = dict(instance_path='C:/Games/TCE2', game_executable='C:/Games/TCE2/ET.exe', maps_dirs=['C:/Maps'], rcon_port=27986, settings_file='test.json', busy=False, running=False)
        elif path == '/api/action':
            self.requests.append(route.request.post_data_json)
            self.assertEqual(route.request.headers['x-navstudio-token'], 'isolated-test-token')
            result = {'id': 'isolated-generate-job'}
        else:
            self.fail('Unexpected HTTP request: ' + route.request.url)
        route.fulfill(body=encoded_response(result), content_type='application/json', status=status)

    def begin(self):
        self.page.click('#mouse-begin')
        self.page.wait_for_selector('[data-edit-tool=add]:not([disabled])')

    def xy(self, x, y):
        box = self.page.locator('#viewer').bounding_box()
        return box['x']+x, box['y']+y

    def point(self, x, y=220):
        self.page.mouse.click(*self.xy(x, y))

    def revision(self, revision):
        self.page.wait_for_function('(r)=>document.querySelector("#edit-status").textContent.startsWith("Revision "+r+" ")', arg=revision)
        self.page.wait_for_selector('[data-edit-tool=select]:not([disabled])')

    def test_all_work_sections_remain_visible_without_tabs(self):
        self.assertEqual(self.page.locator('[data-section]').count(), 0)
        for selector in ['#map-select','#problems-panel','#mouse-begin']:
            self.assertTrue(self.page.locator(selector).is_visible())
        self.begin()
        self.assertTrue(self.page.locator('#apply-draft').is_visible())

    def test_prerequisite_dialog_cancel_then_archive_and_resume(self):
        self.begin()
        self.page.locator('[data-action="rebuild"]').click()
        self.assertTrue(self.page.locator('#prerequisite-dialog').is_visible())
        self.page.click('#prerequisite-cancel')
        self.assertIsNotNone(self.store.get('obj_test'))
        self.assertFalse(any(r.get('action') == 'rebuild' for r in self.requests))
        self.page.locator('[data-action="rebuild"]').click()
        self.page.click('#prerequisite-continue')
        self.page.wait_for_function('!document.querySelector("#prerequisite-dialog").open')
        self.page.wait_for_timeout(100)
        self.assertIsNone(self.store.get('obj_test'))
        self.assertEqual(sum(r.get('action') == 'rebuild' for r in self.requests), 1)
        self.assertTrue(list(self.store.directory.glob('*.archive.json')))

    def test_problem_camera_button_focuses_exact_report_position(self):
        self.page.locator('#problem-list button').click()
        self.assertEqual(self.page.evaluate('window.fixtureViewer.focusedTarget'), [310,220,0])
        self.assertIn('Kein Ausgang', self.page.locator('#problem-list').inner_text())
        self.assertIn('1 Punkte', self.page.locator('#problem-summary').inner_text())
        self.assertTrue(self.page.locator('[data-layer="rejected"]').is_checked())

    def current_doors(self):
        self.scene_extra=dict(status='native_checked_not_live_certified',audit_state='current',report_status={'current':True},
            native_summary={'door_generation':{'live_verified':False,'stop_reason':'door_scan_complete','cases':[
                dict(entity=96,use_entity=99,**{'from':2,'to':3},team=3,reason='closed_identity_open_pose_native_walk_live_pending'),
                dict(entity=96,use_entity=99,**{'from':3,'to':2},team=3,reason='closed_identity_open_pose_native_walk_live_pending'),
                dict(entity=101,use_entity=-1,**{'from':-1,'to':-1},team=0,reason='door_locked_or_key_required'),
                dict(entity=105,use_entity=-1,**{'from':1,'to':2},team=3,reason='unlinked_button'),
                dict(entity=106,use_entity=107,**{'from':1,'to':2},team=3,reason='door_timing_not_proven')]}})
        self.nav['nodes'][1].update(action_roles=[dict(kind='door',role='entry',entry_node=2,target_node=3,use_node=1)],
            action_properties={'tce_door_role':'2','tce_door_entity':'96','tce_door_use_entity':'99','tce_door_model':'3',
                               'tce_door_use_model':'4','tce_door_team':'3','tce_door_aim':'1034 264 56'})
        self.page.click('#reload-scene');self.page.wait_for_selector('#door-diagnostic-list .door-diagnostic-item')

    def test_current_door_cases_are_grouped_readable_and_translate(self):
        self.current_doors()
        self.assertEqual(self.page.locator('.door-diagnostic-item').count(),4)
        text=self.page.text_content('#door-diagnostic-list')
        self.assertIn('TÃ¼r verriegelt',text);self.assertIn('Schalterzuordnung fehlt',text);self.assertIn('Ã–ffnungszeit',text)
        self.assertIn('Botbedienung separat',self.page.text_content('#door-diagnostic-status'))
        self.assertTrue(self.page.locator('.door-diagnostic-item').nth(1).locator('button').is_disabled())
        self.page.locator('.door-diagnostic-item').first.locator('button').click()
        self.assertEqual(self.page.evaluate('fixtureViewer.focusedTarget'),[310,220,0])
        self.page.click('#language-toggle')
        self.page.wait_for_function("document.querySelector('#door-diagnostic-list').textContent.includes('Door is locked or requires a key.')")
        self.assertIn('Test bot activation separately',self.page.text_content('#door-diagnostic-status'))

    def test_stale_report_hides_old_door_cases(self):
        self.current_doors()
        self.scene_extra.update(status='native_stale',audit_state='stale',report_status={'current':False})
        self.page.click('#reload-scene');self.page.wait_for_function("document.querySelector('#door-diagnostic-status').textContent.includes('TÃ¼rprÃ¼fung veraltet')")
        self.assertEqual(self.page.locator('.door-diagnostic-item').count(),0)
        self.assertNotIn('TÃ¼r verriegelt',self.page.text_content('#door-diagnostics'))

    def current_authored_rejection(self):
        self.begin()
        self.nav['edges'][0].update(accepted=False,status='rejected',reason='blocked_hull',native_checked=True)
        self.scene_extra.update(status='native_failed',audit_state='current',report_status={'current':True},
            proof_status={'valid':False},native_summary={'generation_complete':False})
        self.page.click('#reload-scene')
        self.page.wait_for_function("document.querySelector('#stat-rejected').textContent==='1'")

    def test_open_checked_editor_preserves_directional_native_rejection(self):
        self.current_authored_rejection()
        before=self.page.evaluate('JSON.stringify(fixtureViewer._navigation)')
        self.page.uncheck('#edit-show');self.page.check('#edit-show')
        self.page.click('#edit-begin')
        self.assertEqual(self.page.text_content('#stat-rejected'),'1')
        self.assertEqual(self.page.text_content('#reason-count'),'1')
        self.assertEqual(self.page.evaluate('JSON.stringify(fixtureViewer._navigation)'),before)
        self.page.mouse.click(*self.xy(230,220))
        self.assertIn('ABGELEHNT',self.page.text_content('#selection-details'))
        self.assertIn('blocked_hull',self.page.text_content('#selection-description'))
        self.assertEqual(self.page.evaluate('fixtureViewer._navigation.edges[1].accepted'),None)
        self.assertEqual(self.page.text_content('#problem-count'),'1')

    def test_mutation_invalidates_results_and_sha_mismatch_never_claims_checked(self):
        self.current_authored_rejection()
        self.point(150);self.page.fill('#edit-radius','50');self.page.click('#edit-properties');self.revision(1)
        self.assertEqual(self.page.text_content('#stat-rejected'),'0')
        self.assertIn('PrÃ¼fung offen',self.page.text_content('#edit-status'))
        self.assertIn('erneute native PrÃ¼fung',self.page.text_content('#selection-description'))
        self.assertFalse(self.page.evaluate('navstudioClient.app.scene.report_status.current'))
        self.assertFalse(self.page.evaluate('navstudioClient.app.scene.proof_status.valid'))
        self.page.uncheck('#edit-show')
        self.assertIn('veraltet',self.page.text_content('#stat-proof'))
        self.assertFalse(self.page.evaluate('navstudioClient.app.scene.report_status.current'))
        self.page.evaluate("navstudioClient.app.draft.revision=navstudioClient.app.sourceScene.draft.revision;navstudioClient.app.draft.sha256='changed-sha-same-revision'")
        self.page.check('#edit-show')
        self.assertIn('PrÃ¼fung offen',self.page.text_content('#edit-status'))
        self.assertEqual(self.page.text_content('#stat-rejected'),'0')

    def generation_diagnostics(self,current=True):
        self.scene_extra['generation_diagnostics']=dict(current=current,native_report_sha256='a'*64,
            counts=dict(geometry_rejected_nodes=2,connectivity_pending_nodes=1,rejected_directed_edges=1),
            nodes=[dict(id=300,position=[50,60,70],accepted=False,floor_accepted=True,reason='quarantined_unreachable'),
                   dict(id=400,position=[10,20,30],accepted=False,floor_accepted=False,reason='blocked_hull'),
                   dict(id=401,position=[20,30,40],accepted=False,floor_accepted=False,reason='blocked_hull')],
            edges=[dict(**{'from':1,'to':400},reason='blocked_hull',accepted=False,from_position=[150,220,0])])
        self.page.click('#reload-scene')
        self.page.wait_for_selector('#coverage-list button')

    def test_scanner_diagnostics_keep_missing_floor_coverage_visible_and_focusable(self):
        self.current_authored_rejection();self.generation_diagnostics()
        self.assertEqual(self.page.text_content('#stat-rejected'),'1')
        self.assertEqual(self.page.text_content('#coverage-count'),'1')
        self.assertEqual(self.page.text_content('#problem-count'),'1')
        self.assertIn('ungeprÃ¼ft',self.page.text_content('#coverage-status'))
        self.page.locator('#coverage-list button').click()
        self.assertEqual(self.page.evaluate('fixtureViewer.focusedTarget'),[50,60,70])
        self.assertEqual(self.page.text_content('#scanner-count'),'3')
        self.assertFalse(self.page.evaluate("document.querySelector('#scanner-diagnostics').open"))
        self.page.locator('#scanner-diagnostics summary').click()
        self.assertEqual(self.page.locator('#scanner-reason-list button').count(),1)
        self.page.locator('#scanner-reason-list button').click()
        self.assertEqual(self.page.evaluate('fixtureViewer.focusedTarget'),[10,20,30])
        self.assertEqual(self.page.evaluate('fixtureViewer._navigation.nodes.length'),3)
        self.page.fill('#node-id','300');self.page.locator('#node-search button').click()
        self.assertEqual(self.page.evaluate('fixtureViewer.focusedTarget'),[50,60,70])
        self.assertIn('kein aktueller Wegpunkt',self.page.text_content('#node-search-status'))
        self.assertEqual(self.page.evaluate("getComputedStyle(document.querySelector('#coverage-list')).overflowY"),'visible')

    def test_historical_scanner_coverage_is_distinct_from_current_errors_and_translates(self):
        self.current_authored_rejection();self.generation_diagnostics(False)
        self.assertIn('Historischer',self.page.text_content('#coverage-status'))
        self.assertEqual(self.page.text_content('#problem-count'),'1')
        self.page.click('#language-toggle')
        self.page.wait_for_function("document.querySelector('#coverage-status').textContent.includes('Historical generation report')")
        self.assertIn('Ungrouped',self.page.text_content('#coverage-list'))
        self.assertIn('Rejected in graph',self.page.text_content('.stats-strip'))

    def test_cancel_checked_drag_never_mutates_bound_native_report(self):
        self.current_authored_rejection()
        before=self.page.evaluate('JSON.stringify(navstudioClient.app.sourceScene.navigation)')
        self.page.mouse.move(*self.xy(150,220));self.page.mouse.down();self.page.mouse.move(*self.xy(210,270),steps=5)
        self.page.keyboard.press('Escape');self.page.mouse.up()
        self.assertEqual(self.page.evaluate('JSON.stringify(navstudioClient.app.sourceScene.navigation)'),before)
        self.assertEqual(self.page.evaluate('fixtureViewer._navigationNodeMap[1].position'),[150,220,0])
        self.assertEqual(self.store.get('obj_test')['revision'],0)
        self.assertEqual(self.page.text_content('#stat-rejected'),'1')

    def test_thousands_of_floor_samples_render_regions_and_expand_only_on_demand(self):
        self.generation_diagnostics()
        data=self.scene_extra['generation_diagnostics']
        data['counts']['connectivity_pending_nodes']=5000
        data['nodes']=[dict(id=10000+i,position=[float(i),20,0],accepted=False,
            native_input=dict(floor_accepted=True,missing_region=i//1000),diagnostic_kind='missing_floor_area',
            reason='quarantined_no_validated_route_from_spawn') for i in range(5000)]
        data['nodes'].append(dict(id=999,position=[0,0,0],accepted=False,diagnostic_kind='missing_floor_area',
            diagnostic_resolved=True,native_input=dict(floor_accepted=True,missing_region=1)))
        self.page.click('#reload-scene')
        self.page.wait_for_function("document.querySelector('#coverage-count').textContent==='5.000'")
        self.assertEqual(self.page.locator('#coverage-list .coverage-region').count(),5)
        self.assertEqual(self.page.locator('#coverage-list button').count(),5)
        self.assertEqual(self.page.locator('#coverage-list details .reason-list').count(),0)
        self.assertEqual(self.page.text_content('#coverage-region-count'),'5')
        self.page.locator('#coverage-list details summary').first.click()
        self.page.wait_for_selector('#coverage-list details .reason-list button')
        self.assertEqual(self.page.locator('#coverage-list button').count(),1005)
        self.assertNotIn('Bodenprobe 999 Â·',self.page.text_content('#coverage-list'))

    def test_selected_door_recipe_has_models_activator_and_source_references(self):
        self.current_doors();self.point(310)
        details=self.page.text_content('#selection-details')
        for text in ('TÃ¼rmodell','*3','Aktivatormodell','*4','Eintrittspunkt','Austrittspunkt','Bedienpunkt','Ãœber zugeordneten Schalter'):
            self.assertIn(text,details)
        self.assertIn('Botbedienung separat testen',details)
        self.assertIn('1034.00 / 264.00 / 56.00',details)

    def test_live_goal_controls(self):
        self.page.click('#botmenu-open')
        self.page.wait_for_selector('#botmenu-type option[value="Attack"]', state='attached')
        self.page.locator('#botmenu-name').fill('test_corner')
        self.page.click('#botmenu-create')
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('bot goal_create Attack test_corner')")
        self.assertEqual(self.requests[-1]['map'], 'obj_test')
        self.page.get_by_role('button',name='Save Goals',exact=True).click()
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('bot goal_save')")
        self.page.click('#botmenu-close')

    def test_live_switch_properties_and_waypoint_name_use_typed_controls(self):
        self.page.click('#botmenu-open')
        self.page.wait_for_selector('#botmenu-property option[value="timeout"]',state='attached')
        self.page.locator('#botmenu-property').select_option('stance')
        self.page.locator('#botmenu-value-choices').select_option('crouch')
        self.page.click('#botmenu-set')
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('bot goal_setproperty stance crouch')")
        self.page.locator('#botmenu-property').select_option('timeout')
        self.page.locator('#botmenu-value').fill('1500')
        self.page.click('#botmenu-set')
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('bot goal_setproperty timeout 1500')")
        self.page.locator('#botmenu-property').select_option('use')
        self.page.locator('#botmenu-value-choices').select_option('0')
        self.page.click('#botmenu-set')
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('bot goal_setproperty use 0')")
        self.page.locator('#botmenu-waypoint-name').fill('Front Door Switch')
        self.page.get_by_role('button',name='Naechsten Punkt benennen / Name nearest point',exact=True).click()
        self.page.wait_for_function("document.querySelector('#botmenu-result').textContent.includes('bot waypoint_setname Front Door Switch')")
        self.page.click('#botmenu-close')

    def test_drag_is_one_revision_and_undo_redo(self):
        self.begin()
        self.page.mouse.move(*self.xy(150, 220)); self.page.mouse.down()
        self.page.mouse.move(*self.xy(210, 265), steps=8)
        self.assertEqual(len(self.requests), 1)
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['position'], [150, 220, 0])
        self.page.mouse.up(); self.revision(1)
        self.assertEqual([p['op'] for p in self.requests], ['begin', 'update_node'])
        self.assertEqual(self.requests[-1]['position'], [210, 265, 0.125])
        self.assertNotIn('radius', self.requests[-1])
        self.page.keyboard.press('Control+z'); self.revision(2)
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['position'], [150, 220, 0])
        self.page.keyboard.press('Control+Shift+z'); self.revision(3)
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['position'], [210, 265, 0.125])

    def test_standing_jump_target_is_outgoing_neighbour_and_delete_clears_recipe(self):
        self.begin(); self.point(150)
        self.assertEqual(self.page.locator('#edit-standing-jump option').evaluate_all('(options)=>options.map(o=>o.value)'), ['', '2'])
        self.page.select_option('#edit-standing-jump', '2'); self.page.click('#edit-properties'); self.revision(1)
        node=self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['action_roles'],[dict(kind='standing_jump',role='launch',landing_node=2)])
        self.assertEqual(node['action_properties']['tce_stand_jump'],'1')
        self.assertEqual(self.page.input_value('#edit-standing-jump'),'2')
        self.assertEqual(self.page.input_value('#edit-radius'),'2')
        self.assertEqual(self.page.get_attribute('#edit-radius','min'),'1')
        self.point(310); self.page.keyboard.press('Delete'); self.revision(2)
        node=self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['action_roles'],[])
        self.assertNotIn('tce_stand_jump',node['action_properties'])
        self.assertEqual(node['editor_role']&64,0)
        self.point(150)
        self.assertEqual(self.page.input_value('#edit-standing-jump'),'')
        self.assertEqual(self.page.locator('#edit-standing-jump option').count(),1)
        self.page.keyboard.press('Control+z'); self.revision(3)
        self.point(150)
        self.assertEqual(self.page.input_value('#edit-standing-jump'),'2')
        self.page.select_option('#edit-standing-jump',''); self.page.click('#edit-properties'); self.revision(4)
        self.assertIsNone(self.requests[-1]['standing_jump_target'])
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['action_roles'],[])
        self.page.click('[data-edit-tool=add]');self.point(250,340);self.revision(5)
        self.assertEqual(self.store.get('obj_test')['nodes'][-1]['radius'],35)

    def test_free_climb_selector_has_directed_target_preserves_recipe_and_undo(self):
        self.begin();self.point(150)
        self.assertEqual(self.page.locator('#edit-free-climb option').evaluate_all('(options)=>options.map(o=>o.value)'),['','2'])
        self.page.select_option('#edit-free-climb','2');self.page.click('#edit-properties');self.revision(1)
        node=self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['action_roles'],[dict(kind='free_climb',role='launch',landing_node=2)])
        self.assertEqual(node['nav_flags']&512,512)
        self.assertEqual(node['editor_role']&128,128)
        self.assertEqual(node['radius'],2)
        self.assertEqual(self.page.input_value('#edit-free-climb'),'2')
        self.assertNotIn('standing_jump_target',self.requests[-1])
        self.point(310);self.page.keyboard.press('Delete');self.revision(2)
        node=self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['action_roles'],[])
        self.assertEqual(node['nav_flags']&512,0)
        self.page.keyboard.press('Control+z');self.revision(3);self.point(150)
        self.assertEqual(self.page.input_value('#edit-free-climb'),'2')
        self.page.select_option('#edit-free-climb','');self.page.click('#edit-properties');self.revision(4)
        self.assertIsNone(self.requests[-1]['free_climb_target'])
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['action_roles'],[])

    def test_empty_action_selectors_preserve_ordinary_ladder_and_jump_flags(self):
        self.begin();self.point(150)
        for flag in ('CLIMB','JUMP'):
            self.page.check('#edit-flag-picker input[aria-label='+flag+']')
        self.page.fill('#edit-radius','80');self.page.click('#edit-properties');self.revision(1)
        node=self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['nav_flags']&(512|134217728),512|134217728)
        self.assertNotIn('free_climb_target',self.requests[-1])
        self.assertNotIn('standing_jump_target',self.requests[-1])
        self.page.evaluate("window.navstudioClient.app.state.game_mode='et';fixtureViewer.selectNavigation('node',1)")
        self.assertTrue(self.page.is_disabled('#edit-free-climb'))

    def test_escape_invalid_floor_and_error_restore_preview(self):
        self.begin()
        for finish in ['escape', 'invalid', 'error']:
            self.page.mouse.move(*self.xy(150, 220)); self.page.mouse.down()
            self.page.mouse.move(*self.xy(650 if finish == 'invalid' else 210, 270), steps=5)
            if finish == 'escape': self.page.keyboard.press('Escape')
            if finish == 'error': self.fail_next = True
            self.page.mouse.up()
            if finish == 'error': self.page.wait_for_selector('#notice:has-text("Simulierter Schreibfehler")')
            self.page.wait_for_function('fixtureViewer._navigationNodeMap[1].position[0] === 150')
            self.assertEqual(self.store.get('obj_test')['revision'], 0)
        self.assertEqual([p['op'] for p in self.requests], ['begin', 'update_node'])

    def test_add_connect_delete_restore_and_direction(self):
        self.begin(); self.page.click('[data-edit-tool=add]'); self.point(260, 340); self.revision(1)
        added = self.store.get('obj_test')['nodes'][-1]['position']
        self.assertAlmostEqual(added[0], 260, delta=1)
        self.assertAlmostEqual(added[1], 340, delta=1)
        self.assertEqual(added[2], .125)
        self.page.click('[data-edit-tool=link]'); self.point(150); self.point(260, 340); self.revision(2)
        self.assertTrue(self.requests[-1]['bidirectional'])
        self.page.click('[data-edit-tool=unlink]'); self.page.uncheck('#mouse-both'); self.point(150); self.point(260, 340); self.revision(3)
        self.assertIn({'from': 4, 'to': 1}, self.store.get('obj_test')['edges'])
        self.assertNotIn({'from': 1, 'to': 4}, self.store.get('obj_test')['edges'])
        self.page.click('[data-edit-tool=select]'); self.point(260, 340); self.page.keyboard.press('Delete'); self.revision(4)
        self.assertEqual(len(self.store.get('obj_test')['nodes']), 3)
        self.page.keyboard.press('Control+z'); self.revision(5)
        self.assertIn({'from': 4, 'to': 1}, self.store.get('obj_test')['edges'])

    def test_edge_click_unlinks_both(self):
        self.begin(); self.page.click('[data-edit-tool=unlink]'); self.point(230); self.revision(1)
        self.assertEqual(self.store.get('obj_test')['edges'], [])

    def test_action_points_editable_and_text_input_does_not_delete_points(self):
        self.nav['nodes'][2]['nav_flags']=512
        self.begin(); self.point(430)
        self.page.check('#edit-flag-picker input[aria-label=SPRINT]')
        self.page.click('#edit-properties'); self.revision(1)
        node=next(n for n in self.store.get('obj_test')['nodes'] if n['id']==3)
        self.assertEqual(node['nav_flags'], (1<<39)|512)
        self.page.fill('#edit-radius', '80'); self.page.keyboard.press('Delete'); self.page.keyboard.press('Control+z')
        self.assertEqual(self.store.get('obj_test')['revision'],1)

    def test_add_requires_floor_and_map_change_cancels_connection(self):
        self.begin(); self.page.click('[data-edit-tool=add]'); self.point(650)
        self.page.wait_for_selector('#notice:has-text("Kein Boden")')
        self.page.click('[data-edit-tool=link]'); self.point(150)
        self.page.select_option('#map-select', 'obj_other')
        self.page.wait_for_selector('#map-title:has-text("obj_other")')
        self.assertNotIn('Start: Punkt', self.page.inner_text('#mouse-help'))
        self.assertEqual(len(self.requests), 1)

    def test_radius_only_edit_preserves_exact_position(self):
        self.begin(); self.point(150)
        self.page.fill('#edit-radius', '80'); self.page.check('#edit-flag-picker input[aria-label=CROUCH]')
        self.page.click('#edit-properties'); self.revision(1)
        node = self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['position'], [150, 220, 0])
        self.assertEqual(node['radius'], 80)
        self.assertEqual(node['nav_flags'], 64)
        self.assertNotIn('position', self.requests[-1])

    def test_production_json_browser_save_roundtrip_keeps_unknown_high_flags(self):
        mask=(1<<60)|(1<<6)|1
        self.nav['nodes'][0].update(nav_flags=mask,crouch=True)
        self.page.select_option('#map-select','obj_other');self.page.select_option('#map-select','obj_test')
        self.page.wait_for_function("window.navstudioClient.app.scene?.navigation.nodes[0].nav_flags==='1152921504606847041' && !window.navstudioClient.app.loading")
        self.begin();self.point(150)
        self.assertEqual(self.page.input_value('#edit-flags'),str(mask))
        self.assertTrue(self.page.is_checked('#edit-flag-picker input[aria-label=CROUCH]'))
        self.page.uncheck('#edit-flag-picker input[aria-label=CROUCH]')
        self.page.click('#edit-properties');self.revision(1)
        self.assertEqual(self.requests[-1]['nav_flags'],str(mask&~64))
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['nav_flags'],mask&~64)
        self.store.save('obj_test',1,self.environment)
        self.assertEqual(Drafts(Path(self.tmp.name)).get('obj_test')['nodes'][0]['nav_flags'],mask&~64)

    def test_stale_draft_blocks_mouse_mutations(self):
        self.begin()
        self.store.get('obj_test')['base']['environment'] = {'changed':'yes'}
        self.page.click('#reload-scene')
        self.page.wait_for_selector('#mouse-help:has-text("Grundlage geÃ¤ndert")')
        self.assertTrue(self.page.is_disabled('[data-edit-tool=add]'))
        self.point(150); self.page.keyboard.press('Delete'); self.page.keyboard.press('Control+z')
        self.assertEqual(len(self.requests), 1)

    def test_settings_and_single_game_action(self):
        self.assertEqual(self.page.locator('[data-action=start_test]').count(), 0)
        self.assertEqual(self.page.locator('[data-action=load_map]').count(), 1)
        self.page.click('#settings-open')
        self.page.wait_for_selector('#settings-save:not([disabled])')
        self.assertEqual(self.page.input_value('#settings-executable'), 'C:/Games/TCE2/ET.exe')
        self.page.fill('#settings-maps', 'C:/Maps\n\n D:/Maps2 ')
        self.settings_failure = True
        self.page.click('#settings-save'); self.page.wait_for_selector('#settings-error:has-text("Spiel lÃ¤uft noch")')
        self.assertEqual(self.requests[-1]['maps_dirs'], ['C:/Maps', 'D:/Maps2'])
        self.assertTrue(self.page.locator('#settings-dialog').is_visible())

    def test_generate_submits_one_combined_action_without_install(self):
        self.page.click('[data-action=generate]')
        self.page.wait_for_selector('[data-action=generate]:not([disabled])')
        self.assertEqual(self.requests, [{'action': 'generate', 'map': 'obj_test', 'gametype': 2}])

    def test_camera_does_not_edit_and_toolbar_is_visible(self):
        self.begin()
        self.page.mouse.move(*self.xy(350, 260)); self.page.mouse.down(button='right')
        self.page.mouse.move(*self.xy(400, 290), steps=3); self.page.mouse.up(button='right')
        self.page.mouse.wheel(0, 100)
        self.page.wait_for_function('fixtureViewer.cameraChanges >= 4')
        self.assertEqual(len(self.requests), 1)
        self.assertTrue(self.page.is_visible('[data-edit-tool=unlink]'))
        target = HERE.parent / 'build/navgen-overkill/navstudio-ui-tests'
        target.mkdir(parents=True, exist_ok=True)
        self.page.screenshot(path=str(target / 'mouse-editor.png'), full_page=True)

    def test_language_button_persists_and_restores_editor_labels(self):
        self.page.locator('#language-toggle').click()
        self.page.wait_for_function("document.documentElement.lang === 'en'")
        self.assertEqual(self.page.locator('#language-toggle').inner_text(), 'Deutsch')
        self.assertEqual(self.page.evaluate("localStorage.getItem('navstudio-language')"), 'en')
        self.page.reload()
        self.page.wait_for_function("document.documentElement.lang === 'en'")
        self.page.locator('#language-toggle').click()
        self.page.wait_for_function("document.documentElement.lang === 'de'")
        self.assertEqual(self.page.locator('#language-toggle').inner_text(), 'English')

    def test_companion_script_editor_backup_and_dirty_guard(self):
        self.page.locator('#navbundle-open').click()
        self.page.wait_for_function("document.getElementById('navbundle-text').value.includes('global Map')")
        self.page.locator('#navbundle-text').fill('global Map = { Debug = true };\n')
        self.page.locator('#navbundle-close').click()
        self.assertTrue(self.page.locator('#navbundle-dialog').is_visible())
        self.page.locator('#navbundle-save').click()
        self.page.wait_for_function("document.getElementById('navbundle-status').textContent.includes('Backup:')")
        self.assertEqual((self.bundles.root/'obj_test.gm').read_text(), 'global Map = { Debug = true };\n')
        self.assertEqual((self.bundles.root/'obj_test.way').read_bytes(), b'original way')
        self.assertEqual((self.bundles.root/'obj_test_goals.gm').read_text(), 'global _MG = {};\n')
        self.page.locator('#navbundle-goals').click()
        self.assertIn('global _MG', self.page.locator('#navbundle-text').input_value())
        self.page.locator('#navbundle-close').click()
        self.assertFalse(self.page.locator('#navbundle-dialog').is_visible())

    def test_real_viewer_preview_pick_projection_and_floor_rejection(self):
        self.page.add_script_tag(content=(HERE / 'bsp-viewer.js').read_text(encoding='utf-8'))
        result = self.page.evaluate('''() => {
          const v=Object.create(window.BSPViewer.prototype), node={position:[0,0,0],id:1,_navId:1};
          v.canvas={getBoundingClientRect:()=>({left:10,top:20,width:400,height:300})};
          v._navigationMVP=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
          v._navigationNodeMap={1:node};v._navigation={nodes:[node],edges:[]};v._navigationOptions={nodes:true,minZ:-100,maxZ:100,style:'analysis',edges:'none'};
          v._cam={pos:[0,10,0],yaw:0,pitch:-0.5};v._rebuildNavigation=()=>{};
          const projected=v.projectNavigationPoint([0,0,0]), original=node.position;
          v.previewNavigationNode(1,[1,2,3]);
          const untouched=original.join() === '0,0,0', invalid=v.previewNavigationNode(1,[NaN,0,0]);
          v._cm={}; v._traceBox=()=>({fraction:.1,normal:[0,1,0]}); const floor=v.navigationFloorPoint(210,170);
          v._traceBox=()=>({fraction:.1,normal:[1,0,0]}); const wall=v.navigationFloorPoint(210,170);
          v._traceBox=()=>({fraction:1,normal:[0,1,0]}); const empty=v.navigationFloorPoint(210,170);
          v._navigationMVP=null;const hidden=v.projectNavigationPoint([0,0,0]);
          return {projected,untouched,invalid,floor,wall,empty,hidden};
        }''')
        self.assertEqual(result['projected'], [210, 170])
        self.assertTrue(result['untouched']); self.assertFalse(result['invalid'])
        self.assertEqual(len(result['floor']), 3)
        self.assertIsNone(result['wall']); self.assertIsNone(result['empty']); self.assertIsNone(result['hidden'])

    def test_real_recast_overlay_uses_detail_heights_and_legacy_fallback(self):
        self.page.add_script_tag(content=(HERE/'bsp-viewer.js').read_text(encoding='utf-8'))
        result=self.page.evaluate('''() => {
          const v=Object.create(window.BSPViewer.prototype),mesh={vertices:[[0,0,0],[100,0,0],[100,100,0],[0,100,0]]};
          const detail=[[[0,0,0],[100,0,0],[50,50,40]],[[100,100,0],[0,100,0],[50,50,40]]];
          return {detail:v.navigationPolygonTriangles(mesh,{vertices:[0,1,2,3],detail_triangles:detail}),
            legacy:v.navigationPolygonTriangles(mesh,{vertices:[0,1,2,3]}),
            invalid:v.navigationPolygonTriangles(mesh,{vertices:[0,1,2,3],detail_triangles:[[[0,0,0],[1,1,1],[NaN,0,0]]]} )};
        }''')
        self.assertEqual(result['detail'][0][2],[50,50,40]);self.assertEqual(len(result['legacy']),2)
        self.assertEqual(result['invalid'],[])

    def test_goals_are_loaded_as_real_saved_metadata_and_inspected_without_waypoint_edit(self):
        (self.bundles.root/'obj_test_goals.gm').write_text('global _MG={SWITCH_frontdoor={GoalType="SWITCH",Position=Vec3(1,2,3),Radius=25,Stance="stand",Timeout=1500,Use=1,},};')
        self.page.click('#reload-scene')
        self.page.wait_for_function('window.navstudioClient.app.goalOverlay?.goals.length===1')
        self.assertIn('1 Goals',self.page.text_content('#goal-layer-status'))
        self.page.evaluate('''()=>{const g=window.navstudioClient.app.goalOverlay.goals[0];window.fixtureViewer.onNavigationSelect({kind:'goal',id:g.id,item:g});}''')
        self.assertEqual(self.page.text_content('#selection-title'),'SWITCH_frontdoor')
        details=self.page.text_content('#selection-details')
        self.assertIn('1500',details);self.assertIn('Stance',details);self.assertIn('Use',details)
        self.assertNotIn('AKZEPTIERT',details)
        self.assertTrue(self.page.locator('#edit-properties').is_disabled())
        self.assertEqual(self.requests,[])

    def test_real_viewer_goal_label_radius_aim_and_selection_use_saved_properties(self):
        self.page.add_script_tag(content=(HERE/'bsp-viewer.js').read_text(encoding='utf8'))
        result=self.page.evaluate('''()=>{
          const v=Object.create(window.BSPViewer.prototype),buffers=[];
          v.gl={LINES:1,POINTS:2,TRIANGLES:3,deleteBuffer:()=>{}};v._navigationBuffers=[];
          v._navigationOptions={style:'ingame',edges:'none',nodes:false,goals:true,labels:true,accepted:true,rejected:false,minZ:-Infinity,maxZ:Infinity};
          v._navigation={origin_above_feet:30,nodes:[],edges:[],meshes:{}};v._navigationNodeMap={};
          v._navigationBuffer=function(values,p,a){if(values.length){buffers.push({values:values.slice()});this._navigationBuffers.push({});}};
          const goal={id:'SWITCH_frontdoor',name:'SWITCH_frontdoor',type:'SWITCH',position:[100,200,30],radius:25,
            properties:{Stance:'stand',Timeout:1500,Use:1,Facing:[.5,0,0],AimVectors:{0:[1,0,0]}}};
          v.setNavigationGoals({goals:[goal]});const shown=buffers.reduce((n,b)=>n+b.values.length,0);
          const colored=color=>buffers.flatMap(b=>{let points=[];for(let i=0;i<b.values.length;i+=6)if(color.every((v,j)=>b.values[i+3+j]===v))points.push(b.values.slice(i,i+3));return points;});
          const facingDrawn=colored([0,0,1]);
          buffers.length=0;v.setNavigationGoals({goals:[{...goal,properties:{...goal.properties,AimPosition:[300,400,75]}}]});
          const aimDrawn=colored([1,1,0]);
          const selected=v.selectNavigation('goal',goal.id),label=v.navigationLabelLines('goal',goal);
          const waypoint=v.navigationLabelLines('node',{id:34,way_guid:39,radius:25,name:'Front Door Switch',nav_flags:'134217728'});
          buffers.length=0;v._navigationOptions.goals=false;v._rebuildNavigation();
          const marker=v.navigationGoalMarkerPosition(goal);
          v._navigation.origin_above_feet=24;const etMarker=v.navigationGoalMarkerPosition(goal);
          const createdAtFeet=v.navigationGoalMarkerPosition({position:[100,200,0]});
          return {shown,hidden:buffers.length,facingDrawn,aimDrawn,selected,label,waypoint,marker,etMarker,createdAtFeet,position:goal.position};
        }''')
        self.assertGreater(result['shown'],100);self.assertEqual(result['hidden'],0)
        self.assertEqual(result['facingDrawn'],[[100,94,-200],[116,94,-200]])
        self.assertEqual(result['aimDrawn'],[[100,94,-200],[300,75,-400]])
        self.assertEqual(result['selected']['item']['radius'],25)
        self.assertIn('Timeout: 1500',result['label']);self.assertIn('Use: 1',result['label'])
        self.assertIn('Saved GM Â· Active: live runtime only',result['label'])
        self.assertEqual(result['waypoint'][0],'Source-ID 34, Radius 25, UID 39')
        self.assertIn('Flags: JUMP',result['waypoint'])
        self.assertEqual(result['marker'],[100,200,30]);self.assertEqual(result['etMarker'],[100,200,30])
        self.assertEqual(result['createdAtFeet'],[100,200,0]);self.assertEqual(result['position'],[100,200,30])

    def test_saved_switch_arrows_use_native_stance_absolute_aim_and_facing_priority(self):
        self.page.add_script_tag(content=(HERE/'bsp-viewer.js').read_text(encoding='utf8'))
        result=self.page.evaluate('''()=>{
          const v=Object.create(window.BSPViewer.prototype),goal={type:'SWITCH',position:[100,200,30],
            properties:{Stance:'crouch',Facing:[.5,0,0],AimPosition:[0,0,0]}};
          const before=JSON.stringify(goal),facing=v.navigationSavedGoalArrows(goal);
          const aim=v.navigationSavedGoalArrows({...goal,properties:{...goal.properties,AimPosition:[300,400,75]}});
          const prone=v.navigationSavedGoalArrows({...goal,properties:{...goal.properties,Stance:'prone'}});
          const standard=v.navigationSavedGoalArrows({...goal,properties:{Facing:[0,1,0]}});
          const zero=v.navigationSavedGoalArrows({...goal,properties:{Facing:[0,0,0],AimPosition:[0,0,0]}});
          const invalid=v.navigationSavedGoalArrows({...goal,properties:{Facing:[NaN,1,0]}});
          const other=v.navigationSavedGoalArrows({...goal,type:'CAMP'});
          const label=v.navigationLabelLines('node',{id:834,index:387,way_guid:388,radius:35});
          return {facing,aim,prone,standard,zero,invalid,other,label,unchanged:before===JSON.stringify(goal)};
        }''')
        self.assertEqual(result['facing'],[{'kind':'Facing','from':[100,200,70],'to':[116,200,70],'color':[0,0,1]}])
        self.assertEqual(result['aim'],[{'kind':'AimPosition','from':[100,200,70],'to':[300,400,75],'color':[1,1,0]}])
        self.assertEqual(result['prone'][0]['from'],[100,200,44]);self.assertEqual(result['standard'][0]['from'],[100,200,94])
        self.assertEqual(result['zero'],[]);self.assertEqual(result['invalid'],[]);self.assertEqual(result['other'],[])
        self.assertEqual(result['label'][0],'Waypoint 387 (native index), Source-ID 834, Radius 35, UID 388')
        self.assertTrue(result['unchanged'])

    def test_mouse_radius_control_offers_full_dms_range_and_keeps_action_limit(self):
        self.begin();self.point(150)
        self.assertEqual(self.page.get_attribute('#edit-radius','max'),'300')
        self.page.fill('#edit-radius','150');self.page.click('#edit-properties');self.revision(1)
        self.assertEqual(self.store.get('obj_test')['nodes'][0]['radius'],150)
        self.page.click('#edit-save');self.page.wait_for_function('window.navstudioClient.app.draft.saved_revision===1')
        self.assertEqual(Drafts(self.store.directory).get('obj_test')['nodes'][0]['radius'],150)
        self.page.evaluate('''()=>{window.fixtureViewer._navigationNodeMap[2].action_roles=[{kind:'door',role:'entry'}];window.fixtureViewer.selectNavigation('node',2);}''')
        self.assertEqual(self.page.get_attribute('#edit-radius','max'),'100')

    def test_switch_omitted_defaults_are_visible_in_inspector_and_real_viewer_labels(self):
        raw='global _MG={SWITCH_default={GoalType="SWITCH",Position=Vec3(1,2,3),Radius=35,Version=1,},};'
        path=self.bundles.root/'obj_test_goals.gm';path.write_text(raw)
        self.page.click('#reload-scene')
        self.page.wait_for_function('window.navstudioClient.app.goalOverlay?.goals.length===1')
        self.page.evaluate('''()=>{const g=window.navstudioClient.app.goalOverlay.goals[0];window.fixtureViewer.onNavigationSelect({kind:'goal',id:g.id,item:g});}''')
        details=self.page.text_content('#selection-details')
        self.assertIn('Stance Â· Schema default',details);self.assertIn('stand',details)
        self.assertIn('Timeout Â· Schema default',details);self.assertIn('1500',details)
        self.assertIn('Use Â· Schema default',details)
        self.page.add_script_tag(content=(HERE/'bsp-viewer.js').read_text(encoding='utf8'))
        label=self.page.evaluate('''()=>window.BSPViewer.prototype.navigationLabelLines('goal',window.navstudioClient.app.goalOverlay.goals[0])''')
        self.assertIn('Stance: stand (Schema default)',label)
        self.assertIn('Timeout: 1500 (Schema default)',label)
        self.assertIn('Use: 1 (Schema default)',label)
        self.assertIn('Saved GM Â· Active: live runtime only',label)
        self.assertEqual(path.read_text(),raw);self.assertEqual(self.requests,[])

    def test_name_and_facing_are_edited_with_properties_and_preserved_by_undo(self):
        self.begin();self.point(150)
        self.page.fill('#edit-name','Front Door Switch');self.page.fill('#edit-facing','0 1 0')
        self.page.click('#edit-properties');self.revision(1)
        node=self.store.get('obj_test')['nodes'][0]
        self.assertEqual(node['name'],'Front Door Switch');self.assertEqual(node['facing'],[0,1,0])
        self.assertIn('Front Door Switch',self.page.text_content('#selection-details'))
        self.page.click('#mouse-undo');self.revision(2);self.point(150)
        self.assertEqual(self.page.input_value('#edit-name'),'');self.assertEqual(self.page.input_value('#edit-facing'),'0 0 0')
        self.page.click('#mouse-redo');self.revision(3);self.point(150)
        self.assertEqual(self.page.input_value('#edit-name'),'Front Door Switch')

    def test_real_viewer_label_overlay_uses_projection_visibility_and_distance(self):
        self.page.add_script_tag(content=(HERE/'bsp-viewer.js').read_text(encoding='utf8'))
        result=self.page.evaluate('''()=>{
          const v=Object.create(window.BSPViewer.prototype),canvas=document.querySelector('#viewer');v.canvas=canvas;
          v._navigation={origin_above_feet:30};v._cam={pos:[0,0,0]};
          v._navigationMVP=[.001,0,0,0,0,.001,0,0,0,0,.001,0,0,0,0,1];
          v._navigationOptions={labels:true,goals:true,nodes:true,minZ:-100,maxZ:100,accepted:true,maxDistance:800};
          const goal={id:'SWITCH_real',name:'SWITCH_real',position:[100,100,30],properties:{Timeout:1500,Use:1}};
          v._navigationGoalOverlay={goals:[goal]};v._navigationSelection={kind:'goal',id:goal.id,item:goal};
          v._drawNavigationText();const shown=v._navigationTextLayer.textContent;
          const noInput=getComputedStyle(v._navigationTextLayer).pointerEvents;
          v._navigationOptions.maxDistance=50;v._drawNavigationText();const distanceHidden=v._navigationTextLayer.children.length;
          v._navigationOptions.maxDistance=800;v._cm={};v._traceBox=()=>({fraction:.1});
          v._drawNavigationText();const occluded=v._navigationTextLayer.children.length;
          v._navigationOptions.xray=true;v._drawNavigationText();const xray=v._navigationTextLayer.children.length;
          v._navigationOptions.labels=false;v._drawNavigationText();const disabled=v._navigationTextLayer.children.length;
          v._navigationTextLayer.remove();return {shown,noInput,distanceHidden,occluded,xray,disabled};
        }''')
        self.assertIn('SWITCH_real',result['shown']);self.assertIn('Timeout: 1500',result['shown'])
        self.assertEqual(result['noInput'],'none');self.assertEqual(result['distanceHidden'],0)
        self.assertEqual(result['occluded'],0);self.assertEqual(result['xray'],1);self.assertEqual(result['disabled'],0)

    def test_real_link_buffers_use_origin_height_and_edge_toggle(self):
        self.page.add_script_tag(content=(HERE/'bsp-viewer.js').read_text(encoding='utf8'))
        result=self.page.evaluate('''()=>{
          const v=Object.create(window.BSPViewer.prototype),buffers=[];
          v.gl={LINES:1,POINTS:2,TRIANGLES:3,deleteBuffer:()=>{}};v._navigationBuffers=[];
          v._navigationOptions={style:'ingame',edges:'all',nodes:false,accepted:true,rejected:false,minZ:-Infinity,maxZ:Infinity};
          v._navigation={origin_above_feet:30,nodes:[],edges:[{from:10,to:25,accepted:true}],meshes:{}};
          v._navigationNodeMap={'10':{id:10,position:[0,0,0]},'25':{id:25,position:[100,0,0]}};
          v._navigationBuffer=function(values,p,a){if(values.length){buffers.push({values:values.slice(),alpha:a});this._navigationBuffers.push({});}};
          v._rebuildNavigation();const first=buffers[0],lift=v.navigationEdgeLift(),outlined=v._navigationBuffers[0].graphEdges;
          buffers.length=0;v._navigationOptions.edges='selected';v._rebuildNavigation();const selected=buffers.length;
          v._navigationOptions.edges='none';v._rebuildNavigation();
          return {lift,first,outlined,selected,hidden:buffers.length};
        }''')
        self.assertEqual(result['lift'],38);self.assertEqual(result['first']['values'][1],38)
        self.assertEqual(result['first']['alpha'],1);self.assertTrue(result['outlined'])
        self.assertEqual(result['selected'],0);self.assertEqual(result['hidden'],0)


if __name__ == '__main__':
    unittest.main()
