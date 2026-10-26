"""Bounded generic landing exits: fair source coverage and fresh lip proofs."""
import copy
import math
from pathlib import Path
import sys
import unittest

sys.path[:0]=[str(Path(__file__).resolve().parent.parent),str(Path(__file__).resolve().parent.parent/'navmesh')]
from repair import propose_descents


def scene(count=4,translation=(0.,0.,0.)):
    nodes=[];edges=[];routes=[]
    def node(identity,point,flags=0):
        p=[point[k]+translation[k]for k in range(3)]
        return dict(id=identity,position=p,radius=35,source_radius=35,nav_flags=flags,crouch=False,
            spawn=False,topology_protected=True,locked=False,generated_guide=False,
            source_reason='external_native_validated_floor',editor_role=0,action_roles=[],action_properties={})
    for i in range(count):
        x=i*1024.
        nodes.extend((node(10+i,[x,0,100.25]),node(100+i,[x+128,0,.25]),node(200+i,[x+16,0,.25],512)))
        edges.extend({'from':a,'to':b}for a,b in((100+i,200+i),(200+i,100+i),(200+i,10+i)))
        routes.append(dict(kind='free_climb',exported=True,launch=200+i,landing=10+i,
            touchdown=[x+40+translation[0],translation[1],130.125+translation[2]]))
    doc=dict(schema=1,mode='manual_draft',map='obj_test',revision=0,saved_revision=0,created=0,
        base=dict(environment={'fixture':'same-native-scene'}),next_id=1000,undo=[],redo=[],export=None,
        nodes=sorted(nodes,key=lambda n:n['id']),edges=sorted(edges,key=lambda e:(e['from'],e['to'])))
    native=dict(bbox=dict(origin_above_feet=30),traversal_physics=dict(gravity=1000),
        nodes=[dict(id=n['id'],accepted=True,spawn_reachable=True,nav_flags=n['nav_flags'],crouch=False,
                    editor_role=0,native_input=dict(floor_accepted=True))for n in nodes],
        edges=[dict(e,accepted=True,reason='manual_native_transition_live_pending')for e in edges],
        native_input_coverage=dict(ordinary_without_exit=list(range(10,10+count))),traversal_routes=routes)
    return doc,native


