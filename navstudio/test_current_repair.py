"""Repair current authored directions/IDs through real mesh corridors."""
import copy
import json
from pathlib import Path
import sys
import tempfile
from unittest import TestCase, main
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parent.parent/"navmesh"))
from waygraph import Mesh, SpatialGraph, movement_profile, objective_anchors,connect_native_components
from edit import Drafts
from repair import propose


class CurrentRepairTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        self.meshes=[]
        for kind,height in (("stand",87),("duck",61)):
            path=self.root/(kind+".json")
            path.write_text(json.dumps(dict(coordinate_system="TCE_Z_up",profile=dict(radius=16,height=height,step=18,cell=4,cell_height=2),
                vertices=[[0,0,0],[256,0,0],[256,64,0],[64,64,0],[0,64,0],[64,256,0],[0,256,0]],
                polygons=[dict(vertices=[0,1,2,3,4],neighbors=[-1,-1,-1,1,-1]),
                          dict(vertices=[4,3,5,6],neighbors=[0,-1,-1,-1])])) )
            self.meshes.append(Mesh(path))
        self.store=Drafts(self.root/"drafts");self.env={"bsp":"fixture"}
        self.store.start("obj_test",dict(nodes=[dict(id=3,position=[200,32,0],radius=35),dict(id=99,position=[32,200,0],radius=35),
                                                dict(id=120,position=[200,32,16],nav_flags=(1<<27)|(1<<55),radius=4)],
                                        edges=[dict(**{"from":3,"to":99}),dict(**{"from":99,"to":3}),dict(**{"from":120,"to":3})]),
                         dict(environment=self.env))
        self.native=dict(nodes=[dict(id=n["id"],accepted=True,crouch=False) for n in self.store.get("obj_test")["nodes"]],
                         edges=[dict(**e,accepted=False,reason="blocked_sweep") for e in self.store.get("obj_test")["edges"]])

    def test_valid_detached_floor_components_receive_real_mesh_bend_proposal(self):
        points=[[200,32,0],[32,200,0]]
        graph=SpatialGraph(self.meshes[0],self.meshes[1],128,
            [dict(point=p,polygon=i,component=0,crouch=False,spawn=False,kind="spatial_lattice_sample") for i,p in enumerate(points)],set())
        native={i:dict(accepted=False,nav_flags=0,reason="quarantined_no_validated_route_from_spawn",
                       native_input=dict(floor_accepted=True)) for i in range(2)}
        result=connect_native_components(graph,native,set(),2)
        self.assertEqual(len(result["proposals"]),1)
        self.assertGreaterEqual(result["proposals"][0]["exported_turns"],1)
        self.assertTrue(result["native_revalidation_required"])
        self.assertFalse(any(tuple(sorted((0,1)))==pair for pair in graph.edges))

    def test_mesh_bends_replace_rejected_edge_without_recreating_deleted_ids_or_action_flags(self):
        original=copy.deepcopy(self.store.get("obj_test"))
        result,evidence=propose(original,self.native,*self.meshes,{"map":"obj_test"})
        self.assertTrue(evidence["added_nodes"])
        self.assertEqual(evidence["deleted_nodes"],0)
        by_id={n["id"]:n for n in result["nodes"]}
        self.assertEqual(by_id[120],next(n for n in original["nodes"] if n["id"]==120))
        self.assertNotIn(100,by_id)  # Sparse IDs/deletions are not re-enumerated.
        self.assertTrue(all(n["id"]>=121 for n in result["nodes"] if n["id"] not in(3,99,120)))
        self.assertNotIn(dict(**{"from":3,"to":99}),result["edges"])
        self.assertIn(dict(**{"from":120,"to":3}),result["edges"])
        self.assertTrue(evidence["native_revalidation_required"])
        applied=self.store.apply_repair("obj_test",0,self.env,result)
        self.assertEqual(applied["revision"],1)
        undone=self.store.change("obj_test",dict(op="undo",revision=1),self.env)
        self.assertEqual(undone["nodes"],original["nodes"])
        self.assertEqual(undone["edges"],original["edges"])

    def corridor_duplicate_fixture(self, fields=None, offset=0):
        doc=copy.deepcopy(self.store.get("obj_test"))
        node=dict(id=1516,position=[32,32,0],radius=55,nav_flags=0,crouch=False,spawn=False,
                  topology_protected=False,locked=False,generated_guide=False,source_reason="authored_floor")
        node.update(fields or {});doc["nodes"].append(node);doc["next_id"]=1517
        native=copy.deepcopy(self.native);native["nodes"].append(dict(id=1516,accepted=True,crouch=node["crouch"]))
        def corridor(graph,a,b,force_bend=False):
            turn=copy.deepcopy(graph.nodes[a]);turn["point"]=[32+offset,32,0]
            turn["crouch"]=False;graph.nodes.append(turn)
            return dict(chain=[a,len(graph.nodes)-1,b],polygons=[0,1])
        return doc,native,corridor

    def test_exact_ordinary_turn_reuses_source_id_without_changing_any_existing_fields(self):
        for offset in (0,.0005):
            with self.subTest(offset=offset):
                doc,native,corridor=self.corridor_duplicate_fixture(offset=offset)
                original=copy.deepcopy(doc)
                with patch.object(SpatialGraph,"corridor",corridor):
                    result,evidence=propose(doc,native,*self.meshes,{"map":"obj_test"})
                by_id={n["id"]:n for n in result["nodes"]}
                self.assertEqual(set(by_id),{3,99,120,1516})
                self.assertEqual(by_id[1516],next(n for n in original["nodes"] if n["id"]==1516))
                self.assertEqual(evidence["added_nodes"],0)
                self.assertEqual(evidence["reused_ordinary_source_ids"],[1516])
                self.assertIn(dict(**{"from":3,"to":1516}),result["edges"])
                self.assertTrue(all(e["from"]!=e["to"] for e in result["edges"]))
                self.assertTrue(evidence["native_revalidation_required"])
                self.assertEqual(doc,original)

    def test_nearby_posture_or_action_aliases_are_never_coalesced(self):
        cases=[dict(radius=6),dict(nav_flags=512,generated_guide=True),dict(nav_flags=64,crouch=True),
               dict(editor_role=8),dict(action_properties={"tce_walk_drop":"1"}),
               dict(action_roles=[dict(kind="walk_drop",role="landing")]),dict(generated_guide=True)]
        for fields in cases:
            with self.subTest(fields=fields):
                doc,native,corridor=self.corridor_duplicate_fixture(fields)
                original=next(n for n in doc["nodes"] if n["id"]==1516)
                with patch.object(SpatialGraph,"corridor",corridor):
                    result,evidence=propose(doc,native,*self.meshes,{"map":"obj_test"})
                self.assertEqual(next(n for n in result["nodes"] if n["id"]==1516),original)
                self.assertNotIn(1516,evidence["reused_ordinary_source_ids"])
                self.assertGreater(evidence["added_nodes"],0)
        doc,native,corridor=self.corridor_duplicate_fixture(offset=.002)
        with patch.object(SpatialGraph,"corridor",corridor):
            result,evidence=propose(doc,native,*self.meshes,{"map":"obj_test"})
        self.assertNotIn(1516,evidence["reused_ordinary_source_ids"])
        self.assertGreater(evidence["added_nodes"],0)

    def test_a_missing_mesh_component_is_not_repaired_by_a_proximity_link(self):
        stand,duck=self.meshes
        duck.component[1]=1
        result,evidence=propose(self.store.get("obj_test"),self.native,stand,duck,{"map":"obj_test"})
        self.assertEqual(evidence["added_nodes"],0)
        self.assertEqual(result["edges"],self.store.get("obj_test")["edges"])

    def test_rejected_capsule_support_can_request_mesh_bend_to_plain_generated_floor(self):
        for reason in ('capsule_support_gap_or_drop','capsule_support_step_or_drop'):
            doc=copy.deepcopy(self.store.get('obj_test'));native=copy.deepcopy(self.native)
            helper=next(n for n in doc['nodes'] if n['id']==99)
            helper.update(generated_guide=True,editor_role=1,name='floor helper',way_guid=313,
                          properties={'purpose':'floor'},action_roles=[],action_properties={})
            next(n for n in native['nodes'] if n['id']==99).update(accepted=False,native_input=dict(floor_accepted=True))
            for edge in native['edges']:edge['reason']=reason
            before=copy.deepcopy(doc)
            result,evidence=propose(doc,native,*self.meshes,{'map':'obj_test'})
            with self.subTest(reason=reason):
                self.assertEqual(doc,before)
                self.assertEqual(next(n for n in result['nodes'] if n['id']==99),helper)
                self.assertIn([3,99],evidence['replaced_rejected_edges'])
                self.assertTrue(evidence['added_nodes'])
                self.assertTrue(evidence['native_revalidation_required'])
                self.assertNotIn({'from':3,'to':99},result['edges'])

    def test_generated_action_or_unproved_floor_is_not_an_ordinary_retry_endpoint(self):
        for case in ('role','flags','properties','action','door','native_action','no_floor','posture'):
            doc=copy.deepcopy(self.store.get('obj_test'));native=copy.deepcopy(self.native)
            helper=next(n for n in doc['nodes'] if n['id']==99)
            helper.update(generated_guide=True,editor_role=1,action_roles=[],action_properties={})
            source=next(n for n in native['nodes'] if n['id']==99)
            if case=='role':helper['editor_role']=8
            elif case=='flags':helper['nav_flags']=512
            elif case=='properties':helper['action_properties']={'tce_walk_drop':'1'}
            elif case=='action':helper['action_roles']=[dict(kind='walk_drop',role='landing')]
            elif case=='door':helper['door_recipe']={'role':2}
            elif case=='native_action':native['traversal_routes']=[dict(landing=99)]
            elif case=='no_floor':source.update(accepted=False,native_input=dict(floor_accepted=False))
            elif case=='posture':source['crouch']=True
            for edge in native['edges']:edge['reason']='capsule_support_gap_or_drop'
            result,evidence=propose(doc,native,*self.meshes,{'map':'obj_test'})
            with self.subTest(case=case):
                self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual(result['edges'],doc['edges'])
                self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_objective_contact_floor_seed_survives_ordinary_lattice_miss(self):
        graph=SpatialGraph(*self.meshes,128)
        graph.add(graph.sample([32,32,0],0,"spatial_lattice_sample"))
        geometry=dict(objective_volumes=[dict(entity=12,classname="trigger_objective_info",mins=[160,8,20],maxs=[224,56,48])])
        anchors=objective_anchors(graph,geometry,movement_profile(*self.meshes))
        self.assertEqual(len(anchors),1)
        node=graph.nodes[anchors[0]["node"]]
        self.assertTrue(160<node["point"][0]<224)
        self.assertTrue(8<node["point"][1]<56)
        self.assertEqual(node["point"][2],0)
        self.assertTrue(node["topology_protected"])
        self.assertTrue(anchors[0]["native_revalidation_required"])

    def test_bomb_crate_generates_native_pending_floor_outside_trigger_perimeter(self):
        graph=SpatialGraph(*self.meshes,128)
        graph.add(graph.sample([128,32,0],0,"spatial_lattice_sample"))
        geometry=dict(objective_volumes=[dict(entity=12,classname="trigger_objective_info",
            mins=[96,8,8],maxs=[160,56,52],target_class="func_explosive",
            target_bounds=[[112,8,8],[144,56,52]],properties={"spawnflags":"130"})])
        anchors=objective_anchors(graph,geometry,movement_profile(*self.meshes))
        self.assertEqual(len(anchors),2)
        node=graph.nodes[anchors[0]["node"]]
        self.assertLess(node["point"][0],96)
        self.assertEqual(node["point"][2],0)
        self.assertTrue(node["topology_protected"])
        self.assertEqual(anchors[0]["contact_model"],"tce_actor_plant_box")
        self.assertTrue(anchors[0]["native_revalidation_required"])
        self.assertTrue(all(a["native_revalidation_required"] for a in anchors))
        self.assertGreater(graph.nodes[anchors[1]["node"]]["point"][0],160)

    def test_capture_uses_flagonly_target_capsule_not_large_oid_or_solid_centre(self):
        from navmesh.objective_contact import capture_contact
        graph=SpatialGraph(*self.meshes,128)
        graph.add(graph.sample([32,32,0],0,"spatial_lattice_sample"))
        contact=[[96,8,8],[160,56,64]]
        geometry=dict(objective_volumes=[dict(entity=12,classname="trigger_objective_info",
            mins=[0,0,0],maxs=[256,64,100],target_class="trigger_flagonly_multiple",
            target_bounds=contact,properties={"spawnflags":"65"})])
        anchors=objective_anchors(graph,geometry,movement_profile(*self.meshes))
        self.assertGreaterEqual(len(anchors),2)
        self.assertTrue(all(a['node']!=0 for a in anchors))
        self.assertTrue(any(graph.nodes[a['node']]['point'][0]<96 for a in anchors))
        self.assertTrue(any(graph.nodes[a['node']]['point'][0]>160 for a in anchors))
        for a in anchors:
            node=graph.nodes[a['node']]
            self.assertEqual(a['contact_model'],'flagonly_actor_capsule')
            self.assertTrue(capture_contact(node['point'],contact,16,87))
            self.assertTrue(node['topology_protected'])
            self.assertTrue(a['native_revalidation_required'])

    def test_capture_missing_real_target_bounds_is_reported_not_oid_fallback(self):
        graph=SpatialGraph(*self.meshes,128)
        geometry=dict(objective_volumes=[dict(entity=12,classname="trigger_objective_info",
            mins=[0,0,0],maxs=[256,64,100],target_class="trigger_flagonly_multiple")])
        anchors=objective_anchors(graph,geometry,movement_profile(*self.meshes))
        self.assertEqual(anchors[0]['status'],'missing_flagonly_contact_bounds')
        self.assertFalse(graph.nodes)

    def test_small_offset_bomb_target_gets_body_clear_hint_reachable_face_samples(self):
        from navmesh.objective_contact import plant_contact
        graph=SpatialGraph(*self.meshes,128)
        contact=[[0,0,-8],[256,64,52]];target=[[80,8,-8],[128,56,40]]
        geometry=dict(objective_volumes=[dict(entity=12,classname="trigger_objective_info",
            mins=contact[0],maxs=contact[1],target_class="func_explosive",target_bounds=target,
            properties={"spawnflags":"130"})])
        anchors=objective_anchors(graph,geometry,movement_profile(*self.meshes))
        points=[graph.nodes[a['node']]['point'] for a in anchors]
        # The TOI centre128 +32 would be 32 beyond the crate, while -32 is
        # inside it. Its actual target faces need independently sized samples.
        self.assertTrue(any(p[0]<80 for p in points))
        self.assertTrue(any(p[0]>128 for p in points))
        self.assertTrue(all(plant_contact(p,30,contact,16,87,target)for p in points))
        native_target=[[v-2 for v in target[0]],[v+2 for v in target[1]]]
        native_contact=[[v-2 for v in contact[0]],[v+2 for v in contact[1]]]
        self.assertTrue(all(plant_contact(p,30,native_contact,16,87,native_target)for p in points))
        # Exactly BSP face+radius+4 fails against linked native bounds by
        # .125; the new half-unit proposal keeps the same strict contract.
        self.assertFalse(plant_contact([60,32,0],30,native_contact,16,87,native_target))
        self.assertTrue(all(a['native_revalidation_required']for a in anchors))



if __name__=="__main__":main()
