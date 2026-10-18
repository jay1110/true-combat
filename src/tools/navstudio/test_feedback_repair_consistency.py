"""Fresh mesh feedback and repeated draft repair share collision decisions.

Real L-shaped meshes exercise the bend search. No native acceptance, game,
project WAY or API is created by these pure proposal tests.
"""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOL_ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(TOOL_ROOT),str(TOOL_ROOT/'navmesh')]

from edit import Drafts
from repair import ORDINARY_COLLISION_REJECTIONS, propose
from waygraph import Mesh, add_native_feedback, movement_profile, ordinary_collision_rejections


class FeedbackRepairConsistencyTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        vertices=[[0,0,0],[256,0,0],[256,64,0],[64,64,0],[0,64,0],[64,256,0],[0,256,0]]
        polygons=[dict(vertices=[0,1,2,3,4],neighbors=[-1,-1,-1,1,-1]),
                  dict(vertices=[4,3,5,6],neighbors=[0,-1,-1,-1])]
        self.meshes=[]
        for name,height in (('stand',87),('duck',61)):
            path=self.root/(name+'.json')
            path.write_text(json.dumps(dict(coordinate_system='TCE_Z_up',
                profile=dict(radius=16,height=height,step=18,cell=4,cell_height=2),
                vertices=vertices,polygons=polygons)))
            self.meshes.append(Mesh(path))
        self.profile=movement_profile(*self.meshes)

    def fixture(self,reason):
        points=[[200,32,0],[32,200,0]]
        store=Drafts(self.root/'drafts')
        doc=store.start('obj_test',dict(nodes=[dict(id=i,position=p,radius=35)for i,p in enumerate(points)],
            edges=[dict(zip(('from','to'),pair))for pair in ((0,1),(1,0))]),dict(environment={'test':'pure'}))
        rows=[dict(id=i,raw_position=[*p[:2],30],position=[*p[:2],30],accepted=True,
            reason='external_native_validated_floor',nav_flags=0,crouch=False,
            native_input=dict(floor_accepted=True))for i,p in enumerate(points)]
        native=dict(map='obj_test',generation_complete=False,bbox=dict(half_width=16,stand_height=87,
            crouch_height=61,origin_above_feet=30),nodes=rows,
            edges=[dict(e,accepted=False,reason=reason)for e in doc['edges']])
        samples=[dict(point=p[:],polygon=i,component=0,crouch=False,spawn=False,
            kind='spatial_lattice_sample')for i,p in enumerate(points)]
        return copy.deepcopy(doc),native,samples

    def test_fresh_feedback_and_regeneration_repair_propose_same_real_capsule_bends(self):
        self.assertEqual(ORDINARY_COLLISION_REJECTIONS,ordinary_collision_rejections())
        for reason in sorted(ordinary_collision_rejections()):
            with self.subTest(reason=reason):
                doc,native,samples=self.fixture(reason);before=copy.deepcopy(native)
                path=self.root/'native.json';path.write_text(json.dumps(native))
                report=dict(map='obj_test',graph_policy='preserve_spatial_samples',spatial_sampling=dict(spacing=128))
                fresh_edges=add_native_feedback(samples,{(0,1)},report,*self.meshes,path)
                repaired,evidence=propose(doc,native,*self.meshes,{'map':'obj_test'})
                fresh_turns={tuple(n['point'])for n in samples[2:]}
                repaired_turns={tuple(n['position'])for n in repaired['nodes']if n['id']>=2}
                self.assertTrue(fresh_turns)
                self.assertEqual(repaired_turns,fresh_turns)
                self.assertEqual(evidence['replaced_rejected_edges'],[[0,1],[1,0]])
                self.assertTrue(evidence['native_revalidation_required'])
                self.assertFalse(report['native_feedback_repairs']['source_is_acceptance_certificate'])
                self.assertTrue(all(r['status']=='candidate_awaiting_native_revalidation'
                    for r in report['native_feedback_repairs']['routes']))
                self.assertEqual(native,before)
                # Fresh feedback preserves the literal rejected input; neither
                # route grants native acceptance or installation authority.
                self.assertIn((0,1),fresh_edges)
                self.assertFalse(any(e.get('accepted')for e in native['edges']))

    def test_budget_hazards_dynamic_ground_unknown_and_identity_never_become_collision_replacements(self):
        for reason in ('trace_budget','capsule_support_hazard','capsule_support_water',
                       'capsule_support_hurt','capsule_support_dynamic_ground','identity_mismatch','unknown'):
            with self.subTest(reason=reason):
                doc,native,samples=self.fixture(reason);before=copy.deepcopy(native)
                path=self.root/'native.json';path.write_text(json.dumps(native))
                report=dict(map='obj_test',graph_policy='preserve_spatial_samples',spatial_sampling=dict(spacing=128))
                add_native_feedback(samples,{(0,1)},report,*self.meshes,path)
                repaired,evidence=propose(doc,native,*self.meshes,{'map':'obj_test'})
                self.assertFalse(report['native_feedback_repairs']['routes'])
                self.assertEqual(evidence['replaced_rejected_edges'],[])
                self.assertTrue(all(e in repaired['edges']for e in doc['edges']))
                self.assertTrue(evidence['native_revalidation_required'])
                self.assertEqual(native,before)

    def test_undirected_repair_guides_require_both_actual_ordinary_directions(self):
        for kind,expected in (('one_way',0),('ordinary',1),('action_transition',0),('climb',0)):
            with self.subTest(kind=kind):
                doc,native,_=self.fixture('capsule_walk_sweep')
                native['edges'][0].update(accepted=True,reason='external_native_validated_walk')
                if kind!='one_way':
                    native['edges'][1].update(accepted=True,reason='external_native_validated_walk')
                if kind=='action_transition':
                    for edge in native['edges']:edge['reason']='manual_native_transition_live_pending'
                if kind=='climb':
                    doc['nodes'][0]['nav_flags']=512;native['nodes'][0]['nav_flags']=512
                original=copy.deepcopy(doc);source=copy.deepcopy(native)
                result,evidence=propose(doc,native,*self.meshes,{'map':'obj_test'})
                self.assertEqual(evidence['native_collision_proved_bidirectional_pairs'],expected)
                self.assertEqual(doc,original);self.assertEqual(native,source)
                if kind=='climb':self.assertEqual(result['nodes'][0],doc['nodes'][0])


if __name__=='__main__':unittest.main()