class FairDescentTests(unittest.TestCase):
    def test_unresolved_first_source_cannot_starve_three_other_landing_exits(self):
        doc,native=scene();before=copy.deepcopy((doc,native));calls=[]
        def rejected(a,b):
            calls.append((a,b));return dict(stand='blocked_clearance',drop=dict(accepted=False,reason='blocked_hull'))
        result,evidence=propose_descents(doc,native,rejected,max_probes=96)
        self.assertEqual(result,doc);self.assertEqual((doc,native),before)
        self.assertEqual(len(calls),96);self.assertEqual(evidence['unprobed_source_ids'],[])
        allocations=evidence['source_probe_allocations']
        self.assertEqual([r['source_id']for r in allocations],[10,11,12,13])
        self.assertEqual([r['queries']for r in allocations],[24]*4)
        self.assertTrue(all(r['queries']<=r['allowance']for r in allocations))
        self.assertEqual(evidence['deleted_nodes'],0);self.assertEqual(evidence['added_nodes'],0)
        self.assertTrue(evidence['native_revalidation_required'])

    def physical_probe(self,doc,translation=(0.,0.,0.),mode=None,wait_for_others=False):
        nodes={n['id']:n for n in doc['nodes']};seen=set();calls=[]
        high=nodes[10]['position'];lip=[high[0]+40,high[1],high[2]-.125];flat=[lip[0],lip[1],high[2]]
        ground=nodes[100]['position']
        near=lambda a,b:math.dist(a,b)<.001
        def probe(a,b):
            calls.append((a,b));source=min(range(len(doc['nodes'])//3),key=lambda i:abs(a[0]-nodes[10+i]['position'][0]))
            seen.add(source)
            walk='accepted'if abs(a[2]-b[2])<=18.25 else'gap_or_drop'
            if near(a,lip)or near(b,lip):walk='different_floor'
            imports={};actions={};walks={}
            for key,p in(('start',a),('end',b)):
                ordinary=dict(accepted=True,exact=True,reason='accepted',feet=list(p))
                action=dict(ordinary)
                if near(p,lip):ordinary.update(exact=False,feet=flat[:])
                if near(p,flat):action.update(exact=False,feet=lip[:])
                if mode=='no_typed_import'and near(p,lip):action.update(accepted=False,exact=False,reason='blocked_clearance')
                if mode=='no_landing_import'and near(a,lip)and near(b,ground)and key=='end':ordinary.update(accepted=False,exact=False,reason='blocked_clearance')
                imports[key]={s:dict(ordinary)for s in('stand','crouch')}
                actions[key]={'drop_launch':action}
                valid=abs(a[2]-b[2])<=18.25
                if mode=='no_approach'and near(b,lip):valid=False
                if mode=='no_return_approach'and near(a,lip)and near(b,high):valid=False
                walks[key]={'drop_launch':dict(accepted=valid,reason='accepted'if valid else'blocked_sweep')}
            direct_other=(source>0 and near(a,nodes[10+source]['position'])and near(b,nodes[100+source]['position']))
            # Static geometry: refinement discovers a later upper side edge.
            # Never make a cached collision result change with call order.
            late_launch=[high[0],high[1]+32,high[2]]
            lip_drop=(near(a,late_launch if wait_for_others else lip)and near(b,ground)and(mode!='no_drop'))
            return dict(stand=walk,imports=imports,action_imports=actions,action_walks=walks,
                drop=dict(accepted=direct_other or lip_drop,delta=20,max_drop=148.4375))
        return probe,calls,lip

    def test_unused_budget_returns_to_first_source_after_other_three_sources_are_repaired(self):
        doc,native=scene();before=copy.deepcopy((doc,native))
        probe,calls,lip=self.physical_probe(doc,wait_for_others=True)
        result,evidence=propose_descents(doc,native,probe,max_probes=96)
        self.assertLessEqual(len(calls),96)
        self.assertEqual(set(evidence['connected_isolated_source_ids']),{10,11,12,13})
        self.assertTrue(any(a['source_id']==10 and a['phase']=='refinement'for a in evidence['source_probe_allocations']))
        self.assertEqual((doc,native),before)
        # The physical alias is a NEW precise guide; the ordinary anchor and
        # its complete metadata remain unchanged.
        self.assertEqual(next(n for n in result['nodes']if n['id']==10),doc['nodes'][0])
        high=doc['nodes'][0]['position'];later_lip=[high[0],high[1]+32,high[2]]
        self.assertTrue(any(n['id']>=1000 and n['position']==later_lip and n['editor_role']==9 for n in result['nodes']))
        self.assertTrue(evidence['native_revalidation_required'])

    def test_touchdown_player_origin_is_only_a_new_candidate_requiring_every_fresh_proof(self):
        for translation in((0.,0.,0.),(-3840.,221.7025,37.25)):
            for mode in(None,'no_typed_import','no_approach','no_return_approach','no_drop','no_landing_import'):
                with self.subTest(translation=translation,mode=mode):
                    doc,native=scene(1,translation);before=copy.deepcopy((doc,native))
                    probe,calls,lip=self.physical_probe(doc,translation,mode)
                    result,evidence=propose_descents(doc,native,probe,max_probes=48)
                    self.assertLessEqual(len(calls),48);self.assertEqual((doc,native),before)
                    candidates=evidence['ascent_touchdown_launch_candidates']
                    self.assertTrue(any(c['position']==lip for c in candidates))
                    if mode is None:
                        self.assertTrue(evidence['added_nodes'])
                        self.assertEqual(next(n for n in result['nodes']if n['id']==10),doc['nodes'][0])
                        self.assertTrue(any(n['id']>=1000 and n['position']==lip for n in result['nodes']))
                    else:
                        self.assertEqual(evidence['added_nodes'],0);self.assertEqual(result,doc)

    def test_touchdown_outside_ordinary_seed_can_only_recommend_fresh_exact_new_guide(self):
        modes=(None,'no_typed_seed','bad_seed_reason','seed_xy','seed_far','ordinary_retry','typed_retry',
               'ordinary_forward','ordinary_reverse','reverse_import','reverse_typed','drop','landing')
        for translation in((0.,0.,0.),(-3840.,221.7025,37.25)):
            for mode in modes:
                with self.subTest(translation=translation,mode=mode):
                    doc,native=scene(1,translation);before=copy.deepcopy((doc,native))
                    base,calls,lip=self.physical_probe(doc,translation)
                    high=doc['nodes'][0]['position'];flat=lip[:2]+[high[2]];ground=next(n['position']for n in doc['nodes']if n['id']==100)
                    near=lambda a,b:math.dist(a,b)<.001
                    def probe(a,b):
                        value=base(a,b)
                        if near(b,lip):
                            row=value['imports']['end']['stand'];row.update(accepted=False,exact=False,reason='no_floor_near_seed',feet=lip[:])
                            action=value['action_imports']['end']['drop_launch'];action.update(accepted=True,exact=False,reason='accepted',feet=flat[:])
                            if mode=='no_typed_seed':action['accepted']=False
                            if mode=='bad_seed_reason':row['reason']='capsule_support_hazard'
                            if mode=='seed_xy':action['feet'][0]+=1
                            if mode=='seed_far':action['feet'][2]+=3
                        for endpoint,p in(('start',a),('end',b)):
                            if near(p,flat):
                                value['imports'][endpoint]['stand'].update(accepted=True,exact=True,reason='accepted',feet=flat[:])
                                value['action_imports'][endpoint]['drop_launch'].update(accepted=True,exact=True,reason='accepted',feet=flat[:])
                                value['stand']='accepted'if abs(a[2]-b[2])<=18.25 else'gap_or_drop'
                                if mode=='ordinary_retry'and endpoint=='end':value['imports'][endpoint]['stand']['accepted']=False
                                if mode=='typed_retry'and endpoint=='end':value['action_imports'][endpoint]['drop_launch']['exact']=False
                                if mode=='ordinary_forward'and endpoint=='end':value['stand']='blocked_sweep'
                                if near(a,flat)and near(b,high):
                                    if mode=='ordinary_reverse':value['stand']='blocked_sweep'
                                    if mode=='reverse_import':value['imports'][endpoint]['stand']['exact']=False
                                    if mode=='reverse_typed':value['action_imports'][endpoint]['drop_launch']['accepted']=False
                        value['drop']['accepted']=near(a,flat)and near(b,ground)and mode!='drop'
                        if mode=='landing'and near(a,flat)and near(b,ground):value['imports']['end']['stand']['accepted']=False
                        return value
                    result,evidence=propose_descents(doc,native,probe,max_probes=64)
                    self.assertLessEqual(len(calls),64);self.assertEqual((doc,native),before)
                    if mode is None:
                        self.assertTrue(evidence['added_nodes'])
                        self.assertTrue(any(n['id']>=1000 and n['position']==flat and n['editor_role']==9 for n in result['nodes']))
                        self.assertEqual(next(n for n in result['nodes']if n['id']==10),doc['nodes'][0])
                        self.assertTrue(any(near(a,high)and near(b,flat)for a,b in calls))
                        self.assertTrue(any(near(a,flat)and near(b,high)for a,b in calls))
                    else:self.assertEqual(result,doc)


if __name__=='__main__':unittest.main()
