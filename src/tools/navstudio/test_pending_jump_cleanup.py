"""Only unchanged unentered machine jumps with two current walk bypasses retire."""
import copy
import hashlib
import json
import math
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock,patch

from edit import Drafts,graph_hash
from repair import redundant_pending_jump_feedback
import test_rejected_ordinary_feedback as ordinary_fixtures
import test_path_repair as path_fixtures
import test_path_repair_workflow as workflow_fixtures


class PendingJumpCleanupTests(unittest.TestCase):
    def fixture(self):
        positions={1:[704.,-2327.8762,239.729],2:[768.,-2217.0251,233.6111],
                   3:[832.,-2327.8762,239.729],4:[714.,-2310.5557,256.25]}
        doc,native,_=ordinary_fixtures.RejectedOrdinaryFeedbackTests().fixture(positions,
            [(1,4),(4,2),(1,3),(3,2),(2,3),(3,1)],{(1,4):'capsule_support_step_or_drop'})
        doc['base']['movement_profile']=dict(radius=16,standing_height=87,crouching_height=61,origin_above_feet=30,step=18)
        native['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        doc['next_id']=10
        helper=doc['nodes'][3]
        helper.update(name='',facing=[0,0,0],radius=8,source_radius=8,nav_flags=1<<27,
                      generated_guide=True,source_reason='generated_jump_takeoff_live_pending')
        row=native['nodes'][3]
        row.update(name='',facing=[0,0,0],nav_flags=1<<27,radius=8,editor_role=1,
                   accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn',
                   native_input=dict(floor_accepted=True,status='missing_spawn_route',initial_incoming=0,
                                     initial_outgoing=1,initial_rejected_incident_links=1))
        native['edges'][1].update(accepted=False,reason='quarantined_unreachable_endpoint')
        original=copy.deepcopy(native)
        original.update(candidate_mode='automatic_spatial',generation_complete=True,
                        source='navgen/obj_test_candidate.txt',traversal_routes=[dict(kind='jump',exported=True,
                        approach=1,launch=4,landing=2)])
        old=original['nodes'][3]
        old.update(accepted=True,spawn_reachable=True,reason='generated_jump_takeoff_live_pending',index=3,
                   topology_protected=False)
        old.pop('editor_role');old.pop('way_guid')
        for edge in original['edges']:edge['accepted']=True
        raw=json.dumps(original).encode()
        doc['base'].update(native_report=hashlib.sha256(raw).hexdigest(),candidate_name='obj_test_candidate.txt')
        return doc,native,path_fixtures.PathRepairTests().bind(doc,native),raw

    def evaluate(self,doc,native,raw):
        return redundant_pending_jump_feedback(doc,native,path_fixtures.PathRepairTests().bind(doc,native),raw)

    def test_actual_length_two_directions_preserve_all_ordinary_data(self):
        doc,native,binding,raw=self.fixture();before=copy.deepcopy((doc,native,binding))
        proposal,evidence=redundant_pending_jump_feedback(doc,native,binding,raw)
        self.assertEqual((doc,native,binding),before)
        self.assertEqual(proposal['nodes'],doc['nodes'][:3]);self.assertEqual(proposal['edges'],sorted(doc['edges'][2:],key=lambda e:(e['from'],e['to'])))
        self.assertEqual(proposal['next_id'],10)
        self.assertEqual([p['source_ids'] for p in evidence['ordinary_alternatives']],[[1,3,2],[2,3,1]])
        for path in evidence['ordinary_alternatives']:self.assertAlmostEqual(path['length_units'],256.146,places=2)
        self.assertEqual(evidence['tombstoned_way_guid'],4)
        self.assertTrue(evidence['native_revalidation_required']);self.assertFalse(evidence['installed'])

    def test_user_changes_and_every_nonpending_or_nonmachine_variant_remain(self):
        variants=[('name','mine'),('locked',True),('facing',[0,1,0]),('nav_flags',0),('crouch',True),
                  ('spawn',True),('radius',9),('source_radius',9),('position',[715,-2310.5557,256.25]),
                  ('generated_guide',False),('source_reason','manual'),('action_properties',{'custom':'yes'}),
                  ('action_roles',[dict(kind='jump',landing_node=2)]),('editor_role',64),('user_field',1)]
        for key,value in variants:
            doc,native,_,raw=self.fixture();doc['nodes'][3][key]=value
            with self.subTest(key=key):
                try:result=self.evaluate(doc,native,raw)
                except RuntimeError:result=None
                self.assertIsNone(result)
        for history in ('undo','redo'):
            doc,native,_,raw=self.fixture();doc[history]=[dict(before=[copy.deepcopy(doc['nodes'][3])])]
            self.assertIsNone(self.evaluate(doc,native,raw))

    def test_original_literal_machine_provenance_is_mandatory(self):
        for case in ('bytes','map','complete','mode','profile','pose','uid','radius','jump','extra_incident'):
            doc,native,_,raw=self.fixture();old=json.loads(raw)
            if case=='bytes':raw+=b' '
            else:
                if case=='map':old['map']='other'
                elif case=='complete':old['generation_complete']=False
                elif case=='mode':old['candidate_mode']='manual_draft'
                elif case=='profile':old['bbox']['half_width']=14
                elif case=='pose':old['nodes'][3]['position'][0]+=1
                elif case=='uid':old['nodes'][3]['index']=99
                elif case=='radius':old['nodes'][3]['radius']=9
                elif case=='jump':old['traversal_routes']=[]
                else:old['edges'].append(dict(**{'from':3,'to':4},accepted=True,crouch=False))
                raw=json.dumps(old).encode();doc['base']['native_report']=hashlib.sha256(raw).hexdigest()
            with self.subTest(case=case):self.assertIsNone(self.evaluate(doc,native,raw))

    def test_current_floor_direction_hazard_and_action_negatives(self):
        for case in ('floor','incoming','outgoing','accepted','unknown','hazard','budget','protected','action','reverse'):
            doc,native,_,raw=self.fixture();row=native['nodes'][3]
            if case=='floor':row['native_input']['floor_accepted']=False
            elif case=='incoming':row['native_input']['initial_incoming']=1
            elif case=='outgoing':row['native_input']['initial_outgoing']=2
            elif case=='accepted':row.update(accepted=True,spawn_reachable=True)
            elif case in ('unknown','hazard','budget'):native['edges'][0]['reason']=case
            elif case=='protected':doc['base']['protected_action_edges']=[[1,3]]
            elif case=='action':doc['nodes'][2]['nav_flags']=1<<27;native['nodes'][2]['nav_flags']=1<<27
            else:native['edges'][4].update(accepted=False,reason='blocked_clearance')
            with self.subTest(case=case):
                try:result=self.evaluate(doc,native,raw)
                except RuntimeError:result=None
                self.assertIsNone(result)

    def test_path_length_fourfold_absolute_and_hop_limits(self):
        for case in ('fourfold','absolute','13hops'):
            doc,native,_,raw=self.fixture()
            if case in ('fourfold','absolute'):
                if case=='absolute':
                    doc['nodes'][1]['position']=[1704.,-2327.8762,239.729]
                    native['nodes'][1]['position']=native['nodes'][1]['raw_position']=[1704.,-2327.8762,269.729]
                doc['nodes'][2]['position']=[832.,-4000.,239.729]
                native['nodes'][2]['position']=native['nodes'][2]['raw_position']=[832.,-4000.,269.729]
            else:
                doc['nodes'].remove(doc['nodes'][2]);native['nodes'].remove(native['nodes'][2])
                doc['edges']=doc['edges'][:2];native['edges']=native['edges'][:2]
                ids=[1]+list(range(10,22))+[2]
                for index,identity in enumerate(ids[1:-1],1):
                    n=copy.deepcopy(doc['nodes'][0]);n.update(id=identity,way_guid=identity,spawn=False,
                        position=[704+64*index/13,-2327.8762+110.8511*index/13,239.729-6.1179*index/13])
                    r=copy.deepcopy(native['nodes'][0]);r.update(id=identity,import_uid=identity,position=n['position'][:2]+[n['position'][2]+30],
                        raw_position=n['position'][:2]+[n['position'][2]+30])
                    doc['nodes'].append(n);native['nodes'].append(r)
                for a,b in zip(ids,ids[1:]):
                    for f,t in ((a,b),(b,a)):
                        doc['edges'].append({'from':f,'to':t});native['edges'].append(dict(
                            **{'from':f,'to':t},accepted=True,crouch=False,reason='external_native_validated_walk'))
                doc['next_id']=22
            with self.subTest(case=case):self.assertIsNone(self.evaluate(doc,native,raw))

    def studio_fixture(self):
        parent=workflow_fixtures.PathRepairWorkflowTests();self.addCleanup(parent.doCleanups)
        s,_,_,_,_=parent.fixture();doc,native,binding,raw=self.fixture()
        s.drafts.documents['obj_test']=copy.deepcopy(doc)
        s.native_path('obj_test').write_bytes(binding['native_report_bytes'])
        candidate=s.candidate('obj_test');candidate.write_bytes(binding['candidate_bytes'])
        candidate.with_suffix('.json').write_bytes(binding['candidate_metadata_bytes'])
        (s.work/'obj_test.native-attempt.json').write_text(json.dumps(binding['attempt']))
        s.proof_status.return_value=dict(valid=True,inputs=binding['inputs']);s.report_status.return_value=binding['report_status']
        original=s.work/'generation-scans'/('obj_test-'+doc['base']['native_report'])/'scanner.json'
        original.parent.mkdir(parents=True);original.write_bytes(raw)
        return s,doc,original

    def test_real_caller_delete_undo_monotonic_id_and_five_immutable_sources(self):
        s,doc,_=self.studio_fixture();job=dict(map='obj_test',action='repair_check',log='')
        evidence=s.repair_pending_jump_draft(job);current=s.drafts.get('obj_test')
        self.assertEqual(current['nodes'],doc['nodes'][:3]);self.assertEqual(current['next_id'],10)
        self.assertEqual(len(evidence['source_evidence']),5)
        for item in evidence['source_evidence'].values():self.assertEqual(hashlib.sha256(Path(item['path']).read_bytes()).hexdigest(),item['sha256'])
        self.assertIsNone(s.repair_pending_jump_draft(job));s.install.assert_not_called();s.rcon_with_followup.assert_not_called()
        restored=s.drafts.change('obj_test',dict(op='undo',revision=current['revision']),doc['base']['environment'])
        self.assertEqual(sorted(restored['nodes'],key=lambda n:n['id']),doc['nodes'])
        self.assertEqual({tuple(e.values())for e in restored['edges']},{tuple(e.values())for e in doc['edges']})
        removed=s.drafts.change('obj_test',dict(op='redo',revision=restored['revision']),doc['base']['environment'])
        added=s.drafts.change('obj_test',dict(op='add_node',position=[0,0,0],revision=removed['revision']),doc['base']['environment'])
        self.assertEqual(added['nodes'][-1]['id'],10);self.assertNotIn(4,[n['id']for n in added['nodes']])

    def test_check_generate_missing_original_and_source_race_never_mutate(self):
        for case in ('native_check','generate','et','missing','race'):
            s,doc,original=self.studio_fixture();job=dict(map='obj_test',action='repair_check',log='')
            if case in ('native_check','generate'):job['action']=case
            elif case=='et':s.game_mode='et'
            elif case=='missing':original.unlink()
            def raced(*args):
                result=redundant_pending_jump_feedback(*args);original.write_bytes(original.read_bytes()+b' ');return result
            with self.subTest(case=case):
                if case=='race':
                    with patch('repair.redundant_pending_jump_feedback',side_effect=raced),self.assertRaisesRegex(RuntimeError,'Grundlage'):
                        s.repair_pending_jump_draft(job)
                else:self.assertIsNone(s.repair_pending_jump_draft(job))
                self.assertEqual(s.drafts.get('obj_test'),doc);s.install.assert_not_called()

    def test_explicit_fullcheck_loop_checks_after_cleanup_and_never_masks_failure(self):
        for success in (True,False):
            s,doc,_=self.studio_fixture();s.action_repair=Mock(return_value=dict(repairs={}))
            s.recover_rejected_shortcuts=Mock(return_value=None);s.recover_rejected_ordinary=Mock(return_value=None)
            s.repair_path_draft=Mock(return_value=None);s.repair_anchor_draft=Mock(return_value=None)
            s.repair_descent_draft=Mock(return_value=None)
            calls=[]
            def check(job):
                calls.append([n['id']for n in s.drafts.get('obj_test')['nodes']])
                if len(calls)==1 or not success:raise RuntimeError('still unsafe')
                return dict(audit=dict(fresh=True))
            s.action_native_check=Mock(side_effect=check)
            with patch('repair.native_crouch_feedback',return_value=None):
                if success:self.assertFalse(s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))['installed'])
                else:
                    with self.assertRaisesRegex(RuntimeError,'still unsafe'):s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
            self.assertEqual(calls,[[1,2,3,4],[1,2,3]]);s.install.assert_not_called()


if __name__=='__main__':unittest.main()
