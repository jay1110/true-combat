"""Small missing connections inside one accepted graph; no game/API calls."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'navmesh'))
from waygraph import Mesh, BspPointFilters
from edit import Drafts
from repair import propose


class LocalGapRepairTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)

    def meshes(self,vertices=None,polygons=None):
        vertices=vertices or [[-16,-16,0],[272,-16,0],[272,272,0],[-16,272,0]]
        polygons=polygons or [dict(vertices=[0,1,2,3],neighbors=[-1]*4)]
        result=[]
        for name,height in (('stand',87),('duck',61)):
            path=self.root/(name+'.json')
            path.write_text(json.dumps(dict(coordinate_system='TCE_Z_up',
                profile=dict(radius=16,height=height,step=18,cell=4,cell_height=2),
                vertices=vertices,polygons=polygons)))
            result.append(Mesh(path))
        return result

    def fixture(self,points=None):
        points=points or [(3,[0,0,0]),(7,[0,224,0]),(18,[128,224,0]),(99,[128,0,0])]
        store=Drafts(self.root/'drafts')
        chain=[identity for identity,_ in points]
        edges=[dict(zip(('from','to'),pair)) for a,b in zip(chain,chain[1:]) for pair in ((a,b),(b,a))]
        doc=store.start('obj_test',dict(nodes=[dict(id=i,position=p,radius=35,name='node_'+str(i),
                          facing=[0,1,0],way_guid=1000+i) for i,p in points],edges=edges),dict(environment={'test':'gap'}))
        return copy.deepcopy(doc),self.native(doc)

    @staticmethod
    def native(doc):
        return dict(bbox=dict(origin_above_feet=30),generation_complete=True,
            nodes=[dict(id=n['id'],accepted=True,crouch=n['crouch'],nav_flags=n['nav_flags'],
                        position=[n['position'][0],n['position'][1],n['position'][2]+30]) for n in doc['nodes']],
            edges=[dict(e,accepted=True,reason='external_native_validated_walk') for e in doc['edges']])

    def test_small_same_component_gap_is_measured_and_repaired_without_node_changes(self):
        doc,native=self.fixture();before=copy.deepcopy(doc)
        result,evidence=propose(doc,native,*self.meshes(),{'map':'obj_test'})
        self.assertEqual(doc,before);self.assertEqual(result['nodes'],before['nodes'])
        self.assertEqual(result['next_id'],before['next_id']);self.assertEqual(evidence['deleted_nodes'],0)
        self.assertEqual(evidence['added_nodes'],0);self.assertEqual(evidence['added_directed_edges'],2)
        self.assertEqual(evidence['local_gap_search']['proposed_routes'],1)
        route=next(r for r in evidence['routes'] if r.get('kind')=='same_component_local_gap')
        self.assertEqual(route['edge'],[3,99]);self.assertEqual(route['detour_before'],[576,576])
        self.assertEqual(route['proposed_distance'],128);self.assertTrue(route['bidirectional'])
        self.assertTrue(evidence['native_revalidation_required'])
        self.assertEqual({(e['from'],e['to']) for e in result['edges']}-{(e['from'],e['to']) for e in doc['edges']},{(3,99),(99,3)})
        again,second=propose(result,self.native(result),*self.meshes(),{'map':'obj_test'})
        self.assertEqual(again,result);self.assertEqual(second['local_gap_search']['proposed_routes'],0)

    def test_short_existing_detours_do_not_densify_an_open_room(self):
        doc,native=self.fixture([(3,[0,0,0]),(7,[64,32,0]),(99,[128,0,0])])
        result,evidence=propose(doc,native,*self.meshes(),{'map':'obj_test'})
        self.assertEqual(result,doc);self.assertEqual(evidence['added_directed_edges'],0)

    def test_wall_corridor_is_not_replaced_by_a_straight_proximity_chord(self):
        vertices=[[0,0,0],[64,0,0],[64,192,0],[64,256,0],[0,256,0],
                  [192,192,0],[192,256,0],[192,0,0],[256,0,0],[256,256,0]]
        polygons=[dict(vertices=[0,1,2,3,4],neighbors=[-1,-1,1,-1,-1]),
                  dict(vertices=[2,5,6,3],neighbors=[-1,2,-1,0]),
                  dict(vertices=[7,8,9,6,5],neighbors=[-1,-1,-1,1,-1])]
        meshes=self.meshes(vertices,polygons)
        doc,native=self.fixture([(3,[32,32,0]),(7,[32,224,0]),(18,[224,224,0]),(99,[224,32,0])])
        result,evidence=propose(doc,native,*meshes,{'map':'obj_test'})
        self.assertEqual(result,doc);self.assertEqual(evidence['local_gap_search']['proposed_routes'],0)

    def test_intermediate_hazard_and_dynamic_door_block_the_gap_without_partial_nodes(self):
        for kind in ('water','door'):
            with self.subTest(kind=kind):
                doc,native=self.fixture();meshes=self.meshes();geometry={'map':'obj_test'}
                if kind=='door':geometry['dynamic_obstacles']=[dict(entity=30,classname='func_door',
                    dynamic_obstacle=True,mins=[56,-32,0],maxs=[72,40,128])]
                def allows(_self,p,polygon=None,kind='query'):
                    return not (40<p[0]<88 and p[1]<40)
                if kind=='water':
                    with patch.object(BspPointFilters,'allows',autospec=True,side_effect=allows):
                        result,evidence=propose(doc,native,*meshes,geometry)
                else:result,evidence=propose(doc,native,*meshes,geometry)
                self.assertEqual(result,doc);self.assertEqual(evidence['local_gap_search']['proposed_routes'],0)

    def test_one_way_actions_properties_and_unaccepted_edges_never_become_local_walk_proof(self):
        cases=[dict(nav_flags=1<<27,generated_guide=True),dict(nav_flags=1<<45),dict(editor_role=8),
               dict(action_roles=[dict(kind='door',role='entry')]),dict(action_properties={'paththrough':'switch:door'}),
               'oneway','rejected','stale_position','native_action']
        for case in cases:
            with self.subTest(case=case):
                doc,native=self.fixture();meshes=self.meshes()
                if isinstance(case,dict):doc['nodes'][0].update(case);native=self.native(doc)
                elif case=='oneway':
                    doc['edges']=[e for e in doc['edges'] if (e['from'],e['to'])!=(7,3)];native=self.native(doc)
                elif case=='rejected':native['edges'][0].update(accepted=False,reason='disabled_authored_edge')
                elif case=='stale_position':native['nodes'][0]['position'][0]+=2
                elif case=='native_action':native['traversal_routes']=[dict(kind='jump',launch=3,landing=7)]
                before=copy.deepcopy(doc)
                result,evidence=propose(doc,native,*meshes,{'map':'obj_test'})
                self.assertEqual(result,doc);self.assertEqual(doc,before)
                self.assertEqual(evidence['local_gap_search']['proposed_routes'],0)

    def test_other_floor_is_not_a_small_same_floor_gap(self):
        doc,native=self.fixture();doc['nodes'][-1]['position'][2]=64;native=self.native(doc)
        result,evidence=propose(doc,native,*self.meshes(),{'map':'obj_test'})
        self.assertEqual(result,doc);self.assertEqual(evidence['local_gap_search']['proposed_routes'],0)

    def test_real_mesh_bend_reuses_existing_native_anchor_and_preserves_uid_metadata(self):
        vertices=[[0,0,0],[256,0,0],[256,64,0],[64,64,0],[0,64,0],[64,256,0],[0,256,0]]
        polygons=[dict(vertices=[0,1,2,3,4],neighbors=[-1,-1,-1,1,-1]),
                  dict(vertices=[4,3,5,6],neighbors=[0,-1,-1,-1])]
        meshes=self.meshes(vertices,polygons)
        doc,native=self.fixture([(3,[200,16,0]),(7,[16,16,0]),(11,[240,16,0]),
                                 (18,[32,64,0]),(24,[32,240,0]),(39,[32,96,0]),(99,[32,200,0])])
        result,evidence=propose(doc,native,*meshes,{'map':'obj_test'},max_routes=1)
        self.assertEqual(result['nodes'],doc['nodes']);self.assertEqual(evidence['added_nodes'],0)
        routes=[r for r in evidence['routes'] if r.get('kind')=='same_component_local_gap']
        self.assertEqual(len(routes),1);self.assertIn(18,routes[0]['chain'][1:-1])
        self.assertIn(18,evidence['reused_ordinary_source_ids'])
        self.assertGreater(routes[0]['detour_before'][0],routes[0]['proposed_distance']*1.8)


if __name__=='__main__':unittest.main()
