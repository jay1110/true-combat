"""Native feedback is a proposal guide, never a collision bypass."""
import copy
import json
import unittest
from unittest.mock import patch

import test_movement_profile as profiles
from waygraph import (SpatialGraph, add_native_feedback, clean_candidate, complete_native_stubs,
                      connect_native_isolates,
                      movement_profile, native_ordinary_pairs)


class NativeFeedbackTests(unittest.TestCase):
    setUp = profiles.MovementProfileTests.setUp
    meshes = profiles.MovementProfileTests.meshes
    native = profiles.MovementProfileTests.native
    def sample_nodes(self):
        return [dict(point=p, polygon=0, component=0, crouch=False, spawn=False,
                     kind='spatial_lattice_sample')
                for p in [[100,100,0],[80,100,0],[230,100,0],[30,100,0]]]

    def edge(self, a, b, reason='external_native_validated_walk', accepted=True):
        return dict(**{'from':a,'to':b}, reason=reason, accepted=accepted)

    def test_only_bidirectional_native_ordinary_proofs_are_reused(self):
        stand,duck=self.meshes(); nodes=self.sample_nodes()
        native=self.native(movement_profile(stand,duck),nodes)
        native['edges']=[self.edge(0,1),self.edge(1,0),self.edge(1,2),
                         self.edge(0,3,'blocked_sweep',False),self.edge(3,0),
                         self.edge(2,3,'quarantined_unreachable_endpoint',False),
                         self.edge(3,2,'quarantined_unreachable_endpoint',False)]
        self.assertEqual(native_ordinary_pairs(native,4),{(0,1),(2,3)})
        native['nodes'][3]['nav_flags']=8
        self.assertEqual(native_ordinary_pairs(native,4),{(0,1)})

    def test_unproved_candidate_detour_does_not_suppress_bend(self):
        stand,duck=self.meshes(); nodes=self.sample_nodes(); before=copy.deepcopy(nodes)
        native=self.native(movement_profile(stand,duck),nodes)
        native['generation_complete']=False
        native['edges']=[self.edge(0,2,'blocked_sweep',False)]
        path=self.root/'native.json'; path.write_text(json.dumps(native))
        report=dict(map='test',graph_policy='preserve_spatial_samples',spatial_sampling=dict(spacing=128))
        original={(0,1),(1,2),(0,2)}
        with patch.object(SpatialGraph,'corridor',return_value=None) as corridor:
            result=add_native_feedback(nodes,original,report,stand,duck,path)
        corridor.assert_called_once_with(0,2,force_bend=True)
        self.assertEqual(set(result),original); self.assertEqual(nodes,before)
        evidence=report['native_feedback_repairs']
        self.assertFalse(evidence['source_generation_complete'])
        self.assertFalse(evidence['source_is_acceptance_certificate'])
        self.assertTrue(evidence['existing_links_preserved'])

    def test_short_stub_continues_forward_but_not_backwards(self):
        stand,duck=self.meshes(); nodes=self.sample_nodes()
        native={n['id']:n for n in self.native(movement_profile(stand,duck),nodes)['nodes']}
        pairs={(0,1),(1,2),(1,3)}; graph=SpatialGraph(stand,duck,128,nodes,pairs)
        proposals=complete_native_stubs(graph,native,pairs,set())
        self.assertTrue(any(p['edge']==[0,2] for p in proposals))
        self.assertNotIn((0,3),graph.edges)

    def test_native_only_witness_is_not_exported_as_recast_certificate(self):
        stand,duck=self.meshes();nodes=self.sample_nodes()[:3]
        for i,n in enumerate(nodes):n['point']=[20+i*100,100,0];n['id']=i
        native=self.native(movement_profile(stand,duck),nodes)
        native['edges']=[self.edge(0,1),self.edge(1,0),self.edge(1,2),self.edge(2,1)]
        path=self.root/'native.json';path.write_text(json.dumps(native))
        report=dict(map='test',graph_policy='preserve_spatial_samples',spatial_sampling=dict(spacing=128))
        original={(0,1)};before=copy.deepcopy(nodes)
        visible=SpatialGraph.visible
        def mesh_certificate(graph,a,b):
            if {a['point'][0],b['point'][0]}=={120,220}:return False
            return visible(graph,a,b)
        with patch.object(SpatialGraph,'visible',mesh_certificate):
            result=add_native_feedback(nodes,original,report,stand,duck,path)
            self.assertEqual(set(result),original)
            self.assertEqual(set(clean_candidate(nodes,result,report,stand,duck)),original)
            with self.assertRaisesRegex(ValueError,'Recast floor/stance certificate'):
                clean_candidate(nodes,[(0,1),(1,2)],report,stand,duck)
        self.assertEqual(nodes,before)
        self.assertEqual(report['native_feedback_repairs']['native_guidance_edges_not_reexported'],[(1,2)])

    def test_rejected_action_wrong_floor_and_blocked_mesh_stay_excluded(self):
        for mode in ['rejected','height','component','posture','mesh','unaccepted']:
            with self.subTest(mode=mode):
                stand,duck=self.meshes(); nodes=self.sample_nodes()
                native={n['id']:n for n in self.native(movement_profile(stand,duck),nodes)['nodes']}
                if mode=='height': nodes[2]['point'][2]=30
                if mode=='component': nodes[2]['component']=1
                if mode=='posture': nodes[2]['crouch']=True
                if mode=='unaccepted': native[2]['accepted']=False
                pairs={(0,1),(1,2),(1,3)}; graph=SpatialGraph(stand,duck,128,nodes,pairs)
                rejected={(0,2)} if mode=='rejected' else set()
                if mode=='mesh': graph.visible=lambda a,b:False
                complete_native_stubs(graph,native,pairs,rejected)
                self.assertNotIn((0,2),graph.edges)

    def test_isolated_floor_gets_mesh_bend_and_ignores_generated_native_ids(self):
        stand,duck=self.meshes();nodes=self.sample_nodes()[:3];before=copy.deepcopy(nodes)
        native={n['id']:n for n in self.native(movement_profile(stand,duck),nodes)['nodes']}
        native[3]=dict(native[1],id=3)  # Native generated ID, not a base candidate.
        edges=[self.edge(1,2),self.edge(2,1),self.edge(3,1)]
        graph=SpatialGraph(stand,duck,128,nodes,{(1,2)})
        evidence=connect_native_isolates(graph,native,edges,3)
        self.assertEqual(evidence['isolated_base_nodes'],[0])
        self.assertTrue(evidence['proposals'])
        self.assertTrue(evidence['native_revalidation_required'])
        self.assertEqual(nodes[:3],before)
        self.assertTrue(any(0 in edge for edge in graph.edges))
        self.assertTrue(all(graph.visible(nodes[a],nodes[b]) for a,b in graph.edges))
        self.assertTrue(all(route['edge'][1]<3 for route in evidence['proposals']))

    def test_isolate_corridor_never_bypasses_mesh_or_posture_or_height(self):
        for mode in ('mesh','posture','height','component','action','unaccepted'):
            with self.subTest(mode=mode):
                stand,duck=self.meshes();nodes=self.sample_nodes()[:3]
                native={n['id']:n for n in self.native(movement_profile(stand,duck),nodes)['nodes']}
                for b in (1,2):
                    if mode=='posture':nodes[b]['crouch']=True
                    if mode=='height':nodes[b]['point'][2]=30
                    if mode=='component':nodes[b]['component']=1
                    if mode=='action':native[b]['nav_flags']=512
                    if mode=='unaccepted':native[b]['accepted']=False
                graph=SpatialGraph(stand,duck,128,nodes,{(1,2)})
                if mode=='mesh':graph.visible=lambda a,b:False
                before=copy.deepcopy(nodes)
                evidence=connect_native_isolates(graph,native,[self.edge(1,2),self.edge(2,1)],3)
                self.assertFalse(evidence['proposals']);self.assertEqual(nodes,before)
                self.assertEqual(graph.edges,{(1,2)})

    def test_crouch_only_isolate_uses_duck_corridor(self):
        stand,duck=self.meshes();nodes=self.sample_nodes()[:3]
        stand.locate=lambda point: None  # No standing clearance anywhere in this corridor.
        for node in nodes: node['crouch']=True
        native={n['id']:n for n in self.native(movement_profile(stand,duck),nodes)['nodes']}
        for node in native.values(): node['nav_flags']=64
        graph=SpatialGraph(stand,duck,128,nodes,{(1,2)})
        evidence=connect_native_isolates(graph,native,[self.edge(1,2),self.edge(2,1)],3)
        self.assertEqual(evidence['isolated_base_nodes'],[0])
        self.assertTrue(evidence['proposals'])
        self.assertTrue(all(node['crouch'] for node in nodes))
        self.assertTrue(all(graph.visible(nodes[a],nodes[b]) for a,b in graph.edges))


if __name__=='__main__':
    import unittest
    unittest.main()
