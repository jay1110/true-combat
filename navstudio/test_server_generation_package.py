"""Offline completion/export contracts with temporary files and native stubs.

No game/API is contacted. Native spawn replies are explicit fixture responses;
these tests do not certify a real map or real spawn.
"""
import copy
import hashlib
from io import BytesIO
import json
from pathlib import Path
import tempfile
import threading
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import Mock,patch
import zipfile

import goalgen
from navbundle import NavBundles
from server import Studio,MOVEMENT_PROFILES
import spawns


def digest(path):
    path=Path(path)
    return hashlib.sha256(path.read_bytes()).hexdigest()if path.is_file()else None


class GenerationPackageTests(TestCase):
    def fixture(self,gametypes=(2,5,7),positions=None,accepted=None):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        root=Path(temp.name);project=root/'project';assets=root/'assets'
        for folder in(project/'work',project/'native',project/'bundles',assets/'maps'):folder.mkdir(parents=True)
        name='obj_fixture'
        for gt in gametypes:(assets/'maps'/(name+'_gt'+str(gt)+'.script')).write_text('game_manager { spawn { wm_announce "Keep GT%d" } }'%gt)
        positions=positions or [[i*256.,0.,30.]for i in range(8)]
        nodes=[dict(id=i,position=copy.deepcopy(p),raw_position=copy.deepcopy(p),accepted=True,spawn_reachable=True,
            nav_flags=0,crouch=False,radius=35,editor_role=0)for i,p in enumerate(positions)]
        report=dict(map=name,generation_complete=True,bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),nodes=nodes,
            edges=[dict(**{'from':i,'to':j},accepted=True)for i in range(len(nodes))for j in(i-1,i+1)if 0<=j<len(nodes)])
        report_path=project/'native'/(name+'_navmesh.json');report_path.write_text(json.dumps(report));report_path.with_suffix('.way').write_bytes(b'fixture native WAY')
        studio=Studio.__new__(Studio);studio.game_mode='tce2';studio.assets=SimpleNamespace(roots=[assets])
        studio.test=root/'existing-instance';studio.mod=studio.test/'tce2';studio.work=project/'work'
        studio.active_project=dict(id='fixture-project',name='Fixture',map=name,mod='tce2',gametype=gametypes[0]if gametypes else 2,
            paths=dict(root=str(project),work=str(project/'work'),native=str(project/'native'),bundles=str(project/'bundles'),spawns=str(project/'spawns.json')))
        studio.lock=threading.RLock();studio.busy=False;studio.file_hash=digest
        studio.native=lambda name:report;studio.native_path=lambda name:report_path;studio.require_map=lambda name:name
        current=dict(valid=True,inputs=dict(candidate='fixture-candidate',game_module='fixture-module'))
        studio.proof_status=Mock(side_effect=lambda name:copy.deepcopy(current));studio.script_directory=Mock(return_value='mapscripts')
        studio.update=lambda job,**values:job.update(values)
        events=[]
        def load(job):
            self.assertNotIn('quiet_generation',job)
            events.append(('load',job['gametype']))
        def quiet(job):
            self.assertNotIn('quiet_generation',job)
            job['quiet_generation']=dict(token='fixture');events.append(('quiet',job['gametype']))
        def restore(job):
            job.pop('quiet_generation',None);events.append(('restore',job['gametype']))
        studio.action_load_map=Mock(side_effect=load);studio.quiet_generation=Mock(side_effect=quiet);studio.restore_generation_bots=Mock(side_effect=restore)
        studio.require_bbox=Mock(return_value=copy.deepcopy(MOVEMENT_PROFILES[1]))
        studio.reconcile_objective_anchors=Mock(return_value=dict(certificate=None))
        studio.stationary_goal_support=Mock(side_effect=AssertionError('Only the explicit goal generation stub may be called'))
        allowed=set(range(len(nodes)))if accepted is None else set(accepted)
        def reply(command,read_only=False):
            self.assertTrue(read_only);self.assertTrue(command.startswith('tce_navgen_spawncheck '))
            position=list(map(float,command.split()[1:]));matches=[n for n in nodes if n['position']==position]
            self.assertEqual(len(matches),1,'No manual coordinate fixes or invented candidates')
            success=matches[0]['id']in allowed
            check=dict(accepted=success,reason='accepted'if success else'occupied_fixture',map=name,
                origin=position,feet=[*position[:2],position[2]-30],origin_above_feet=30,traces=1)
            return 'navgen spawncheck: '+json.dumps(check)
        studio.rcon=Mock(side_effect=reply)
        def goals(job):
            events.append(('goals',job['gametype']))
            requested=tuple(gt for gt in(2,5)if gt in studio.available_game_types(name)['available'])
            def objectives(assets,name,gt):
                p=nodes[1]['position'];x,y,z=p;bounds=[[x-40,y-40,z-38],[x+120,y+40,z+50]]
                mission=dict(tag='fixture_site',entity_id=20,position=p,aim_position=[x+80,y,z],bounds=bounds,
                    owner=1,target='fixture',target_class='trigger_flagonly_multiple'if gt==2 else'func_explosive',
                    operation='capture'if gt==2 else'plant_defuse',capture_teams=4,capture_flag_mask=1,
                    target_bounds=[[x+24,y-16,z-30],[x+42,y+16,z+46]]if gt==5 else None,contact_top=z+50)
                return ([dict(tag='fixture_flag',entity_id=19,position=nodes[0]['position'],aim_position=nodes[0]['position'],
                    bounds=None,owner=2,stealing_team=1,operation='take_flag',target='flag',target_class='team_CTF_blueflag'),mission]if gt==2 else[mission])
            with patch('goalgen.objectives',side_effect=objectives):
                result=goalgen.generate(studio,name,requested,map_script_directory='mapscripts')
            result['objective_anchor_certificate']=None;return result
        studio.action_generate_goals=Mock(side_effect=goals)
        studio.nav_bundles=lambda gt:NavBundles(project/'bundles',gt)
        job=dict(map=name,gametype=gametypes[0]if gametypes else 2,action='generate',log='')
        return SimpleNamespace(studio=studio,root=root,project=project,assets=assets,name=name,job=job,
            report=report,current=current,events=events,allowed=allowed)

    def complete(self,f):return f.studio.complete_generated_project(f.job)

    def test_completed_project_contains_only_real_gt_pairs_and_project_local_checked_spawns(self):
        f=self.fixture();before=json.dumps(f.report,sort_keys=True);result=self.complete(f)
        self.assertTrue(result['package_status']['ready']);self.assertEqual(result['generated_gametypes'],[2,5])
        self.assertGreaterEqual(result['spawns']['count'],4);self.assertEqual(f.job['gametype'],2)
        self.assertNotIn('quiet_generation',f.job);self.assertFalse(f.studio.test.exists())
        self.assertEqual(json.dumps(f.report,sort_keys=True),before)
        receipt=result['package_status']['receipt'];self.assertFalse(receipt['bot_live_tested'])
        self.assertTrue(Path(result['spawns']['path']).is_relative_to(f.project))
        self.assertFalse((f.project/'bundles'/(f.name+'_gt7.gm')).exists())

    def test_gt2_only_without_gt7_never_enters_spawn_stage_or_writes_gt5_phantom_files(self):
        f=self.fixture((2,));result=self.complete(f)
        self.assertTrue(result['package_status']['ready']);self.assertIsNone(result['spawns'])
        self.assertEqual(result['generated_gametypes'],[2]);f.studio.rcon.assert_not_called()
        f.studio.action_load_map.assert_not_called();self.assertFalse((f.project/'spawns.json').exists())
        self.assertEqual(sorted(p.name for p in(f.project/'bundles').iterdir()),[f.name+'_gt2.gm',f.name+'_gt2_goals.gm'])

    def test_missing_selected_goal_gametype_blocks_before_snapshot_stationary_or_spawn_access(self):
        f=self.fixture((5,));f.job['gametype']=2
        with self.assertRaisesRegex(ValueError,'kein unterstütztes GT2-/GT5-Mapscript'):
            Studio.action_generate_goals(f.studio,f.job)
        f.studio.reconcile_objective_anchors.assert_not_called();f.studio.stationary_goal_support.assert_not_called();f.studio.rcon.assert_not_called()
        self.assertFalse(list((f.project/'bundles').iterdir()))

    def test_gt7_rejected_spawn_uses_another_existing_native_node_without_coordinate_edits(self):
        f=self.fixture((7,),accepted=set(range(1,8)));before=copy.deepcopy(f.report);result=self.complete(f)
        attempts=result['spawns']['attempts'];self.assertEqual(attempts[0]['source_id'],0);self.assertFalse(attempts[0]['check']['accepted'])
        doc=spawns.SpawnStore(f.project/'spawns.json',f.name).read()
        self.assertNotIn(0,[n['source_node']for n in doc['nodes']]);self.assertGreaterEqual(len(doc['nodes']),4)
        by_id={n['id']:n for n in f.report['nodes']}
        self.assertTrue(all(n['position']==by_id[n['source_node']]['position']for n in doc['nodes']))
        self.assertEqual(f.report,before);self.assertTrue(result['package_status']['ready'])

    def test_zero_to_three_native_accepted_spawns_never_produce_ready_receipt_or_spawn_store(self):
        for count in range(4):
            f=self.fixture((7,),accepted=set(range(count)))
            with self.subTest(count=count),self.assertRaisesRegex(RuntimeError,'Keine vier nativ freien'):
                self.complete(f)
            self.assertFalse((f.project/'spawns.json').exists());self.assertFalse((f.project/'work'/(f.name+'.generation-ready.json')).exists())
            self.assertFalse(f.studio.package_status(f.name)['ready']);self.assertNotIn('quiet_generation',f.job)

    def test_no_exact_script_cannot_generate_goals_or_spawns_through_direct_goal_action(self):
        f=self.fixture(())
        (f.assets/'maps'/(f.name+'.script')).write_text('game_manager { spawn { } }')
        with self.assertRaisesRegex(ValueError,'kein unterstütztes GT2-/GT5-Mapscript'):
            Studio.action_generate_goals(f.studio,f.job)
        f.studio.reconcile_objective_anchors.assert_not_called();f.studio.rcon.assert_not_called()

    def test_missing_or_wrong_project_ready_receipt_stays_not_ready(self):
        f=self.fixture((2,));self.assertFalse(f.studio.package_status(f.name)['ready'])
        self.complete(f);path=f.project/'work'/(f.name+'.generation-ready.json');receipt=json.loads(path.read_bytes())
        receipt['project']='other-project';path.write_text(json.dumps(receipt))
        self.assertFalse(f.studio.package_status(f.name)['ready'])

    def test_graph_script_goal_and_spawn_changes_each_invalidate_ready_status(self):
        for mode in('proof','native','way','script','goal_manifest','goal_bytes','spawn_store','spawn_script'):
            f=self.fixture();self.complete(f)
            if mode=='proof':f.current['inputs']['candidate']='changed'
            else:
                path={'native':f.studio.native_path(f.name),'way':f.studio.native_path(f.name).with_suffix('.way'),
                    'script':f.assets/'maps'/(f.name+'_gt5.script'),'goal_manifest':f.project/'work/generated-goals.json',
                    'goal_bytes':f.project/'bundles'/(f.name+'_gt5_goals.gm'),'spawn_store':f.project/'spawns.json',
                    'spawn_script':f.project/'mapscripts'/(f.name+'_gt7.script')}[mode]
                if mode=='goal_manifest':
                    doc=json.loads(path.read_bytes());doc['changed']=True;path.write_text(json.dumps(doc))
                else:path.write_bytes(path.read_bytes()+b'\n// changed')
            with self.subTest(mode=mode):self.assertFalse(f.studio.package_status(f.name)['ready'])

    def test_receipt_cannot_skip_available_goal_types_by_claiming_empty_generated_scope(self):
        f=self.fixture((2,));self.complete(f);path=f.project/'work'/(f.name+'.generation-ready.json')
        receipt=json.loads(path.read_bytes());receipt['generated_gametypes']=[];path.write_text(json.dumps(receipt))
        self.assertFalse(f.studio.package_status(f.name)['ready'])

    def test_export_has_current_way_goal_pairs_gt_scripts_and_no_live_claim(self):
        f=self.fixture();self.complete(f);raw=f.studio.export_project_package(f.name)
        with zipfile.ZipFile(BytesIO(raw))as z:
            self.assertIsNone(z.testzip());names=set(z.namelist())
            self.assertIn('nav/'+f.name+'.way',names)
            for gt in(2,5):
                for suffix in('.gm','_goals.gm'):self.assertEqual(z.read('nav/'+f.name+'_gt'+str(gt)+suffix),(f.project/'bundles'/(f.name+'_gt'+str(gt)+suffix)).read_bytes())
            for gt in(2,5,7):self.assertIn('mapscripts/'+f.name+'_gt'+str(gt)+'.script',names)
            self.assertNotIn('nav/'+f.name+'_gt7.gm',names)
            metadata=json.loads(z.read('manifest.json'))['metadata'];self.assertFalse(metadata['bot_live_tested'])
        self.assertFalse(f.studio.test.exists())

    def test_gt7_only_ready_project_can_export_without_inventing_goal_pairs(self):
        f=self.fixture((7,));self.complete(f);raw=f.studio.export_project_package(f.name)
        with zipfile.ZipFile(BytesIO(raw))as z:
            self.assertIn('nav/'+f.name+'.way',z.namelist());self.assertIn('mapscripts/'+f.name+'_gt7.script',z.namelist())
            self.assertFalse(any(n.endswith('.gm')for n in z.namelist()))

    def test_export_is_refused_if_source_changes_between_two_status_reads(self):
        f=self.fixture((2,));self.complete(f);original=f.studio.package_status;calls=[]
        def check(name):
            calls.append(name)
            if len(calls)==2:
                path=f.project/'bundles'/(f.name+'_gt2.gm');path.write_bytes(path.read_bytes()+b'\n// changed while exporting')
            return original(name)
        f.studio.package_status=Mock(side_effect=check)
        with self.assertRaisesRegex(RuntimeError,'Projekt während ZIP-Erstellung verändert'):f.studio.export_project_package(f.name)

    def test_three_generic_candidate_pools_choose_same_native_sources_on_fresh_and_regeneration(self):
        pools=([[i*256.,0.,30.]for i in range(8)],
            [[(i%4)*384.,(i//4)*384.,30.]for i in range(8)],
            [[i*256.,64.*(i%2),30.+32.*(i//4)]for i in range(8)])
        for index,positions in enumerate(pools):
            f=self.fixture((7,),positions=positions);before=copy.deepcopy(f.report)
            first=self.complete(f);first_doc=spawns.SpawnStore(f.project/'spawns.json',f.name).read()
            second=self.complete(f);second_doc=spawns.SpawnStore(f.project/'spawns.json',f.name).read()
            with self.subTest(pool=index):
                self.assertEqual([n['source_node']for n in first_doc['nodes']],[n['source_node']for n in second_doc['nodes']])
                self.assertEqual([n['position']for n in first_doc['nodes']],[n['position']for n in second_doc['nodes']])
                self.assertGreater(second_doc['revision'],first_doc['revision']);self.assertEqual(f.report,before)
                self.assertTrue(first['package_status']['ready']);self.assertTrue(second['package_status']['ready'])


    def test_spawn_echo_all_accepted_wrong_map_origin_feet_body_or_traces_cannot_complete(self):
        for mode in('map','origin','feet','body','traces'):
            f=self.fixture((7,));original=f.studio.rcon.side_effect
            def altered(command,read_only=False):
                raw=original(command,read_only=read_only)
                check=json.loads(raw.split('navgen spawncheck: ',1)[1])
                self.assertIs(check['accepted'],True)
                if mode=='map':check['map']='obj_other'
                elif mode=='origin':check['origin'][0]+=.001
                elif mode=='feet':check['feet'][2]+=2
                elif mode=='body':check['origin_above_feet']=24
                else:check['traces']=0
                return 'navgen spawncheck: '+json.dumps(check)
            f.studio.rcon.side_effect=altered
            with self.subTest(mode=mode),self.assertRaisesRegex(RuntimeError,'Keine vier nativ freien'):
                self.complete(f)
            self.assertFalse((f.project/'spawns.json').exists())
            self.assertFalse((f.project/'work'/(f.name+'.generation-ready.json')).exists())
            self.assertFalse(f.studio.package_status(f.name)['ready'])
            self.assertNotIn('quiet_generation',f.job)

    def test_spawn_echo_invalid_accepted_node_uses_another_native_source_unchanged(self):
        f=self.fixture((7,));original=f.studio.rcon.side_effect;before=copy.deepcopy(f.report)
        def altered(command,read_only=False):
            raw=original(command,read_only=read_only)
            check=json.loads(raw.split('navgen spawncheck: ',1)[1])
            if check['origin']==f.report['nodes'][0]['position']:check['origin'][0]+=.001
            return 'navgen spawncheck: '+json.dumps(check)
        f.studio.rcon.side_effect=altered
        result=self.complete(f)
        doc=spawns.SpawnStore(f.project/'spawns.json',f.name).read()
        self.assertGreaterEqual(len(doc['nodes']),4)
        self.assertNotIn(f.report['nodes'][0]['id'],[n['source_node']for n in doc['nodes']])
        by_id={n['id']:n for n in f.report['nodes']}
        self.assertTrue(all(n['position']==by_id[n['source_node']]['position']for n in doc['nodes']))
        self.assertTrue(all(n['native_check']['origin']==n['position']for n in doc['nodes']))
        self.assertEqual(f.report,before);self.assertTrue(result['package_status']['ready'])


    def test_actual_server_zip_preserves_gt2_utf8_bom_and_gt5_legacy_crlf_source_bytes(self):
        f=self.fixture((2,5))
        originals={2:b'\xef\xbb\xbf'+('// 日本語 Grüße\r\ngame_manager { spawn { wm_announce "Keep" } }\r\n').encode('utf8'),
            5:b'// caf\xe9 \x81\r\ngame_manager { spawn { wm_announce "Keep GT5" } }\r\n'}
        for gt,raw in originals.items():(f.assets/'maps'/(f.name+'_gt'+str(gt)+'.script')).write_bytes(raw)
        self.complete(f);archive=f.studio.export_project_package(f.name)
        with zipfile.ZipFile(BytesIO(archive))as z:
            for gt,raw in originals.items():
                self.assertEqual(z.read('mapscripts/'+f.name+'_gt'+str(gt)+'.script'),raw)
                self.assertEqual((f.assets/'maps'/(f.name+'_gt'+str(gt)+'.script')).read_bytes(),raw)
            manifest=json.loads(z.read('manifest.json'))
            for gt,raw in originals.items():
                record=manifest['files']['mapscripts/'+f.name+'_gt'+str(gt)+'.script']
                self.assertEqual(record,dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))

    def test_actual_completion_and_zip_preserve_gt7_source_codec_bom_and_unmanaged_crlf_bytes(self):
        for codec,bom,label in(('utf-8',b'','日本語'),('utf-8',b'\xef\xbb\xbf','日本語'),('cp1252',b'','Grüße')):
            f=self.fixture();prefix=('// '+label+'\r\ngame_manager\r\n{\r\n spawn\r\n {').encode(codec)
            suffix=('\r\n  wm_announce "Keep '+label+'"\r\n }\r\n}\r\n').encode(codec)
            raw=bom+prefix+suffix;source=f.assets/'maps'/(f.name+'_gt7.script');source.write_bytes(raw)
            result=self.complete(f);generated=Path(result['spawns']['path']).read_bytes()
            with self.subTest(codec=codec,bom=bom):
                self.assertTrue(generated.startswith(bom+prefix));self.assertTrue(generated.endswith(suffix))
                self.assertEqual(source.read_bytes(),raw);self.assertEqual(generated.count(spawns.START.encode()),1)
                if bom:self.assertEqual(generated.count(bom),1)
                with zipfile.ZipFile(BytesIO(f.studio.export_project_package(f.name)))as z:
                    self.assertEqual(z.read('mapscripts/'+f.name+'_gt7.script'),generated)


class ConnectivityCompletionTests(TestCase):
    fixture = GenerationPackageTests.fixture
    complete = GenerationPackageTests.complete
    # Use the actual completion/export coordinator and its physical native
    # fixture. All nodes are spawn_reachable even in the disconnected case,
    # reproducing the multiple-spawn-region bug rather than trusting that flag.
    def split(self, f, direction=None):
        f.report['edges']=[e for e in f.report['edges'] if {e['from'],e['to']}!={5,6}]
        if direction:
            f.report['edges'].append(dict(**{'from':direction[0],'to':direction[1]},accepted=True,reason='external_native_walkdrop'))

    def test_generation_stops_before_goals_or_spawns_for_separate_spawn_region(self):
        f=self.fixture();self.split(f)
        with self.assertRaisesRegex(RuntimeError,'Source-IDs: 6, 7'):
            self.complete(f)
        f.studio.action_generate_goals.assert_not_called();f.studio.rcon.assert_not_called()
        self.assertFalse((f.project/'work'/(f.name+'.generation-ready.json')).exists())

    def test_one_way_physical_drop_does_not_invent_return_access(self):
        for direction in ((5,6),(6,5)):
            f=self.fixture();self.split(f,direction)
            with self.subTest(direction=direction),self.assertRaisesRegex(RuntimeError,'einseitig'):
                self.complete(f)
            f.studio.action_generate_goals.assert_not_called()

    def test_export_rechecks_connectivity_even_if_cached_receipt_and_proof_are_valid(self):
        f=self.fixture();self.complete(f);self.split(f)
        # Deliberately keep the fake current proof and the old report file
        # unchanged: topology has its own current-native gate.
        self.assertFalse(f.studio.package_status(f.name)['ready'])
        with self.assertRaisesRegex(RuntimeError,'ZIP noch nicht bereit'):
            f.studio.export_project_package(f.name)

    def test_repaired_native_edges_unlock_completion_without_changing_nodes(self):
        f=self.fixture();before=copy.deepcopy(f.report['nodes']);self.split(f)
        with self.assertRaises(RuntimeError):self.complete(f)
        f.report['edges'] += [dict(**{'from':5,'to':6},accepted=True,reason='external_native_climb'),
                              dict(**{'from':6,'to':5},accepted=True,reason='external_native_walkdrop')]
        result=self.complete(f)
        self.assertTrue(result['package_status']['ready']);self.assertEqual(f.report['nodes'],before)
        self.assertEqual(result['package_status']['receipt']['connectivity']['component_sizes'],[8])
        self.assertFalse(result['package_status']['receipt']['bot_live_tested'])


class NativeSpawnDocumentTests(TestCase):
    def report(self):
        return dict(map='obj_fixture',generation_complete=True,bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),nodes=[dict(id=i,position=[i*256.,0.,30.],accepted=True,spawn_reachable=True,crouch=False,nav_flags=0,editor_role=0)for i in range(8)],
            edges=[dict(**{'from':i,'to':j},accepted=True)for i in range(8)for j in(i-1,i+1)if 0<=j<8])

    def test_candidate_pool_requires_ordinary_two_way_native_support_and_preserves_source(self):
        report=self.report();report['nodes'][0]['accepted']=False;report['nodes'][2]['nav_flags']=1<<9;report['nodes'][4]['crouch']=True
        report['nodes'][6]['editor_role']=1;before=copy.deepcopy(report)
        candidates=spawns.spawn_candidates(report)
        self.assertEqual([n['id']for n in candidates],[]);self.assertEqual(report,before)
        report=self.report();report['edges']=[e for e in report['edges']if(e['from'],e['to'])!=(1,0)]
        self.assertNotIn(0,[n['id']for n in spawns.spawn_candidates(report)])

    def test_checked_document_rejects_unchecked_wrong_map_and_changed_candidate_coordinates(self):
        report=self.report();nodes=spawns.spawn_candidates(report)[:4];previous=dict(map='obj_fixture',revision=3,next_id=8,enabled=True)
        attempts=[dict(source_id=n['id'],check=dict(accepted=True,map='obj_fixture',origin=list(n['position']),
            feet=[*n['position'][:2],n['position'][2]-30],origin_above_feet=30,traces=1))for n in nodes]
        doc=spawns.checked_document('obj_fixture',previous,nodes,report,attempts)
        self.assertEqual(doc['revision'],4);self.assertEqual([n['position']for n in doc['nodes']],[n['position']for n in nodes])
        for mode in('missing','wrong_map','moved'):
            use=copy.deepcopy(nodes);checks=copy.deepcopy(attempts)
            if mode=='missing':checks.pop()
            elif mode=='wrong_map':checks[0]['check']['map']='another'
            else:use[0]['position'][0]+=1
            with self.subTest(mode=mode),self.assertRaisesRegex(ValueError,'aktuellen nativen Prüfungen'):
                spawns.checked_document('obj_fixture',previous,use,report,checks)


if __name__=='__main__':main()
