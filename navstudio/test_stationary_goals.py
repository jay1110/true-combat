"""Stationary goals require more than successful transit-node import.

No game or process is controlled. The Snow regression uses its archived real
reply; synthetic replies exercise binding, shared budget and workflow guards.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import Mock,patch

from goalgen import StationarySupport,stationary_binding,make_plan,generate,TACTICAL_TIMES,STATIONARY_PROBE_LIMIT
from server import Studio,native_body_profile


def report(count=40):
    nodes=[dict(id=i,position=[i*400.,0.,30.],accepted=True,spawn_reachable=True,nav_flags=0,
                radius=35,crouch=False)for i in range(count)]
    edges=[dict(**{'from':i,'to':j},accepted=True)for i in range(count)for j in (i-1,i+1)if 0<=j<count]
    return dict(generation_complete=True,bbox=dict(half_width=16.,stand_height=87.,crouch_height=61.,origin_above_feet=30.),nodes=nodes,edges=edges)


def script_assets(root):
    directory=root/'fixture-assets/maps';directory.mkdir(parents=True,exist_ok=True)
    for gt in(2,5):(directory/('obj_test_gt'+str(gt)+'.script')).write_text('game_manager { spawn { } }')
    return SimpleNamespace(roots=[directory.parent])


def reply(start,end,stand='accepted',crouch='accepted',skin=0):
    vec=lambda v:'('+' '.join('%.5f'%x for x in v)+')'
    text=f'navgen probe -1->-1 feet={vec(start)}->{vec(end)} stand={stand} crouch={crouch} traces=18 quarantined=0/0 (diagnostic only; graph unchanged)\n'
    for endpoint,feet in enumerate((start,end)):
        floor=list(feet);floor[2]+=skin
        for duck in (0,1):
            text+=f'navgen probe import endpoint={endpoint} crouch={duck} accepted=1 exact={int(not skin)} reason=accepted feet={vec(floor)}\n'
    return text


def objective(operation='plant_defuse'):
    return dict(tag='site',entity_id=42,position=[8000,0,30],aim_position=[8000,0,64],
                bounds=[[7980,-32,-8],[8020,32,80]],owner=1,operation=operation,capture_teams=4)


def counts(plan):
    return {kind:sum(g['type']==kind for g in plan['goals'])for kind in TACTICAL_TIMES}


class StationaryPlanTests(TestCase):
    def binding(self,value):return stationary_binding('obj_test',value,'a'*64,dict(game_module='b'*64,bsp='c'*64,candidate='d'*64))

    def test_rejected_resting_points_are_replaced_without_way_or_mission_changes(self):
        value=report();before=copy.deepcopy(value)
        base=make_plan(value,[objective()],5)
        bad={g['source_node']for g in base['goals']if g['type']in TACTICAL_TIMES}
        rejected=set(sorted(bad)[:3]);calls=[]
        def probe(a,b):
            calls.append((a,b));return reply(a,b,stand='capsule_support_gap_or_drop'if int(a[0]/400)in rejected else 'accepted')
        gate=StationarySupport(value,self.binding(value),probe=probe)
        plan=make_plan(value,[objective()],5,gate)
        self.assertEqual(counts(base),counts(plan))
        self.assertTrue(plan['stationary_support']['complete'])
        self.assertFalse(rejected&{g['source_node']for g in plan['goals']if g['type']in TACTICAL_TIMES})
        self.assertEqual([g for g in base['goals']if g['type']=='TCEOBJECTIVE'],[g for g in plan['goals']if g['type']=='TCEOBJECTIVE'])
        self.assertEqual(value,before)
        self.assertEqual(len(calls),len({tuple(a)for a,b in calls}))
        self.assertTrue(all(a==b for a,b in calls))

    def test_both_plans_share_one_probe_per_exact_candidate_and_keep_counts(self):
        value=report();probe=Mock(side_effect=reply)
        gate=StationarySupport(value,self.binding(value),probe=probe,observation=dict(probed_gametype=5,process_id=123))
        first=make_plan(value,[objective('capture')],2,gate)
        count=probe.call_count
        second=make_plan(value,[objective()],5,gate)
        self.assertLessEqual(probe.call_count,len(value['nodes']))
        self.assertLessEqual(probe.call_count,STATIONARY_PROBE_LIMIT)
        self.assertEqual(first['stationary_support']['probed_gametypes'],[5])
        self.assertEqual(second['stationary_support']['probed_gametypes'],[5])
        self.assertGreater(count,0)
        self.assertTrue(all(g['stationary_support_checked']for p in (first,second)for g in p['goals']if g['type']in TACTICAL_TIMES))
        self.assertEqual(counts(first),counts(make_plan(value,[objective('capture')],2)))
        self.assertEqual(counts(second),counts(make_plan(value,[objective()],5)))

    def test_actual_snow_exact_import_is_not_a_stationary_support_proof(self):
        root=Path(__file__).resolve().parents[1]
        evidence=root/'build/navstudio-distribution-20261015/obj_snow/camp-destination-four-native-4fa1/result.json'
        if not evidence.exists():self.skipTest('local real Snow evidence not distributed')
        row=next(r for r in json.loads(evidence.read_bytes())['queries']if r['name']=='original-goal-self')
        value=report(2);node=value['nodes'][0]
        node.update(id=4604,position=[864.,166.2769,13.65703])
        value['edges']=[dict(**{'from':4604,'to':1},accepted=True),dict(**{'from':1,'to':4604},accepted=True)]
        gate=StationarySupport(value,self.binding(value),probe=lambda a,b:row['raw_reply'])
        self.assertFalse(gate.allows(node))
        self.assertEqual(gate.records[4604]['reason'],'capsule_support_gap_or_drop')
        self.assertTrue(row['proof']['imports']['start']['stand']['exact'])
        self.assertEqual(node['position'],[864.,166.2769,13.65703])

    def test_supported_floor_skin_does_not_waive_exact_self_walk(self):
        value=report(2);probe=lambda a,b:reply(a,b,skin=.125)
        gate=StationarySupport(value,self.binding(value),probe=probe)
        self.assertTrue(gate.allows(value['nodes'][0]))
        self.assertEqual(gate.records[0]['feet'],[0.,0.,0.])

    def test_crouch_matches_crouch_result_not_standing_or_any_accepted_import(self):
        value=report(2);value['nodes'][0]['crouch']=True
        gate=StationarySupport(value,self.binding(value),probe=lambda a,b:reply(a,b,stand='blocked_clearance'))
        self.assertTrue(gate.allows(value['nodes'][0]))
        self.assertEqual(gate.records[0]['posture'],'crouch')
        gate=StationarySupport(value,self.binding(value),probe=lambda a,b:reply(a,b,crouch='blocked_clearance'))
        self.assertFalse(gate.allows(value['nodes'][0]))

    def test_cache_is_reparsed_and_original_observation_retained_without_fresh_claim(self):
        value=report(2);binding=self.binding(value)
        first=StationarySupport(value,binding,probe=reply,observation=dict(probed_gametype=2,process_id=12))
        first.allows(value['nodes'][0]);certificate=first.certificate()
        probe=Mock(side_effect=AssertionError('cached raw must be reused'))
        cached=StationarySupport(value,binding,probe=probe,certificate=certificate,observation=dict(probed_gametype=5,process_id=24))
        self.assertTrue(cached.allows(value['nodes'][0]));probe.assert_not_called()
        self.assertEqual(cached.records[0]['observation'],dict(probed_gametype=2,process_id=12))
        tampered=copy.deepcopy(certificate);tampered['queries'][0]['accepted']=False
        with self.assertRaisesRegex(RuntimeError,'Rohantwort'):StationarySupport(value,binding,certificate=tampered)

    def test_stale_module_candidate_body_native_or_point_cannot_supply_acceptance(self):
        value=report(2);binding=self.binding(value)
        first=StationarySupport(value,binding,probe=reply);first.allows(value['nodes'][0]);certificate=first.certificate()
        for key in ('native_report_sha256','candidates_sha256','proof_inputs','body_profile'):
            changed=copy.deepcopy(binding);changed[key]='other'
            with self.subTest(key=key),self.assertRaisesRegex(RuntimeError,'veraltet'):StationarySupport(value,changed,certificate=certificate)
        altered=copy.deepcopy(value);altered['nodes'][0]['position'][0]=1
        with self.assertRaisesRegex(RuntimeError,'veraltet'):StationarySupport(altered,self.binding(altered),certificate=certificate)

    def test_missing_duplicate_wrong_coordinate_or_unclear_reply_fails_closed(self):
        value=report(2);binding=self.binding(value)
        for probe in (lambda a,b:'lost UDP reply',lambda a,b:reply([1,2,3],b),lambda a,b:reply(a,b)+reply(a,b)):
            with self.subTest(probe=probe),self.assertRaises(RuntimeError):StationarySupport(value,binding,probe=probe).allows(value['nodes'][0])

    def test_budget_exhaustion_never_installs_unprobed_tactical_positions(self):
        value=report(110);probe=Mock(side_effect=lambda a,b:reply(a,b,stand='capsule_support_gap_or_drop'))
        gate=StationarySupport(value,self.binding(value),probe=probe)
        plan=make_plan(value,[],5,gate)
        self.assertEqual(probe.call_count,96)
        self.assertEqual(counts(plan),dict(CAMP=0,Defend=0,Attack=0))
        summary=plan['stationary_support']
        self.assertFalse(summary['complete']);self.assertTrue(summary['budget_exhausted'])
        self.assertEqual(len(summary['rejected']),96)
        self.assertEqual(summary['status'],'incomplete')

    def test_external_plan_without_explicit_certificate_is_clearly_unchecked(self):
        plan=make_plan(report(),[objective()],5)
        self.assertEqual(plan['stationary_support']['status'],'unchecked')
        self.assertFalse(plan['stationary_support']['complete'])
        self.assertTrue(all(not g['stationary_support_checked']for g in plan['goals']if g['type']in TACTICAL_TIMES))

    def test_no_stationary_goal_is_never_reported_complete_even_below_budget(self):
        value=report(2)
        gate=StationarySupport(value,self.binding(value),probe=lambda a,b:reply(a,b,stand='blocked_clearance'))
        plan=make_plan(value,[],5,gate)
        self.assertEqual(plan['stationary_support']['query_count'],2)
        self.assertFalse(plan['stationary_support']['budget_exhausted'])
        self.assertFalse(plan['stationary_support']['complete'])


class StationaryServerTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.value=report(5)
        self.native=self.root/'obj_test_navmesh.json';self.native.write_text(json.dumps(self.value))
        self.studio=Studio.__new__(Studio);s=self.studio
        s.active_project=dict(id='one',map='obj_test',paths=dict(work=str(self.root)))
        s.require_map=lambda n:n;s.native=lambda n:self.value;s.native_path=lambda n:self.native
        s.file_hash=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
        self.inputs=dict(game_module='b'*64,bsp='c'*64,candidate='d'*64)
        s.proof_status=lambda n:dict(valid=True,inputs=self.inputs)
        s.own_test_process=Mock(return_value=dict(pid=12));s.game_executable=self.root/'ET.exe'
        self.identity=[os.path.normcase(str(s.game_executable.resolve())),1234]
        s.process_api=SimpleNamespace(identity=Mock(return_value=self.identity))
        s.require_loaded=Mock();s.live_gametype=Mock(return_value=5)
        s.require_bbox=Mock(return_value=native_body_profile(self.value));s.require_profile_inputs=Mock()
        s.candidate=lambda n:self.root/'candidate.txt'
        s.update=lambda job,**values:job.update(values)
        def rcon(command,**kwargs):
            self.assertEqual(kwargs,dict(read_only=True))
            self.assertTrue(command.startswith('tce_navgen_probe feet '))
            xyz=[float(v)for v in command.split()[2:]]
            return reply(xyz[:3],xyz[3:])
        s.rcon=Mock(side_effect=rcon)
        s.atomic_copy=lambda a,b:Path(b).write_bytes(Path(a).read_bytes())
        self.job=dict(map='obj_test',gametype=5,log='')

    def test_server_archives_exact_readonly_queries_and_reuses_only_matching_cache(self):
        gate=self.studio.stationary_goal_support(self.job)
        self.assertTrue(gate.allows(self.value['nodes'][0]))
        evidence=gate.finalizer(gate.certificate())
        archive=Path(evidence['path']);raw=archive.read_bytes()
        self.assertEqual(evidence['sha256'],hashlib.sha256(raw).hexdigest())
        self.assertEqual(evidence['probed_gametypes'],[5])
        self.assertFalse(json.loads(raw)['bots_moved'])
        cache=self.studio.stationary_goal_support(self.job)
        self.assertTrue(cache.allows(self.value['nodes'][0]));self.assertEqual(self.studio.rcon.call_count,1)
        self.inputs=dict(self.inputs,candidate='e'*64)
        fresh=self.studio.stationary_goal_support(self.job)
        self.assertTrue(fresh.allows(self.value['nodes'][0]));self.assertEqual(self.studio.rcon.call_count,2)

    def test_changed_process_map_gt_body_or_proof_prevents_archive_finalization(self):
        for change in ('process','gt','body','proof'):
            with self.subTest(change=change):
                self.setUp();gate=self.studio.stationary_goal_support(self.job);gate.allows(self.value['nodes'][0])
                if change=='process':self.studio.process_api.identity.return_value=['other',2345]
                elif change=='gt':self.studio.live_gametype.return_value=2
                elif change=='body':self.studio.require_bbox.return_value={}
                else:self.inputs=dict(self.inputs,bsp='e'*64)
                with self.assertRaises(RuntimeError):gate.finalizer(gate.certificate())
                self.assertFalse((self.root/'stationary-goals').exists())

    def test_source_changes_during_generation_cannot_write_any_goals(self):
        s=self.studio;s.game_mode='tce2'
        bundles=self.root/'bundles';bundles.mkdir()
        s.active_project['paths']['bundles']=str(bundles);s.assets=script_assets(self.root)
        gate=s.stationary_goal_support(self.job)
        original=gate.probe
        def changed(a,b):
            result=original(a,b);self.inputs=dict(self.inputs,candidate='e'*64);return result
        gate.probe=changed
        with patch('goalgen.objectives',return_value=[]),self.assertRaisesRegex(RuntimeError,'Prüfgrundlage'):
            generate(s,'obj_test',stationary_support=gate)
        self.assertFalse(list(bundles.iterdir()))
        self.assertFalse((self.root/'generated-goals.json').exists())

    def test_generation_embeds_shared_raw_certificate_and_complete_team_quotas(self):
        s=self.studio;s.game_mode='tce2'
        bundles=self.root/'bundles';bundles.mkdir()
        s.active_project['paths']['bundles']=str(bundles);s.assets=script_assets(self.root)
        gate=s.stationary_goal_support(self.job)
        with patch('goalgen.objectives',return_value=[]):result=generate(s,'obj_test',stationary_support=gate)
        manifest=json.loads(Path(result['manifest']).read_bytes())
        self.assertEqual(s.rcon.call_count,5)
        self.assertEqual(len(manifest['stationary_support_certificate']['queries']),5)
        for plan in manifest['plans']:
            self.assertTrue(plan['stationary_support']['complete'])
            self.assertEqual(plan['stationary_support']['shortfalls'],[])
            self.assertEqual(plan['stationary_support']['probed_gametypes'],[5])
        raw=Path(manifest['stationary_support_evidence']['path']).read_bytes()
        self.assertEqual(json.loads(raw)['certificate'],manifest['stationary_support_certificate'])
        self.assertEqual(hashlib.sha256(raw).hexdigest(),manifest['stationary_support_evidence']['sha256'])

    def test_changed_source_after_finalizer_still_cannot_write_goals(self):
        s=self.studio;s.game_mode='tce2'
        bundles=self.root/'bundles';bundles.mkdir()
        s.active_project['paths']['bundles']=str(bundles);s.assets=script_assets(self.root)
        gate=s.stationary_goal_support(self.job)
        def finalizer(certificate):self.inputs=dict(self.inputs,candidate='e'*64)
        gate.finalizer=finalizer
        with patch('goalgen.objectives',return_value=[]),self.assertRaisesRegex(RuntimeError,'vor dem Speichern'):
            generate(s,'obj_test',stationary_support=gate)
        self.assertFalse(list(bundles.iterdir()))


if __name__=='__main__':main()
