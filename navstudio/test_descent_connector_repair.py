"""A proved road connector keeps the literal descent endpoint and all old nodes."""
import copy
import unittest
from repair import propose_descents


class DescentConnectorTests(unittest.TestCase):
    A=[-246.2368,101.0325,60.25]
    B=[-246.2368,37.0325,42.25]
    C=[-246.2368,-18.9675,4.25]
    E=[-256.,0.,4.25]
    def fixture(self):
        positions={883:self.A,393:self.E,407:[-192.,-110.8512,4.25],927:[-320.2368,-139.9675,8.25],
                   1117:[-340.2368,-130.6342,8.25],1100:[-344.2368,-107.9675,8.25],
                   1101:[-344.2368,-95.9675,8.25],1057:[-338.2368,-89.9675,8.25]}
        path=[393,407,927,1117,1100,1101,1057]
        pairs=[(883,1057),(1057,883)]+[(a,b)for p,q in zip(path,path[1:])for a,b in((p,q),(q,p))]
        nodes=[dict(id=i,position=copy.deepcopy(p),radius=35,source_radius=35,nav_flags=0,crouch=False,
                    spawn=False,topology_protected=False,locked=False,generated_guide=False,
                    source_reason='external_native_validated_floor',editor_role=0,action_roles=[],action_properties={})
               for i,p in sorted(positions.items())]
        doc=dict(map='obj_fixture',nodes=nodes,edges=[{'from':a,'to':b}for a,b in pairs],next_id=2000,base={})
        native=dict(bbox=dict(origin_above_feet=24),traversal_physics=dict(gravity=800),
                    native_input_coverage=dict(capacity=dict(input_validation_complete=True,input_truncated=False)),
                    nodes=[dict(id=i,accepted=i!=883,spawn_reachable=i!=883,nav_flags=0,crouch=False,editor_role=0,
                                reason='quarantined_no_validated_route_from_spawn' if i==883 else 'external_native_validated_floor',
                                native_input=dict(floor_accepted=True,status='missing_spawn_route' if i==883 else 'spawn_reachable',initial_incoming=0 if i==883 else 1))for i in positions],
                    edges=[dict(**{'from':a,'to':b},accepted=a!=883 and b!=883,crouch=False,
                                reason='capsule_support_gap_or_drop' if a==883 else 'capsule_support_step_or_drop' if b==883 else
                                'external_native_validated_walk')for a,b in pairs])
        return doc,native

    def probe(self,a,b,mode=None):
        def imported(p):
            feet=p[:]
            if p[:2]==self.B[:2] and p[2]==self.A[2]:feet=self.B[:]
            if p[:2]==self.C[:2] and p[2]==8.25:feet=self.C[:]
            row=dict(accepted=True,exact=feet==p,feet=feet)
            return dict(stand=row,crouch=copy.deepcopy(row))
        value=dict(stand='blocked_clearance',imports=dict(start=imported(a),end=imported(b)))
        if (a,b)in((self.A,self.B),(self.B,self.A),(self.C,self.E),(self.E,self.C)):
            value['stand']='accepted'
        if mode=='fresh_reverse' and a==self.E and b==self.C:value['stand']='blocked_clearance'
        if a[2]-b[2]>18.25:value['drop']=dict(accepted=a==self.B and b==self.C,delta=10.7,max_drop=118.75)
        if a==self.C and b==self.B:
            # The connector regression deliberately supplies a valid action
            # fixture. Actual Stadtrand's unstable landing remains rejected.
            value['jump']=dict(standing=dict(accepted=mode!='unstable_landing'),standing_proof=dict(samples=27))
        return value

    def test_blocked_literal_target_uses_two_actual_325_unit_graph_paths_and_fresh_connection(self):
        doc,native=self.fixture();before=copy.deepcopy(doc);calls=[]
        def probe(a,b):calls.append((a,b));return self.probe(a,b)
        result,evidence=propose_descents(doc,native,probe)
        self.assertEqual(doc,before);self.assertEqual(evidence['added_nodes'],2)
        route=evidence['routes'][0];proof=route['connector_paths']
        self.assertEqual(route['edge'],[883,1057]);self.assertEqual(route['connection_node'],393)
        self.assertEqual(proof['to_original_target']['source_ids'],[393,407,927,1117,1100,1101,1057])
        self.assertEqual(proof['from_original_target']['source_ids'],[1057,1101,1100,1117,927,407,393])
        for path in (proof['to_original_target'],proof['from_original_target']):
            self.assertAlmostEqual(path['length_units'],325.13428,places=4);self.assertEqual(path['hops'],6)
            self.assertTrue(all(edge['accepted']for edge in path['native_edges']))
        self.assertEqual(route['chain'][0],883);self.assertEqual(route['chain'][-1],1057)
        self.assertEqual(route['return_chain'][0],1057);self.assertEqual(route['return_chain'][-1],883)
        self.assertIn((self.C,self.E),calls);self.assertIn((self.E,self.C),calls)
        self.assertEqual(route['return_proof']['existing_graph_walk'],proof['from_original_target'])
        self.assertEqual({tuple(e)for e in evidence['replaced_rejected_edges']},{(883,1057),(1057,883)})
        for node in before['nodes']:self.assertEqual(next(n for n in result['nodes']if n['id']==node['id']),node)
        self.assertLessEqual(len(evidence['queries']),96)

    def test_missing_reverse_action_hazard_or_unbounded_graph_never_licenses_connection(self):
        for mode in('no_reverse','action','crouch','unknown','protected','long_path','nine_hops'):
            doc,native=self.fixture()
            if mode=='no_reverse':native['edges']=[e for e in native['edges']if (e['from'],e['to'])!=(1057,1101)]
            elif mode in('action','crouch'):
                for collection in(doc['nodes'],native['nodes']):
                    next(n for n in collection if n['id']==927)['nav_flags']=1<<27 if mode=='action' else 64
            elif mode=='unknown':next(e for e in native['edges']if (e['from'],e['to'])==(927,1117))['reason']='unknown'
            elif mode=='protected':doc['base']['protected_action_edges']=[[927,1117]]
            elif mode=='long_path':next(n for n in doc['nodes']if n['id']==927)['position'][1]-=1000
            else:
                for collection in(doc['edges'],native['edges']):collection[:]=[e for e in collection if(e['from'],e['to'])not in((393,407),(407,393))]
                seq=[393,1500,1501,1502,407]
                for i in seq[1:-1]:
                    node=copy.deepcopy(next(n for n in doc['nodes']if n['id']==393));node.update(id=i,position=[-256+(i-1499)*16.,-(i-1499)*27.,4.25]);doc['nodes'].append(node)
                    row=copy.deepcopy(next(n for n in native['nodes']if n['id']==393));row['id']=i;native['nodes'].append(row)
                for p,q in zip(seq,seq[1:]):
                    for a,b in((p,q),(q,p)):
                        doc['edges'].append({'from':a,'to':b});native['edges'].append(dict(**{'from':a,'to':b},accepted=True,crouch=False,reason='external_native_validated_walk'))
            result,evidence=propose_descents(doc,native,self.probe)
            with self.subTest(mode=mode):self.assertEqual(evidence['added_nodes'],0);self.assertEqual(result['nodes'],sorted(doc['nodes'],key=lambda n:n['id']))

    def test_fresh_connection_reverse_and_real_action_proof_are_still_mandatory(self):
        for mode in('fresh_reverse','unstable_landing'):
            doc,native=self.fixture();result,evidence=propose_descents(doc,native,lambda a,b:self.probe(a,b,mode))
            with self.subTest(mode=mode):
                self.assertEqual(evidence['added_nodes'],0);self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_accepted_old_reverse_is_retained_despite_complete_new_return(self):
        doc,native=self.fixture();native['edges'][1].update(accepted=True,reason='accepted')
        result,evidence=propose_descents(doc,native,self.probe)
        self.assertIn({'from':1057,'to':883},result['edges'])
        self.assertNotIn([1057,883],evidence['replaced_rejected_edges'])
        self.assertTrue(evidence['routes'][0]['return_chain'])


if __name__=='__main__':unittest.main()
