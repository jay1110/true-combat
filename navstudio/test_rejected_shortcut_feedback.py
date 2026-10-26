"""Rejected automatic additions never authorize deletion of authored topology."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

from edit import Drafts, graph_hash
from repair import rejected_shortcut_feedback, filter_rejected_shortcuts
from server import Studio


def refresh_coverage(native):
    quarantined=sum(row.get('accepted')is False for row in native['nodes'])
    rejected=[e for e in native['edges']if e.get('accepted')is False and e['reason']!='quarantined_unreachable_endpoint']
    counts={}
    for edge in rejected:counts[edge['reason']]=counts.get(edge['reason'],0)+1
    native['quarantined_unreachable_nodes']=quarantined
    native['native_input_coverage']=dict(input_nodes=len(native['nodes']),native_floor_accepted=len(native['nodes']),native_floor_rejected=0,
        initial_directed_links=len(native['edges']),initial_accepted_links=len(native['edges'])-len(rejected),
        initial_rejected_links=len(rejected),geometry_rejections=[],ordinary_without_exit=[],
        connectivity_missing_input_nodes=quarantined,contracted_input_nodes=0,
        exported_input_nodes=len(native['nodes'])-quarantined,spawn_reachable_input_nodes=len(native['nodes'])-quarantined,
        link_rejections=[dict(reason=r,count=c)for r,c in counts.items()],
        capacity=dict(input_validation_complete=True,input_truncated=False))


class RejectedShortcutTests(unittest.TestCase):
    def fixture(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=Path(temp.name)
        s=Studio.__new__(Studio);s.work=root;s.drafts=Drafts(root/'drafts')
        nodes=[dict(id=i,position=[i*32.,0.,0.],radius=35,nav_flags=0,crouch=False,spawn=False,
                    topology_protected=False,locked=False,generated_guide=False)for i in(1,2,3)]
        s.drafts.start('obj_test',dict(nodes=nodes,edges=[{'from':a,'to':b}for a,b in((1,2),(2,1),(2,3),(3,2))]),dict(environment={}))
        s.draft_environment=lambda name:{};s.path=lambda name,suffix:root/(name+suffix)
        s.native_path=lambda name:root/'native.json';s.native_path('obj_test').write_text('{}')
        candidate=root/'candidate.txt';candidate.write_bytes(b'current candidate')
        s.candidate=lambda name:candidate;s.prepare_draft=Mock(return_value=candidate)
        s.file_hash=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()if path.is_file()else None
        s.candidate_identity=lambda path:dict(sha256=s.file_hash(path))
        s.proof_status=Mock(return_value=dict(inputs=dict(candidate='current',candidate_metadata='meta',bsp='bsp',game_module='module')))
        s.report_status=Mock(return_value=dict(current=True));s.update=lambda job,**kw:job.update(kw)
        s.action_load_map=Mock();s.quiet_generation=Mock();s.install=Mock()
        # Cross-GT reconciliation has its own complete asset-bound fixtures.
        s.reconcile_objective_anchors=Mock(return_value=None)
        s.repair_path_draft=Mock(return_value=None)
        s.current_native={}
        s.native=lambda name:s.current_native
        def propose():
            doc=s.drafts.get('obj_test');proposal=copy.deepcopy(doc)
            proposal['edges']+=[{'from':1,'to':3},{'from':3,'to':1}]
            current=s.drafts.apply_repair('obj_test',doc['revision'],{},proposal)
            receipt=dict(model='current_authored_mesh_corridor_proposals_v1',draft_revision=current['revision'],
                added_nodes=0,deleted_nodes=0,added_directed_edges=2,replaced_rejected_edges=[],required_crouch_flags=[],
                retained_node_ids=[1,2,3],routes=[dict(chain=[1,3],bidirectional=True,
                status='mesh_proposal_requires_native_revalidation',kind='same_component_local_gap')],
                source_native_report='previous-proof',proposal_graph_sha256=graph_hash(current),candidate=s.candidate_identity(candidate))
            s.path('obj_test','.repair-proposal.json').write_text(json.dumps(receipt))
            report=dict(map='obj_test',candidate_mode='manual_draft',generation_complete=False,bbox=dict(origin_above_feet=30),
                waypoint_metadata_contract='TCE_NODE_METADATA_1',
                nodes=[dict(id=n['id'],accepted=True,spawn_reachable=True,raw_position=n['position'][:2]+[30.],nav_flags=0,crouch=False,
                            native_input=dict(floor_accepted=True))for n in current['nodes']],
                edges=[dict(e,accepted=(e['from'],e['to'])not in((1,3),(3,1)),reason='steep_ground'if(e['from'],e['to'])in((1,3),(3,1))else'accepted')for e in current['edges']],
                traces=100,trace_budget=1000,native_input_coverage=dict(input_nodes=3,native_floor_accepted=3,native_floor_rejected=0,
                    initial_directed_links=6,initial_accepted_links=4,initial_rejected_links=2,geometry_rejections=[],
                    capacity=dict(input_validation_complete=True,input_truncated=False)))
            refresh_coverage(report)
            s.current_native=report;s.native_path('obj_test').write_text(json.dumps(report))
            return current,report,receipt
        return s,propose

    def test_only_rejected_new_edges_withdrawn_without_changing_inputs(self):
        s,propose=self.fixture();doc,report,receipt=propose();before=copy.deepcopy((doc,report,receipt))
        result=rejected_shortcut_feedback(doc,report,receipt);self.assertIsNotNone(result)
        proposal,evidence=result
        self.assertEqual(proposal['nodes'],doc['nodes']);self.assertEqual(len(proposal['edges']),4)
        self.assertEqual(evidence['retained_nodes'],3);self.assertTrue(evidence['native_revalidation_required'])
        self.assertEqual((doc,report,receipt),before)

    def test_authored_rejection_or_changed_nodes_capacity_receipt_cannot_roll_back(self):
        for mode in('floor','flags','truncated','stale_revision','manual_undo','node_patch','foreign_route'):
            s,propose=self.fixture();doc,native,receipt=copy.deepcopy(propose())
            if mode=='floor':native['nodes'][0]['accepted']=False
            elif mode=='flags':native['nodes'][0]['nav_flags']=64
            elif mode=='truncated':native['native_input_coverage']['capacity']['input_truncated']=True
            elif mode=='stale_revision':receipt['draft_revision']-=1
            elif mode=='manual_undo':doc['undo'][-1]['op']='connect'
            elif mode=='node_patch':doc['undo'][-1]['after']=[doc['nodes'][0]]
            else:receipt['routes'][0]['kind']='authored_action'
            with self.subTest(mode=mode):self.assertIsNone(rejected_shortcut_feedback(doc,native,receipt))

    def test_accepted_automatic_addition_is_retained(self):
        s,propose=self.fixture();doc,native,receipt=propose()
        next(e for e in native['edges']if(e['from'],e['to'])==(3,1)).update(accepted=True,reason='accepted')
        refresh_coverage(native)
        proposal,evidence=rejected_shortcut_feedback(doc,native,receipt)
        self.assertEqual(len(proposal['edges']),5)
        self.assertIn({'from':3,'to':1},proposal['edges'])
        self.assertEqual([r['edge']for r in evidence['rejected_edges']],[[1,3]])

    def mixed_patch(self):
        s,propose=self.fixture();doc,native,receipt=copy.deepcopy(propose())
        before=copy.deepcopy(doc['nodes'][1]);after=copy.deepcopy(before);after.update(nav_flags=64,crouch=True)
        doc['nodes'][1]=after;doc['undo'][-1].update(before=[before],after=[copy.deepcopy(after)])
        receipt.update(required_crouch_flags=[2],proposal_graph_sha256=graph_hash(doc))
        native['nodes'][1].update(nav_flags=64,crouch=True)
        for edge in native['edges']:
            if 2 in(edge['from'],edge['to']):edge['crouch']=True
        # An independently disproved old authored link remains a failure.
        native['edges'][0].update(accepted=False,reason='capsule_support_step_or_drop')
        for i in (4,5):
            node=copy.deepcopy(doc['nodes'][0]);node.update(id=i,position=[500.+i*32,0.,0.])
            doc['nodes'].append(node)
            native['nodes'].append(dict(id=i,accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn',
                raw_position=node['position'][:2]+[30.],nav_flags=0,crouch=False,
                native_input=dict(floor_accepted=True,status='missing_spawn_route')))
        for a,b in ((4,5),(5,4)):
            doc['edges'].append(dict(**{'from':a,'to':b}))
            native['edges'].append(dict(**{'from':a,'to':b},accepted=False,reason='quarantined_unreachable_endpoint'))
        receipt['retained_node_ids']=[n['id']for n in doc['nodes']]
        receipt['proposal_graph_sha256']=graph_hash(doc);refresh_coverage(native)
        source=copy.deepcopy(native);source['nodes'][1]['nav_flags']=0
        source['edges']=[edge for edge in source['edges']if(edge['from'],edge['to'])not in((1,3),(3,1))]
        refresh_coverage(source);raw=json.dumps(source).encode();receipt['source_native_report']=hashlib.sha256(raw).hexdigest()
        return doc,native,receipt,raw

    def test_rejected_new_links_roll_back_independently_retaining_proved_crouch_and_old_failures(self):
        doc,native,receipt,raw=self.mixed_patch();before=copy.deepcopy((doc,native,receipt))
        proposal,evidence=rejected_shortcut_feedback(doc,native,receipt,raw)
        self.assertEqual((doc,native,receipt),before);self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual([r['edge']for r in evidence['rejected_edges']],[[1,3],[3,1]])
        self.assertEqual(evidence['retained_required_crouch_flags'],[2])
        self.assertEqual(evidence['preserved_quarantined_source_ids'],[4,5])
        self.assertEqual([r['edge']for r in evidence['remaining_rejected_edges']],[[1,2],[4,5],[5,4]])
        self.assertIn({'from':1,'to':2},proposal['edges']);self.assertTrue(proposal['nodes'][1]['crouch'])
        self.assertFalse(native['generation_complete']);self.assertTrue(evidence['native_revalidation_required'])

    def test_mixed_patch_never_accepts_other_node_edits_or_unproved_crouch(self):
        for case in ('radius','position','uid','name','flags','old_flags','old_crouch','missing_before',
                     'extra_id','missing_required','source_hash','no_crouch_walk','action_crouch','native_action',
                     'new_action','unknown_new_reason','bad_quarantine_count','quarantine_floor','wrong_initial_counts'):
            doc,native,receipt,raw=self.mixed_patch();before=doc['undo'][-1]['before'][0];after=doc['undo'][-1]['after'][0]
            if case in('radius','position','uid','name'):
                key={'uid':'way_guid'}.get(case,case);value={'radius':36,'position':[64.,1.,0.],'uid':42,'name':'changed'}[case]
                doc['nodes'][1][key]=value;after[key]=value
            elif case=='flags':doc['nodes'][1]['nav_flags']=after['nav_flags']=65
            elif case=='old_flags':before['nav_flags']=1
            elif case=='old_crouch':before['crouch']=True
            elif case=='missing_before':doc['undo'][-1]['before']=[]
            elif case=='extra_id':receipt['required_crouch_flags'].append(1)
            elif case=='missing_required':receipt['required_crouch_flags']=[]
            elif case=='source_hash':receipt['source_native_report']='old'
            elif case=='no_crouch_walk':
                for edge in native['edges']:edge['crouch']=False
            elif case=='action_crouch':doc['nodes'][1]['action_roles']=after['action_roles']=[dict(kind='jump')]
            elif case=='native_action':native['traversal_routes']=[dict(landing=2)]
            elif case=='new_action':doc['nodes'][0]['nav_flags']=native['nodes'][0]['nav_flags']=512
            elif case=='unknown_new_reason':native['edges'][-4]['reason']='capsule_support_gap_or_drop';refresh_coverage(native)
            elif case=='bad_quarantine_count':native['quarantined_unreachable_nodes']-=1
            elif case=='quarantine_floor':native['nodes'][-1]['native_input']['floor_accepted']=False
            elif case=='wrong_initial_counts':native['native_input_coverage']['initial_rejected_links']+=2
            with self.subTest(case=case):self.assertIsNone(rejected_shortcut_feedback(doc,native,receipt,raw))

    def test_mixed_patch_source_report_must_prove_exact_previous_graph_and_crouch(self):
        for case in ('missing','stale_bytes','wrong_map','wrong_flags','unproved_crouch','source_action','source_edges','source_position'):
            doc,native,receipt,raw=self.mixed_patch();source=json.loads(raw)
            if case=='missing':raw=None
            elif case=='stale_bytes':raw+=b' '
            else:
                if case=='wrong_map':source['map']='obj_other'
                elif case=='wrong_flags':source['nodes'][1]['nav_flags']=64
                elif case=='unproved_crouch':source['nodes'][1]['crouch']=False
                elif case=='source_action':source['traversal_routes']=[dict(launch=2)]
                elif case=='source_edges':source['edges'].pop();refresh_coverage(source)
                elif case=='source_position':source['nodes'][1]['raw_position'][0]+=1
                raw=json.dumps(source).encode();receipt['source_native_report']=hashlib.sha256(raw).hexdigest()
            with self.subTest(case=case):self.assertIsNone(rejected_shortcut_feedback(doc,native,receipt,raw))

    def test_crouch_source_may_have_no_initial_rejects_before_new_bad_shortcut(self):
        doc,native,receipt,raw=self.mixed_patch();source=json.loads(raw)
        for edge in source['edges']:
            if edge['reason']!='quarantined_unreachable_endpoint':edge.update(accepted=True,reason='accepted')
        refresh_coverage(source);raw=json.dumps(source).encode();receipt['source_native_report']=hashlib.sha256(raw).hexdigest()
        self.assertIsNotNone(rejected_shortcut_feedback(doc,native,receipt,raw))

    def test_workflow_rechecks_retained_graph_instead_of_accepting_failed_way(self):
        s,propose=self.fixture();s.action_repair=Mock(side_effect=lambda job:(propose()and dict(repairs={})))
        observations=[]
        def check(job):
            observations.append(copy.deepcopy(s.drafts.get('obj_test')))
            if len(observations)==1:raise RuntimeError('rejected new edge')
            return dict(audit=dict(unresolved_nodes=[],fresh=True),proof='new proof')
        s.action_native_check=Mock(side_effect=check)
        result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual([len(d['edges'])for d in observations],[6,4])
        self.assertEqual(observations[0]['nodes'],observations[1]['nodes'])
        self.assertTrue(result['audit']['fresh']);self.assertFalse(result['installed']);s.install.assert_not_called()

    def test_persistent_failed_proposal_resumes_after_restart_without_new_proposals(self):
        s,propose=self.fixture();doc,_,_=propose();s.action_repair=Mock()
        s.action_native_check=Mock(return_value=dict(audit=dict(unresolved_nodes=[]),proof='current native fixture proof'))
        result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        s.action_repair.assert_not_called();s.action_native_check.assert_called_once()
        self.assertEqual(s.drafts.get('obj_test')['nodes'],doc['nodes'])
        self.assertEqual(len(s.drafts.get('obj_test')['edges']),4)
        self.assertIn('resumed_rejected_shortcuts',result['repair']['repairs'])

    def test_legacy_receipt_needs_exact_prior_proof_and_current_source_context(self):
        s,propose=self.fixture();_,_,receipt=propose()
        receipt.pop('proposal_graph_sha256');receipt.pop('candidate')
        s.path('obj_test','.repair-proposal.json').write_text(json.dumps(receipt))
        self.assertIsNone(s.recover_rejected_shortcuts(dict(map='obj_test')))
        way=s.native_path('obj_test').with_suffix('.way');way.write_bytes(b'preserved complete prior WAY')
        proof=dict(native_report='previous-proof',inputs=s.proof_status.return_value['inputs'],way=s.file_hash(way))
        (s.work/'obj_test.native-proof.json').write_text(json.dumps(proof))
        self.assertIsNotNone(s.recover_rejected_shortcuts(dict(map='obj_test')))

    def test_stale_or_changed_receipt_or_graph_never_mutates(self):
        s,propose=self.fixture();doc,_,receipt=propose();before=copy.deepcopy(doc)
        receipt['proposal_graph_sha256']='old';s.path('obj_test','.repair-proposal.json').write_text(json.dumps(receipt))
        self.assertIsNone(s.recover_rejected_shortcuts(dict(map='obj_test')))
        self.assertEqual(s.drafts.get('obj_test'),before)

    def test_negative_cache_scopes_sources_endpoints_and_preserves_authored_edges(self):
        s,propose=self.fixture();doc,native,receipt=propose();baseline,evidence=rejected_shortcut_feedback(doc,native,receipt)
        context=dict(bsp='same');memo=dict(context=context,rejected_edges=evidence['rejected_edges'])
        result,suppressed=filter_rejected_shortcuts(baseline,doc,memo,context)
        self.assertEqual(len(suppressed),2);self.assertEqual(result['edges'],baseline['edges'])
        self.assertEqual(filter_rejected_shortcuts(doc,doc,memo,context)[0]['edges'],doc['edges'])
        self.assertEqual(filter_rejected_shortcuts(baseline,doc,memo,dict(bsp='changed'))[1],[])
        changed=copy.deepcopy(baseline);changed['nodes'][0]['position'][0]+=1
        self.assertEqual(filter_rejected_shortcuts(changed,doc,memo,context)[1],[])


if __name__=='__main__':unittest.main()
