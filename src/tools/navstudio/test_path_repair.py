"""Pure native-chain merge contract, real Drafts Undo and Format5/6 export."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from edit import Drafts,graph_hash
from pathrepair import merge,validate_source
import test_rejected_ordinary_feedback as fixtures


class PathRepairTests(unittest.TestCase):
    def bind(self,doc,baseline):
        factory=fixtures.RejectedOrdinaryFeedbackTests();factory.environment=doc['base']['environment']
        binding=factory.bind(doc,baseline)
        candidate=f"TCE_NAVMESH 5 {doc['map']} {len(doc['nodes'])} {len(doc['edges'])}\r\n".encode()
        binding['candidate_bytes']=candidate;normalized=hashlib.sha256(candidate.replace(b'\r\n',b'\n')).hexdigest()
        binding['metadata']['candidate_sha256']=doc['export']['candidate_sha256']=normalized
        binding['candidate_metadata_bytes']=json.dumps(binding['metadata']).encode()
        binding['inputs'].update(candidate=hashlib.sha256(candidate).hexdigest(),
                                candidate_metadata=hashlib.sha256(binding['candidate_metadata_bytes']).hexdigest())
        binding['report_status']['candidate_sha256']=binding['attempt']['staged_candidate']=binding['inputs']['candidate']
        binding['attempt']['inputs']=copy.deepcopy(binding['inputs'])
        binding['attempt']['movement_profile']=doc['base']['movement_profile']
        return binding

    def fixture(self,jump=False,guide=False):
        factory=fixtures.RejectedOrdinaryFeedbackTests();doc,baseline,_=factory.fixture()
        doc['next_id']=1000
        doc['base']['movement_profile']=dict(radius=16,standing_height=87,crouching_height=61,origin_above_feet=30,step=18)
        baseline['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        doc['nodes'][2]['action_properties']={'note':'preserve opaque metadata'}
        baseline['nodes'][2]['custom_properties']={'note':'preserve opaque metadata'}
        if guide:
            doc['nodes'][0].update(generated_guide=True,editor_role=0)
            baseline['nodes'][0]['editor_role']=1
        if jump:
            doc['nodes'][1].update(nav_flags=1<<27,generated_guide=True)
            baseline['nodes'][1].update(nav_flags=1<<27,editor_role=1,accepted=False,spawn_reachable=False,
                                        reason='quarantined_no_validated_route_from_spawn')
            baseline['nodes'][1]['native_input']['status']='missing_spawn_route'
            for edge in baseline['edges']:
                if 2 in(edge['from'],edge['to']):edge.update(accepted=False,reason='quarantined_unreachable_endpoint')
        binding=self.bind(doc,baseline)
        report=copy.deepcopy(baseline)
        if jump:
            report['nodes'][1].update(accepted=True,spawn_reachable=True,reason='external_native_validated_floor')
            for edge in report['edges']:
                if edge['reason']=='quarantined_unreachable_endpoint':
                    edge.update(accepted=True,reason='manual_native_transition_live_pending')
        report['nodes'].append(dict(id=1000,raw_position=[32.,16.,30.],position=[32.,16.,30.],radius=35.,
            accepted=True,spawn_reachable=True,nav_flags=0,crouch=False,editor_role=0,way_guid=0,
            reason='generated_native_path_repair_bend',facing=[0.,1.,0.],name='Fresh bend',custom_properties={}))
        pairs=[(1,1000),(1000,2)]+([]if jump else[(2,1000),(1000,1)])
        for a,b in pairs:
            report['edges'].append(dict(**{'from':a,'to':b},accepted=True,crouch=False,
                reason='native_path_repair_jump_approach'if jump else'native_path_repair_walk'))
        report['path_repair']=dict(model='native_append_paths_v1',explicit_repair=True,preserved_input=True,
            minimum_new_source_id=1000,input_nodes=3,input_edges=4,input_validation_complete=True,
            added_node_ids=[1000],chains=[dict(kind='jump_launch_approach'if jump else'ordinary',
                source_ids=[1,1000,2],crouch=[False,False],accepted=True)]+
                ([]if jump else[dict(kind='ordinary',source_ids=[2,1000,1],crouch=[False,False],accepted=True)]),
            traces=50,trace_budget=5000,budget_complete=True,stop_reason='bounded_frontier_exhausted')
        report['traces']+=50;binding['repair_report_bytes']=json.dumps(report).encode()
        return doc,baseline,report,binding

    def run_merge(self,parts):return merge(parts[0],parts[2],30,parts[1],parts[3])

    def test_source_binding_is_available_before_any_native_search(self):
        doc,baseline,_,binding=self.fixture();binding.pop('repair_report_bytes')
        self.assertIs(validate_source(doc,baseline,30,binding),True)

    def test_partial_report_appends_only_exact_directed_chains_and_retains_bad_old_edges(self):
        parts=self.fixture();before=copy.deepcopy(parts);proposal,evidence=self.run_merge(parts)
        doc,baseline,report,binding=parts
        self.assertEqual(parts,before);self.assertEqual(proposal['nodes'][:3],doc['nodes'])
        self.assertEqual(proposal['edges'][:4],doc['edges']);self.assertFalse(report['generation_complete'])
        self.assertEqual(evidence['added_node_ids'],[1000]);self.assertEqual(evidence['added_directed_edges'],4)
        self.assertEqual(evidence['retained_rejected_edges'],[[1,2]])
        self.assertEqual(evidence['source_native_report'],hashlib.sha256(binding['native_report_bytes']).hexdigest())
        self.assertTrue(evidence['native_revalidation_required']);self.assertFalse(evidence['installed'])
        self.assertFalse(evidence['changes_existing_node_metadata']);self.assertFalse(evidence['adopts_existing_way_uids'])
        self.assertEqual(proposal['nodes'][-1]['position'],[32.,16.,0.]);self.assertEqual(proposal['next_id'],1001)
        self.assertEqual(proposal['nodes'][-1]['way_guid'],0)

    def test_ordinary_generated_floor_guide_is_retained_without_action_waiver(self):
        parts=self.fixture(guide=True);proposal,_=self.run_merge(parts)
        self.assertEqual(proposal['nodes'][:3],parts[0]['nodes'])
        for change in('role','action','flag'):
            parts=self.fixture(guide=True);doc,_,report,binding=parts
            if change=='role':report['nodes'][0]['editor_role']=3
            elif change=='action':doc['nodes'][0]['action_roles']=[dict(kind='jump')]
            else:report['nodes'][0]['nav_flags']=512
            binding['repair_report_bytes']=json.dumps(report).encode()
            with self.subTest(change=change),self.assertRaises(RuntimeError):self.run_merge(parts)

    def test_preexisting_proved_crouch_is_kept_but_repair_must_not_promote_authored_posture(self):
        parts=self.fixture();doc,baseline,report,_=parts
        for i in(0,1):
            doc['nodes'][i].update(nav_flags=64,crouch=True)
            baseline['nodes'][i].update(nav_flags=64,crouch=True)
            report['nodes'][i].update(nav_flags=64,crouch=True)
        report['nodes'][-1].update(nav_flags=64,crouch=True)
        for edge in report['edges'][4:]:edge['crouch']=True
        for chain in report['path_repair']['chains']:chain['crouch']=[True,True]
        parts[3].update(self.bind(doc,baseline));parts[3]['repair_report_bytes']=json.dumps(report).encode()
        proposal,_=self.run_merge(parts)
        self.assertEqual(proposal['nodes'][:3],doc['nodes']);self.assertTrue(proposal['nodes'][-1]['crouch'])
        parts=self.fixture();parts[2]['nodes'][0]['crouch']=True
        parts[3]['repair_report_bytes']=json.dumps(parts[2]).encode()
        with self.assertRaises(RuntimeError):self.run_merge(parts)

    def mixed_fixture(self):
        doc,baseline,report,_=self.fixture()
        doc['nodes'][0].update(nav_flags=64,crouch=True)
        baseline['nodes'][0].update(nav_flags=64,crouch=True)
        report['nodes'][0].update(nav_flags=64,crouch=True)
        report['nodes'][-1].update(nav_flags=64,crouch=True)
        for edge,duck in zip(report['edges'][4:],[True,False,False,True]):edge['crouch']=duck
        report['path_repair']['chains'][0]['crouch']=[True,False]
        report['path_repair']['chains'][1]['crouch']=[False,True]
        binding=self.bind(doc,baseline);binding['repair_report_bytes']=json.dumps(report).encode()
        return doc,baseline,report,binding

    def test_mixed_crouch_corridor_keeps_old_standing_terminal_with_two_directed_stand_boundaries(self):
        parts=self.mixed_fixture();before=copy.deepcopy(parts);proposal,evidence=self.run_merge(parts)
        self.assertEqual(parts,before);self.assertEqual(proposal['nodes'][:3],parts[0]['nodes'])
        self.assertEqual([n['nav_flags']for n in proposal['nodes']],[64,0,0,64])
        self.assertEqual([r['crouch']for r in evidence['chains']],[[True,False],[False,True]])
        self.assertTrue(evidence['native_revalidation_required']);self.assertFalse(evidence['changes_existing_node_metadata'])

    def test_mixed_boundary_duck_only_marking_fails_in_both_directions_without_mutating_old_flags(self):
        for chain_index,segment_index,edge_index in((0,1,5),(1,0,6)):
            parts=self.mixed_fixture();doc,_,report,binding=parts;before=copy.deepcopy(doc)
            # Keep edge and chain claims mutually consistent. The rejection
            # must come from the standing-boundary contract, not a mismatch.
            report['path_repair']['chains'][chain_index]['crouch'][segment_index]=True
            report['edges'][edge_index]['crouch']=True
            binding['repair_report_bytes']=json.dumps(report).encode()
            with self.subTest(direction=chain_index),self.assertRaisesRegex(RuntimeError,'Duck-/Stehgrenze'):
                self.run_merge(parts)
            self.assertEqual(doc,before);self.assertEqual(doc['nodes'][1]['nav_flags'],0)

    def test_exact_thirty_two_vertex_boundary_and_directional_ordinary_chain(self):
        for length,accepted in((32,True),(33,False)):
            parts=self.fixture();doc,_,report,binding=parts;template=copy.deepcopy(report['nodes'][-1])
            report['nodes']=report['nodes'][:3];report['edges']=report['edges'][:4]
            ids=list(range(1000,1000+length-2));path=[1]+ids+[2]
            for index,identity in enumerate(ids,1):
                row=copy.deepcopy(template);row.update(id=identity,position=[64*index/(length-1),16.,30.],
                    raw_position=[64*index/(length-1),16.,30.]);report['nodes'].append(row)
            for a,b in zip(path,path[1:]):
                report['edges'].append(dict(**{'from':a,'to':b},accepted=True,crouch=False,reason='native_path_repair_walk'))
            report['path_repair'].update(added_node_ids=ids,chains=[dict(kind='ordinary',source_ids=path,
                crouch=[False]*(length-1),accepted=True)])
            binding['repair_report_bytes']=json.dumps(report).encode()
            with self.subTest(vertices=length):
                if accepted:
                    proposal,evidence=self.run_merge(parts);self.assertEqual(evidence['added_directed_edges'],31)
                    self.assertNotIn({'from':2,'to':ids[-1]},proposal['edges'])
                else:
                    with self.assertRaises(RuntimeError):self.run_merge(parts)

    def test_jump_launch_gets_only_standing_incoming_chain_and_keeps_sole_outgoing_action(self):
        parts=self.fixture(jump=True);proposal,evidence=self.run_merge(parts)
        self.assertEqual(evidence['added_directed_edges'],2)
        self.assertIn({'from':2,'to':1},proposal['edges'])
        self.assertNotIn({'from':2,'to':1000},proposal['edges'])
        self.assertEqual(proposal['nodes'][:3],parts[0]['nodes'])
        for case in('crouch','terminal_flag','terminal_reject','outgoing_reject','reverse','ordinary_label'):
            parts=self.fixture(jump=True);_,_,report,binding=parts
            if case=='crouch':report['path_repair']['chains'][0]['crouch'][0]=True
            elif case=='terminal_flag':report['nodes'][1]['nav_flags']|=512
            elif case=='terminal_reject':report['nodes'][1]['accepted']=False
            elif case=='outgoing_reject':report['edges'][3]['accepted']=False
            elif case=='reverse':report['path_repair']['chains'][0]['source_ids']=[2,1000,1]
            else:report['path_repair']['chains'][0]['kind']='ordinary'
            binding['repair_report_bytes']=json.dumps(report).encode()
            with self.subTest(case=case),self.assertRaises(RuntimeError):self.run_merge(parts)

    def test_source_staleness_raw_bytes_metadata_profile_and_deleted_id_boundary_fail_closed(self):
        for case in('not_current','unsaved','revision','graph','candidate','metadata','report','attempt',
                    'environment','profile','candidate_source','metadata_object','old_position','old_uid',
                    'old_name','old_property','missing_node','missing_edge','truncated','initial_counts','trace_budget'):
            parts=self.fixture();doc,baseline,_,binding=parts
            if case=='not_current':binding['report_status']['current']=False
            elif case=='unsaved':doc['saved_revision']-=1
            elif case=='revision':binding['metadata']['edit_draft']['revision']-=1
            elif case=='graph':doc['export']['sha256']='0'*64
            elif case=='candidate':binding['candidate_bytes']+=b' '
            elif case=='metadata':binding['candidate_metadata_bytes']+=b' '
            elif case=='report':binding['native_report_bytes']+=b' '
            elif case=='attempt':binding['attempt']['inputs']['game_module']='0'*64
            elif case=='environment':doc['base']['environment']['bsp']='0'*64
            elif case=='profile':binding['attempt']['movement_profile']=dict(radius=14)
            elif case=='candidate_source':baseline['source']='other_candidate.txt'
            elif case=='metadata_object':binding['metadata']['nodes'][0]['name']='changed'
            elif case=='old_position':baseline['nodes'][0]['raw_position'][0]+=1
            elif case=='old_uid':baseline['nodes'][0]['import_uid']=42
            elif case=='old_name':baseline['nodes'][0]['name']='changed'
            elif case=='old_property':baseline['nodes'][2]['custom_properties']={}
            elif case=='missing_node':baseline['nodes'].pop()
            elif case=='missing_edge':baseline['edges'].pop()
            elif case=='truncated':baseline['native_input_coverage']['capacity']['input_truncated']=True
            elif case=='initial_counts':baseline['native_input_coverage']['initial_rejected_links']+=1
            else:baseline['traces']=baseline['trace_budget']
            # Rehash modified native objects so metadata/coverage tests cannot
            # pass merely because the raw-byte binding caught an earlier error.
            if case.startswith('old_')or case in('candidate_source','missing_node','missing_edge','truncated','initial_counts','trace_budget'):
                binding['native_report_bytes']=json.dumps(baseline).encode()
                binding['report_status']['native_report']=binding['attempt']['native_report']=hashlib.sha256(binding['native_report_bytes']).hexdigest()
            with self.subTest(case=case),self.assertRaises(RuntimeError):validate_source(doc,baseline,30,binding)

    def test_native_repair_cannot_move_or_upgrade_authored_records_or_add_unlisted_nodes_edges(self):
        for case in('explicit','preserved','boundary','reused_id','listed_id','budget','input_count','raw_mismatch','map',
                    'profile','old_raw','old_projected','old_flags','old_posture','old_name','old_uid','old_property',
                    'old_role','old_edge_upgrade','old_edge_removed','unlisted_edge','unaccepted_edge','false_chain',
                    'wrong_direction','chain_cycle','unknown_kind','new_action','new_not_reachable','new_hazard',
                    'new_property','new_radius','unknown_reason'):
            parts=self.fixture();doc,_,report,binding=parts;recipe=report['path_repair']
            if case=='explicit':recipe['explicit_repair']=False
            elif case=='preserved':recipe['preserved_input']=False
            elif case=='boundary':recipe['minimum_new_source_id']=999
            elif case=='reused_id':report['nodes'][-1]['id']=999
            elif case=='listed_id':recipe['added_node_ids']=[]
            elif case=='budget':recipe['budget_complete']=False
            elif case=='input_count':recipe['input_nodes']+=1
            elif case=='map':report['map']='obj_other'
            elif case=='profile':report['bbox']['half_width']=14
            elif case=='old_raw':report['nodes'][0]['raw_position'][0]+=1
            elif case=='old_projected':report['nodes'][0]['position'][2]+=.125
            elif case=='old_flags':report['nodes'][0]['nav_flags']=64
            elif case=='old_posture':report['nodes'][0]['crouch']=True
            elif case=='old_name':report['nodes'][0]['name']='changed'
            elif case=='old_uid':report['nodes'][0]['import_uid']=42
            elif case=='old_property':report['nodes'][2]['custom_properties']={}
            elif case=='old_role':report['nodes'][0]['editor_role']=64
            elif case=='old_edge_upgrade':report['edges'][0].update(accepted=True,reason='accepted')
            elif case=='old_edge_removed':report['edges'].pop(0)
            elif case=='unlisted_edge':report['edges'].append(dict(**{'from':3,'to':1000},accepted=True,reason='native_path_repair_walk'))
            elif case=='unaccepted_edge':report['edges'][-1]['accepted']=False
            elif case=='false_chain':recipe['chains'][0]['accepted']=False
            elif case=='wrong_direction':recipe['chains'][0]['source_ids']=[1,1000,3]
            elif case=='chain_cycle':recipe['chains'][0]['source_ids']=[1,1000,1,2]
            elif case=='unknown_kind':recipe['chains'][0]['kind']='teleport'
            elif case=='new_action':report['nodes'][-1]['nav_flags']=512
            elif case=='new_not_reachable':report['nodes'][-1]['spawn_reachable']=False
            elif case=='new_hazard':report['nodes'][-1]['accepted']=False
            elif case=='new_property':report['nodes'][-1]['custom_properties']={'tce_walk_drop':'1'}
            elif case=='new_radius':report['nodes'][-1]['radius']=float('nan')
            elif case=='unknown_reason':report['edges'][-1]['reason']='unproved_visibility'
            if case!='raw_mismatch':binding['repair_report_bytes']=json.dumps(report).encode()
            else:binding['repair_report_bytes']+=b' invalid'
            with self.subTest(case=case),self.assertRaises(RuntimeError):self.run_merge(parts)

    def test_real_drafts_apply_undo_and_format6_export_never_adopt_existing_uid_or_restore_deletions(self):
        parts=self.fixture();doc,baseline,report,binding=parts;proposal,_=self.run_merge(parts)
        with tempfile.TemporaryDirectory()as folder:
            store=Drafts(Path(folder)/'drafts');store.documents[doc['map']]=copy.deepcopy(doc)
            applied=store.apply_repair(doc['map'],doc['revision'],doc['base']['environment'],proposal)
            self.assertEqual(applied['undo'][-1]['before'],[])
            self.assertEqual(applied['nodes'][:3],doc['nodes']);self.assertEqual(applied['next_id'],1001)
            store.save(doc['map'],applied['revision'],doc['base']['environment'])
            candidate=store.export_candidate(doc['map'],applied['revision'],doc['base']['environment'],Path(folder)/'preview')
            self.assertRegex(candidate.read_text().splitlines()[0],r'^TCE_NAVMESH [56] ')
            restored=store.change(doc['map'],dict(op='undo',revision=applied['revision']),doc['base']['environment'])
            self.assertEqual(restored['nodes'],doc['nodes'])
            self.assertEqual({(e['from'],e['to'])for e in restored['edges']},{(e['from'],e['to'])for e in doc['edges']})
            self.assertGreaterEqual(restored['next_id'],1001)


if __name__=='__main__':unittest.main()
