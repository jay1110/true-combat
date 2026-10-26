"""Actual Rain touchdown pattern: propose literal, grounded inner lip guides."""
import copy
import math
from pathlib import Path
import sys
import unittest

sys.path[:0]=[str(Path(__file__).resolve().parent.parent),str(Path(__file__).resolve().parent.parent/'navmesh')]
from repair import propose_descents
from test_descent_fair_allocation import scene


class InsetLipTests(unittest.TestCase):
    def fixture(self,translation=(0.,0.,0.),mode=None):
        # Literal measured coordinates/stances from Rain Q13/Q14. The fake
        # collision responses below certify no real map clearance: they test
        # candidate selection and mandatory fresh proof sequencing only.
        doc,native=scene(1)
        pos=lambda p:[round(p[k]+translation[k],5)for k in range(3)]
        high=pos([768.,443.405,108.25]);touchdown=pos([813.9612,443.405,108.125])
        recommended=touchdown[:2]+[high[2]];ground=pos([896.,443.405,.25])
        nodes={n['id']:n for n in doc['nodes']}
        nodes[10]['position']=high;nodes[100]['position']=ground
        nodes[200]['position']=pos([816.25,443.405,.25])
        native['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        native['traversal_routes'][0]['touchdown']=touchdown[:2]+[touchdown[2]+30]
        inner=[pos([797.9612,443.405,108.25]),pos([781.9612,443.405,108.25])]
        target=inner[1]if mode=='second_inset'else inner[0]
        near=lambda a,b:math.dist(a,b)<.001
        calls=[]
        def probe(a,b):
            calls.append((list(a),list(b)))
            if a[2]-b[2]>18.25:
                self.assertGreaterEqual(math.dist(a[:2],b[:2]),8)
                self.assertLessEqual(math.dist(a[:2],b[:2]),160)
            walk='accepted'if abs(a[2]-b[2])<=18.25 else'gap_or_drop'
            imports={};actions={}
            for key,p in(('start',a),('end',b)):
                ordinary=dict(accepted=True,exact=True,reason='accepted',feet=list(p))
                action=dict(ordinary)
                if near(p,touchdown)or near(p,recommended):
                    ordinary.update(accepted=False,exact=False,reason='no_floor_near_seed')
                    action.update(exact=near(p,recommended),feet=recommended[:])
                if any(near(p,i)for i in inner):
                    if mode=='ordinary':ordinary.update(accepted=False,exact=False,reason='no_floor_near_seed')
                    if mode=='typed':action.update(accepted=False,exact=False,reason='capsule_startsolid')
                    if mode=='forward'and key=='end':walk='blocked_sweep'
                    if mode=='reverse'and key=='start'and near(b,high):walk='blocked_clearance'
                imports[key]={s:dict(ordinary)for s in('stand','crouch')}
                actions[key]={'drop_launch':action}
            drop=near(a,target)and near(b,ground)and mode!='drop'
            if mode=='landing'and near(a,target)and near(b,ground):
                imports['end']['stand'].update(accepted=False,exact=False,reason='blocked_clearance')
            return dict(stand=walk,imports=imports,action_imports=actions,
                drop=dict(accepted=drop,delta=24.8223,max_drop=148.4375))
        return doc,native,probe,calls,high,touchdown,recommended,ground,target,inner

    def test_measured_overhanging_touchdown_keeps_ordinary_gate_and_tries_new_inward_guide(self):
        for translation in((0.,0.,0.),(-3840.,-665.1075,37.25)):
            for mode in(None,'second_inset'):
                with self.subTest(translation=translation,mode=mode):
                    doc,native,probe,calls,high,raw,rec,ground,target,inner=self.fixture(translation,mode)
                    before=copy.deepcopy((doc,native));result,evidence=propose_descents(doc,native,probe,max_probes=48)
                    self.assertEqual((doc,native),before);self.assertLessEqual(len(calls),48)
                    self.assertEqual(evidence['connected_isolated_source_ids'],[10])
                    self.assertEqual(next(n for n in result['nodes']if n['id']==10),next(n for n in doc['nodes']if n['id']==10))
                    self.assertTrue(any(n['id']>=1000 and n['position']==target and n['editor_role']==9 for n in result['nodes']))
                    self.assertFalse(any(n['id']>=1000 and n['position']in(raw,rec)for n in result['nodes']))
                    self.assertTrue(any(math.dist(a,high)<.001 and math.dist(b,target)<.001 for a,b in calls))
                    self.assertTrue(any(math.dist(a,target)<.001 and math.dist(b,high)<.001 for a,b in calls))
                    if mode=='second_inset':
                        failed_first=[(a,b)for a,b in calls if math.dist(a,inner[0])<.001 and a[2]-b[2]>18.25]
                        # Three independent departure rays each have at most
                        # six bounded candidate attempts; cached anchors are
                        # not queried repeatedly across their fans.
                        self.assertLessEqual(len(failed_first),18)
                    self.assertTrue(evidence['native_revalidation_required'])

    def test_inset_never_waives_ordinary_typed_body_approach_drop_or_landing_proof(self):
        for mode in('ordinary','typed','forward','reverse','drop','landing'):
            with self.subTest(mode=mode):
                doc,native,probe,calls,*_=self.fixture(mode=mode);before=copy.deepcopy((doc,native))
                result,evidence=propose_descents(doc,native,probe,max_probes=48)
                self.assertEqual((doc,native),before);self.assertEqual(result,doc)
                self.assertEqual(evidence['added_nodes'],0);self.assertLessEqual(len(calls),48)

    def test_blocked_east_ray_uses_distinct_diagonal_departure_not_more_collinear_lengths(self):
        for translation in((0.,0.,0.),(-3840.,-665.1075,37.25)):
            doc,native,base,calls,high,raw,rec,ground,target,inner=self.fixture(translation,mode='drop')
            offset=96/math.sqrt(2);landing=[round(target[k]+offset,5)for k in(0,1)]+[ground[2]]
            near=lambda a,b:math.dist(a,b)<.001
            def probe(a,b):
                value=base(a,b)
                if a[2]-b[2]>18.25 and abs(a[1]-b[1])<.00001 and b[0]>a[0]:
                    # Actual V5 failure class: same launch + direction hits
                    # the same body obstacle independently of target range.
                    value['drop'].update(accepted=False,reason='blocked_hull_or_wrong_floor',
                                         touchdown=[a[0]+97.9138,a[1],47.8231+translation[2]])
                if near(a,target)and near(b,landing):value['drop'].update(accepted=True,reason='accepted')
                return value
            result,evidence=propose_descents(doc,native,probe,max_probes=96)
            self.assertEqual(evidence['connected_isolated_source_ids'],[10]);self.assertLessEqual(len(calls),96)
            self.assertTrue(any(n['id']>=1000 and n['position']==landing for n in result['nodes']))
            self.assertTrue(any(r.get('upper_approach')==target for r in evidence['routes']))
            east=[(a,b)for a,b in calls if near(a,target)and a[2]-b[2]>18.25 and abs(a[1]-b[1])<.00001 and b[0]>a[0]]
            self.assertEqual(len(east),1);self.assertGreater(evidence['blocked_drop_ray_count'],0)
            self.assertTrue(evidence['native_revalidation_required'])

    def test_another_roof_lip_needs_fresh_literal_upper_walk_and_import_before_diagonal_drop(self):
        for mode in('proved','upper_walk','upper_import'):
            doc,native,base,calls,high,raw,rec,ground,_,inner=self.fixture(mode='drop')
            upper=[high[0],high[1]+4*native['bbox']['half_width'],high[2]]
            offset=96/math.sqrt(2);landing=[round(upper[k]+offset,5)for k in(0,1)]+[ground[2]]
            near=lambda a,b:math.dist(a,b)<.001
            def probe(a,b):
                value=base(a,b)
                if a[2]-b[2]>18.25:value['drop'].update(accepted=False,reason='blocked_hull_or_wrong_floor',
                                                     touchdown=a[:2]+[b[2]+48])
                if near(a,upper)and near(b,landing):value['drop'].update(accepted=True,reason='accepted')
                if near(b,upper):
                    if mode=='upper_walk':value['stand']='blocked_sweep'
                    if mode=='upper_import':value['imports']['end']['stand'].update(accepted=False,exact=False,reason='no_floor_near_seed')
                return value
            before=copy.deepcopy((doc,native));result,evidence=propose_descents(doc,native,probe,max_probes=96)
            self.assertEqual((doc,native),before);self.assertLessEqual(len(calls),96)
            with self.subTest(mode=mode):
                if mode=='proved':
                    self.assertEqual(evidence['connected_isolated_source_ids'],[10])
                    self.assertTrue(any(n['id']>=1000 and n['position']==upper and n['editor_role']==9 for n in result['nodes']))
                    self.assertTrue(any(near(a,high)and near(b,upper)for a,b in calls))
                    self.assertTrue(any(near(a,upper)and near(b,high)for a,b in calls))
                else:self.assertEqual(result,doc)

    def test_endpoint_or_same_floor_failure_does_not_memoize_an_entire_departure_ray(self):
        doc,native,base,calls,high,raw,rec,ground,target,inner=self.fixture(mode='drop')
        first=[target[0]+96,target[1],ground[2]];second=[target[0]+128,target[1],ground[2]]
        near=lambda a,b:math.dist(a,b)<.001
        def probe(a,b):
            value=base(a,b)
            if near(a,target)and near(b,first):
                # Native uses this combined reason for goal-distance errors
                # too. An impact on the requested floor is not evidence of
                # the elevated obstacle needed by the angular search memo.
                value['drop'].update(accepted=False,reason='blocked_hull_or_wrong_floor',touchdown=second[:])
            if near(a,target)and near(b,second):value['drop'].update(accepted=True,reason='accepted')
            return value
        result,evidence=propose_descents(doc,native,probe,max_probes=96)
        self.assertEqual(evidence['connected_isolated_source_ids'],[10])
        self.assertTrue(any(n['id']>=1000 and n['position']==second for n in result['nodes']))
        self.assertTrue(any(near(a,target)and near(b,first)for a,b in calls))
        self.assertTrue(any(near(a,target)and near(b,second)for a,b in calls))


if __name__=='__main__':unittest.main()
