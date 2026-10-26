"""Bounded two-action returns use existing StandingJump recipes and distinct IDs."""
import copy
import math
import tempfile
import unittest
from pathlib import Path
from repair import propose_descents
import test_descent_connector_repair as connector


class StagedAscentTests(unittest.TestCase):
    def fixture(self):
        source=connector.DescentConnectorTests();doc,native=source.fixture()
        extra={1191:[-168.2368,-9.9675,4.25],409:[-192.,110.8512,4.25],
               1206:[-157.5701,222.0325,4.25],394:[-256.,221.7025,4.25]}
        for identity,point in extra.items():
            node=copy.deepcopy(next(n for n in doc['nodes']if n['id']==393));node.update(id=identity,position=point)
            doc['nodes'].append(node)
            row=copy.deepcopy(next(n for n in native['nodes']if n['id']==393));row['id']=identity;native['nodes'].append(row)
        path=[393,1191,409,1206,394]
        for a,b in zip(path,path[1:]):
            for s,t in((a,b),(b,a)):
                doc['edges'].append({'from':s,'to':t});native['edges'].append(dict(**{'from':s,'to':t},
                    accepted=True,crouch=False,reason='external_native_validated_walk'))
        doc['nodes'].sort(key=lambda n:n['id'])
        return doc,native

    def probe(self,a,b,mode=None,translation=(0,0,0)):
        a0=[a[i]-translation[i]for i in range(3)];b0=[b[i]-translation[i]for i in range(3)]
        A=connector.DescentConnectorTests.A;E=connector.DescentConnectorTests.E
        rear=[-256.,221.7025,4.25]
        near=lambda p,q:math.dist(p,q)<.001
        def imported(p):
            q=list(p)
            if abs(p[2]-60.25)<.001 and not near(p,A):
                q[2]=40.25 if abs(p[0]-A[0])<.001 and 160<=p[1]<=190 else 4.25
            if mode=='upper_stage' and near(p[:2],connector.DescentConnectorTests.B[:2]):q[2]=42.25
            if abs(p[2]-8.25)<.001 and abs(p[0]-A[0])<.001:q[2]=4.25
            feet=[q[i]+translation[i]for i in range(3)]
            row=dict(accepted=True,exact=near(q,p),feet=feet)
            return dict(stand=row,crouch=copy.deepcopy(row))
        value=dict(stand='blocked_clearance',imports=dict(start=imported(a0),end=imported(b0)))
        if mode=='upper_stage':
            B=connector.DescentConnectorTests.B;C=connector.DescentConnectorTests.C
            if (near(a0,A) and near(b0,B))or(near(a0,B) and near(b0,A)):value['stand']='accepted'
        if a0[2]-b0[2]>18.25:
            value['drop']=dict(accepted=near(a0,A) and abs(b0[0]-A[0])<.001 and b0[1]<0 and abs(b0[2]-4.25)<.001,
                               delta=14.8,max_drop=118.75)
            if mode=='upper_stage':value['drop']['accepted']=near(a0,B) and near(b0,C)
        if ((near(a0,E) and b0[1]<0 and abs(b0[2]-4.25)<.001)or
            (near(b0,E) and a0[1]<0 and abs(a0[2]-4.25)<.001)):
            value['stand']='accepted'
        if ((near(a0,rear) and b0[1]>200 and abs(b0[2]-4.25)<.001)or
            (near(b0,rear) and a0[1]>200 and abs(a0[2]-4.25)<.001)):
            value['stand']='accepted'
            if mode=='street_reverse' and near(a0,rear):value['stand']='blocked_clearance'
        if abs(a0[2]-40.25)<.001 and abs(b0[2]-40.25)<.001 and abs(a0[1]-b0[1])<.001 and math.dist(a0,b0)<=6.001:
            value['stand']='accepted'
            if mode=='alias' and math.dist(a0,b0)>0.001:value['stand']='blocked_clearance'
            if mode=='middle_reverse' and b0[0]<a0[0]:value['stand']='blocked_clearance'
            if mode=='middle_import':value['imports']['end']['stand']['exact']=False
        if b0[2]-a0[2]>18.25:
            accepted=False;reason='blocked_hull_arc'
            if abs(b0[2]-40.25)<.001:
                if not 172<=b0[1]<=177:reason='unstable_landing'
                else:accepted=49<=a0[1]-b0[1]<=51 and mode!='first_jump'
            elif near(b0,A) and abs(a0[2]-40.25)<.001:accepted=mode!='second_jump'
            value['jump']=dict(standing=dict(accepted=accepted,reason='accepted' if accepted else reason),
                               standing_proof=dict(samples=26 if mode=='samples' else 27))
            if mode=='jump_import' and accepted:value['imports']['start']['stand']['exact']=False
        return value

    def run_scene(self,mode=None,budget=96,translation=(0,0,0)):
        doc,native=self.fixture();calls=[]
        for node in doc['nodes']:node['position']=[node['position'][i]+translation[i]for i in range(3)]
        def probe(a,b):calls.append((a,b));return self.probe(a,b,mode,translation)
        result,evidence=propose_descents(doc,native,probe,max_probes=budget)
        return doc,result,evidence,calls

    def test_two_distinct_jump_launches_refine_real_middle_and_street_intervals(self):
        doc,result,evidence,calls=self.run_scene()
        self.assertGreater(evidence['added_nodes'],0)
        route=evidence['routes'][0];proof=route['return_proof'];self.assertEqual(proof['kind'],'two_standing_jumps')
        parts=proof['proof'];self.assertAlmostEqual(parts['middle'][1],173.0325)
        self.assertAlmostEqual(parts['street'][1],223.0325)
        self.assertLessEqual(len(calls),96)
        self.assertEqual(parts['from_descent_connector']['source_ids'],[393,1191,409,1206,394])
        self.assertEqual(parts['from_descent_connector']['hops'],4)
        self.assertAlmostEqual(parts['from_descent_connector']['length_units'],426.2816,places=3)
        nodes={n['id']:n for n in result['nodes']}
        self.assertNotEqual(proof['landing_source_id'],proof['launch_source_id'])
        self.assertEqual(nodes[proof['landing_source_id']]['nav_flags'],0)
        self.assertEqual(nodes[proof['launch_source_id']]['nav_flags'],1<<27)
        self.assertEqual(nodes[proof['street_launch_source_id']]['nav_flags'],1<<27)
        self.assertEqual(route['return_chain'][0],1057);self.assertEqual(route['return_chain'][-1],883)
        self.assertEqual(route['edge'],[883,1057]);self.assertTrue(evidence['native_revalidation_required'])
        for old in doc['nodes']:
            self.assertEqual(nodes[old['id']]['position'],old['position']);self.assertEqual(nodes[old['id']]['nav_flags'],old['nav_flags'])
        # Unstable landing is target-wide: one street trial per unstable R.
        for y in(165.0325,181.0325):
            self.assertEqual(sum(abs(b[1]-y)<.001 and abs(b[2]-40.25)<.001 and abs(a[2]-4.25)<.001 for a,b in calls),1)

    def test_translation_invariance_has_no_absolute_map_coordinates(self):
        _,_,first,_=self.run_scene();shift=(1234.,-543.,17.)
        _,_,second,_=self.run_scene(translation=shift)
        a=first['routes'][0]['return_proof']['proof'];b=second['routes'][0]['return_proof']['proof']
        for key in('street','middle','launch'):
            for i in range(3):self.assertAlmostEqual(a[key][i]+shift[i],b[key][i])

    def test_every_action_import_and_fresh_reverse_remains_mandatory(self):
        for mode in('first_jump','second_jump','samples','jump_import','street_reverse','middle_import'):
            doc,result,evidence,calls=self.run_scene(mode)
            with self.subTest(mode=mode):
                self.assertEqual(evidence['added_nodes'],0);self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual(evidence['replaced_rejected_edges'],[]);self.assertLessEqual(len(calls),96)

    def test_zero_length_alias_uses_distinct_ids_only_after_fresh_native_walk(self):
        _,result,evidence,calls=self.run_scene('alias');proof=evidence['routes'][0]['return_proof']
        nodes={n['id']:n for n in result['nodes']};a,b=proof['landing_source_id'],proof['launch_source_id']
        self.assertNotEqual(a,b);self.assertEqual(nodes[a]['position'],nodes[b]['position'])
        self.assertIn((nodes[a]['position'],nodes[a]['position']),calls)

    def test_budget_exhaustion_does_not_commit_half_an_action_chain(self):
        for limit in(12,24,36):
            doc,result,evidence,calls=self.run_scene(budget=limit)
            with self.subTest(limit=limit):
                self.assertLessEqual(len(calls),limit);self.assertEqual(evidence['added_nodes'],0)
                self.assertEqual(result['nodes'],doc['nodes']);self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_upper_walk_descent_reserves_shared_budget_for_independent_return(self):
        doc,result,evidence,calls=self.run_scene('upper_stage')
        self.assertGreater(evidence['added_nodes'],0)
        route=evidence['routes'][0]
        self.assertEqual(route['upper_approach'],connector.DescentConnectorTests.B)
        self.assertEqual(route['return_proof']['kind'],'two_standing_jumps')
        self.assertLessEqual(len(calls),96)
        for old in doc['nodes']:self.assertEqual(next(n for n in result['nodes']if n['id']==old['id']),old)

    def test_actual_draft_save_export_and_undo_preserve_both_directed_jump_ids(self):
        from edit import Drafts
        import test_descent_import_jump as single
        doc,proposal,evidence,_=self.run_scene('upper_stage')
        template,_=single.DescentImportJumpTests().fixture()
        for key in('schema','mode','revision','saved_revision','export','undo','redo','base'):
            if key in template:doc[key]=copy.deepcopy(template[key])
        proposal.update({key:copy.deepcopy(doc[key])for key in('schema','mode','revision','saved_revision','export','undo','redo','base')if key in doc})
        with tempfile.TemporaryDirectory() as folder:
            store=Drafts(Path(folder)/'drafts');store.documents[doc['map']]=copy.deepcopy(doc)
            current=store.apply_repair(doc['map'],doc['revision'],doc['base']['environment'],proposal)
            store.save(doc['map'],current['revision'],doc['base']['environment'])
            candidate=store.export_candidate(doc['map'],current['revision'],doc['base']['environment'],Path(folder)/'preview')
            lines=candidate.read_text().splitlines();header=lines[0].split()
            rows={int(row[0]):row for line in lines[1:1+int(header[3])]if(row:=line.split())}
            proof=evidence['routes'][0]['return_proof'];landing=proof['landing_source_id'];launch=proof['launch_source_id'];street=proof['street_launch_source_id']
            self.assertEqual((int(rows[landing][8]),int(rows[landing][10]),int(rows[landing][11])),(0,1,0))
            self.assertEqual((int(rows[street][8]),int(rows[street][10]),int(rows[street][11])),(1<<27,65,landing+1))
            self.assertEqual((int(rows[launch][8]),int(rows[launch][10]),int(rows[launch][11])),(1<<27,65,884))
            self.assertNotEqual(landing,launch)
            restored=store.change(doc['map'],dict(op='undo',revision=current['revision']),doc['base']['environment'])
            self.assertEqual(restored['nodes'],doc['nodes'])
            self.assertEqual({(e['from'],e['to'])for e in restored['edges']},{(e['from'],e['to'])for e in doc['edges']})


if __name__=='__main__':unittest.main()
