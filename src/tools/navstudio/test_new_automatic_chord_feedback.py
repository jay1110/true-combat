"""Long ordinary alternates apply only to a byte-bound new automatic chord."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path[:0]=[str(Path(__file__).resolve().parent.parent),str(Path(__file__).resolve().parent.parent/'navmesh')]
from edit import graph_hash
from repair import rejected_ordinary_edge_feedback
import test_rejected_ordinary_feedback as ordinary_fixture

digest=ordinary_fixture.digest


class NewAutomaticChordTests(unittest.TestCase):
    def fixture(self,mixed=True,sink=True):
        fixture=ordinary_fixture.RejectedOrdinaryFeedbackTests()
        chain=[1,*range(3,18),2]
        positions={1:[0.,0.,0.],2:[64.,0.,0.]}
        positions.update({i:[64.*(i-3),2048.,0.]for i in range(3,18)})
        pairs=list(zip(chain,chain[1:]));pairs+=list((b,a)for a,b in pairs)
        if sink:positions[90]=[0.,64.,80.];pairs.append((1,90))
        doc,source,_=fixture.fixture(positions,pairs,{(-1,-1):'unused'})
        for row in source['nodes']:row['radius']=35
        fixture.bind(doc,source)
        if sink:source['native_input_coverage']['ordinary_without_exit']=[90]
        source_raw=json.dumps(source).encode();old=copy.deepcopy(doc)
        doc['revision']+=1;doc['saved_revision']=doc['revision']
        added=[(1,2),(2,1)];after=[]
        native=copy.deepcopy(source)
        for a,b in added:
            doc['edges'].append({'from':a,'to':b})
            native['edges'].append(dict(**{'from':a,'to':b},accepted=False,crouch=True,reason='blocked_clearance'))
        if mixed:
            node=copy.deepcopy(doc['nodes'][0]);node.update(id=100,way_guid=100,position=[-32.,0.,0.],
                generated_guide=True,source_reason='automatic_repair_corridor')
            doc['nodes'].append(node);after=[copy.deepcopy(node)];doc['next_id']=101
            row=copy.deepcopy(native['nodes'][0]);row.update(id=100,import_uid=100,raw_position=[-32.,0.,30.],position=[-32.,0.,30.],editor_role=1)
            native['nodes'].append(row)
            for a,b in((1,100),(100,1)):
                doc['edges'].append({'from':a,'to':b});added.append((a,b))
                native['edges'].append(dict(**{'from':a,'to':b},accepted=True,crouch=False,reason='manual_native_transition_live_pending'))
        doc['undo']=[dict(op='automatic_repair',before=[],after=after,added=[list(p)for p in added],removed=[])]
        bound=fixture.bind(doc,native)
        if sink:native['native_input_coverage']['ordinary_without_exit']=[90]
        raw=json.dumps(native).encode();bound['native_report_bytes']=raw
        bound['report_status']['native_report']=bound['attempt']['native_report']=digest(raw)
        receipt=dict(model='current_authored_mesh_corridor_proposals_v1',source_native_report=digest(source_raw),
            proposal_graph_sha256=graph_hash(doc),draft_revision=doc['revision'],deleted_nodes=0,replaced_rejected_edges=[],
            added_nodes=int(mixed),added_directed_edges=len(added),retained_node_ids=[n['id']for n in old['nodes']],
            candidate=dict(name=bound['report_status']['candidate'],sha256=bound['inputs']['candidate'],metadata=bound['inputs']['candidate_metadata']),
            routes=[dict(kind='same_component_local_gap',status='mesh_proposal_requires_native_revalidation',edge=[1,2],chain=[1,2],bidirectional=True)])
        self.bind_origin(bound,receipt,source)
        return doc,native,bound,receipt,source

    def bind_origin(self,bound,receipt,source):
        sr=json.dumps(source).encode();receipt['source_native_report']=digest(sr);rr=json.dumps(receipt).encode()
        bound.update(automatic_shortcut_receipt_bytes=rr,automatic_shortcut_receipt_sha256=digest(rr),
            automatic_shortcut_source_native_bytes=sr,automatic_shortcut_source_native_sha256=digest(sr))

    def test_only_new_machine_chord_uses_long_current_ordinary_alternate_with_mixed_helpers_and_sink(self):
        for mixed in(False,True):
            for sink in(False,True):
                with self.subTest(mixed=mixed,sink=sink):
                    doc,native,bound,receipt,source=self.fixture(mixed,sink);before=copy.deepcopy((doc,native,bound))
                    proposal,evidence=rejected_ordinary_edge_feedback(doc,native,bound)
                    self.assertEqual((doc,native,bound),before);self.assertEqual(proposal['nodes'],doc['nodes'])
                    self.assertEqual({tuple(r['edge'])for r in evidence['withdrawn_edges']},{(1,2),(2,1)})
                    self.assertEqual(proposal['edges'],[e for e in doc['edges']if(e['from'],e['to'])not in((1,2),(2,1))])
                    self.assertEqual(evidence['remaining_ordinary_without_exit'],[90]if sink else[])
                    for row in evidence['withdrawn_edges']:
                        self.assertEqual(row['kind'],'new_automatic_local_gap_ordinary_walk')
                        self.assertGreater(row['hops'],12);self.assertGreater(row['length_units'],2048)
                        self.assertLessEqual(row['hops'],row['maximum_hops'])
                        self.assertEqual(row['automatic_addition_origin']['receipt_sha256'],bound['automatic_shortcut_receipt_sha256'])
                    self.assertTrue(evidence['native_revalidation_required'])

    def test_old_authored_edge_and_unbound_or_changed_receipts_keep_existing_distance_limits(self):
        cases=('missing','raw_receipt','raw_source','receipt_hash','source_hash','source_reference','graph','revision',
               'candidate','manual_patch','redo','not_added','source_present','source_moved','source_flag','source_name',
               'source_radius','source_incomplete','source_sinks','node_changed','route_kind','route_status','route_chain',
               'route_edge','retained_ids','added_count','deleted','replaced')
        for case in cases:
            doc,native,bound,receipt,source=self.fixture()
            if case=='missing':
                for key in list(bound):
                    if key.startswith('automatic_shortcut_'):bound.pop(key)
            elif case in('raw_receipt','raw_source','receipt_hash','source_hash'):
                key={'raw_receipt':'automatic_shortcut_receipt_bytes','raw_source':'automatic_shortcut_source_native_bytes',
                     'receipt_hash':'automatic_shortcut_receipt_sha256','source_hash':'automatic_shortcut_source_native_sha256'}[case]
                bound[key]+=b' 'if case.startswith('raw_')else'0'
            else:
                if case=='source_reference':receipt['source_native_report']='0'*64
                elif case=='graph':receipt['proposal_graph_sha256']='0'*64
                elif case=='revision':receipt['draft_revision']-=1
                elif case=='candidate':receipt['candidate']['sha256']='0'*64
                elif case=='manual_patch':doc['undo'][-1]['op']='manual_edit'
                elif case=='redo':doc['redo']=[dict(op='manual_edit')]
                elif case=='not_added':doc['undo'][-1]['added'].remove([1,2])
                elif case=='source_present':source['edges'].append(dict(**{'from':1,'to':2},accepted=True,reason='accepted'))
                elif case=='source_moved':source['nodes'][0]['raw_position'][0]+=1
                elif case=='source_flag':source['nodes'][0]['nav_flags']=64
                elif case=='source_name':source['nodes'][0]['name']='changed'
                elif case=='source_radius':source['nodes'][0]['radius']=36
                elif case=='source_incomplete':source['native_input_coverage']['capacity']['input_validation_complete']=False
                elif case=='source_sinks':source['native_input_coverage']['ordinary_without_exit']=[]
                elif case=='node_changed':doc['undo'][-1]['before']=[copy.deepcopy(doc['nodes'][0])];doc['undo'][-1]['before'][0]['radius']=34
                elif case=='route_kind':receipt['routes'][0]['kind']='authored_route'
                elif case=='route_status':receipt['routes'][0]['status']='unverified'
                elif case=='route_chain':receipt['routes'][0]['chain']=[1,3,2]
                elif case=='route_edge':receipt['routes'][0]['edge']=[2,1]
                elif case=='retained_ids':receipt['retained_node_ids'].pop()
                elif case=='added_count':receipt['added_nodes']+=1
                elif case=='deleted':receipt['deleted_nodes']=1
                elif case=='replaced':receipt['replaced_rejected_edges']=[[1,2]]
                self.bind_origin(bound,receipt,source)
                if case=='source_reference':
                    receipt['source_native_report']='0'*64;raw=json.dumps(receipt).encode()
                    bound['automatic_shortcut_receipt_bytes']=raw;bound['automatic_shortcut_receipt_sha256']=digest(raw)
            with self.subTest(case=case):self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,bound))

    def test_long_alternate_still_excludes_unproved_actions_and_rejected_or_unknown_edges(self):
        for case in('action','unknown','rejected','flags','crouch_unowned'):
            doc,native,bound,receipt,source=self.fixture()
            # This essential edge is the only accepted forward route from1.
            row=next(e for e in native['edges']if(e['from'],e['to'])==(1,3))
            if case=='action':native['traversal_routes']=[dict(kind='jump',exported=True,launch=3,landing=4)]
            elif case=='unknown':row['reason']='unverified_walk'
            elif case=='rejected':row.update(accepted=False,reason='blocked_sweep')
            elif case=='flags':row['flags']=1
            else:row['crouch']=True
            raw=json.dumps(native).encode();bound['native_report_bytes']=raw
            bound['report_status']['native_report']=bound['attempt']['native_report']=digest(raw)
            result=rejected_ordinary_edge_feedback(doc,native,bound)
            with self.subTest(case=case):
                if result:self.assertNotIn([1,2],[r['edge']for r in result[1]['withdrawn_edges']])


if __name__=='__main__':unittest.main()
