"""Literal native import heights and typed 27-sample return-jump proposals."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from repair import parse_native_probe,propose_descents
import test_native_descent_repair as fixtures


def jump_records(a,b,accepted=True):
    return [dict(part='points',coordinates='feet',**{'from':a,'to':b},readonly=True,live_verified=False),
            dict(part='running',accepted=False,reason='height_range',queries=0,hit=a),
            dict(part='standing',accepted=accepted,reason='accepted' if accepted else 'blocked_hull_arc',queries=246,hit=b)]+(
            [dict(part='standing_proof',samples=27,duration=.608,crash=8.,touchdown=b)] if accepted else [])


def raw_reply(a,b,result,jumps=None):
    vector=lambda p:'('+' '.join('%.5f'%v for v in p)+')'
    reply=(f'navgen probe -1->-1 feet={vector(a)}->{vector(b)} stand={result.get("stand","accepted")} '
           f'crouch={result.get("stand","accepted")} traces=15 quarantined=0/0 (diagnostic only; graph unchanged)\n')
    for endpoint,key in enumerate(('start','end')):
        for stance in(0,1):
            item=result.get('imports',{}).get(key,{}).get('crouch' if stance else 'stand')
            if item is not None:
                reply+=(f'navgen probe import endpoint={endpoint} crouch={stance} accepted={int(item["accepted"])} '
                        f'exact={int(item["exact"])} reason=accepted feet={vector(item["feet"])}\n')
    if 'drop' in result:
        drop=result['drop']
        reply+=(f'navgen probe drop -1->-1 accepted={int(drop["accepted"])} reason=accepted '
                f'crash_limit=38 max_drop=118.7500 delta={drop["delta"]:.4f} '
                f'landing={vector(b[:2]+[b[2]+24])} (readonly samples)\n')
    for row in jumps or():reply+='navgen probe jump '+json.dumps(row)+'\n'
    return reply


class NativeJumpParserTests(unittest.TestCase):
    def test_actual_json_parts_bind_exact_feet_and_keep_standing_body_proof(self):
        a=[0.,-110.,.25];b=[0.,0.,25.25]
        reply=raw_reply(a,b,{},jump_records(a,b))
        result=parse_native_probe(reply,a,b,24)
        self.assertTrue(result['jump']['standing']['accepted'])
        self.assertFalse(result['jump']['running']['accepted'])
        self.assertEqual(result['jump']['standing_proof']['touchdown'],b)
        self.assertEqual(result['reply_sha256'],hashlib.sha256(reply.encode()).hexdigest())
        rejected=parse_native_probe(raw_reply(a,b,{},jump_records(a,b,False)),a,b,24)
        self.assertNotIn('standing_proof',rejected['jump'])

    def test_malformed_duplicate_unbound_and_out_of_volume_proofs_fail_closed(self):
        a=[0.,-110.,.25];b=[0.,0.,25.25]
        cases=('missing_part','duplicate_part','missing_proof','proof_after_rejection','wrong_points',
               'wrong_units','not_readonly','fake_live','string_bool','accepted_reason','query_bool',
               'negative_queries','unknown_part','nan_hit','far_hit','samples','duration','crash',
               'nan_crash','far_touchdown','wrong_floor','bad_json','duplicate_key')
        for case in cases:
            rows=jump_records(a,b);points,running,standing,proof=rows
            if case=='missing_part':rows.pop(1)
            elif case=='duplicate_part':rows.append(copy.deepcopy(running))
            elif case=='missing_proof':rows.pop()
            elif case=='proof_after_rejection':standing.update(accepted=False,reason='blocked_hull_arc')
            elif case=='wrong_points':points['to']=[0.,0.,26.25]
            elif case=='wrong_units':points['coordinates']='origin'
            elif case=='not_readonly':points['readonly']=False
            elif case=='fake_live':points['live_verified']=True
            elif case=='string_bool':standing['accepted']='true'
            elif case=='accepted_reason':standing['reason']='blocked_hull_arc'
            elif case=='query_bool':standing['queries']=True
            elif case=='negative_queries':standing['queries']=-1
            elif case=='unknown_part':running['part']='guess'
            elif case=='nan_hit':running['hit']=[float('nan'),0,0]
            elif case=='far_hit':running['hit']=[131073,0,0]
            elif case=='samples':proof['samples']=26
            elif case=='duration':proof['duration']=2.01
            elif case=='crash':proof['crash']=38.01
            elif case=='nan_crash':proof['crash']=float('nan')
            elif case=='far_touchdown':proof['touchdown']=[96.1,0,25.25]
            elif case=='wrong_floor':proof['touchdown']=[0,0,43.35]
            reply=raw_reply(a,b,{},rows)
            if case=='bad_json':reply+='navgen probe jump {broken}\n'
            elif case=='duplicate_key':reply=reply.replace('"samples": 27','"samples": 27, "samples": 27')
            with self.subTest(case=case),self.assertRaises(RuntimeError):parse_native_probe(reply,a,b,24)


class DescentImportJumpTests(unittest.TestCase):
    def fixture(self,reverse=True):
        doc,native=fixtures.fixture()
        doc['nodes'][0]['position'][2]=25.25;doc['nodes'][1]['position'][2]=4.25
        for n in doc['nodes']:
            n.update(name='retained-'+str(n['id']),facing=[1.,0.,0.],way_guid=n['id'])
        doc['nodes'][0]['action_properties']['note']='retained metadata'
        native['bbox']['origin_above_feet']=24;native['traversal_physics']['gravity']=800
        if reverse:
            doc['edges'].append({'from':99,'to':3})
            native['edges'].append(dict(**{'from':99,'to':3},accepted=False,reason='capsule_support_step_or_drop'))
        return doc,native

    def result(self,a,b):
        def imported(p):
            feet=list(p)
            # Street candidates need their true floor. Existing target99 is
            # on a separate 4-unit step; it is never relocated.
            if p[2]==4.25 and p!=[64.,-110.,4.25]:feet[2]=.25
            item=dict(accepted=True,exact=feet==p,feet=feet)
            return dict(stand=copy.deepcopy(item),crouch=copy.deepcopy(item))
        result=dict(stand='accepted',imports=dict(start=imported(a),end=imported(b)))
        if a[2]>b[2]+18.25:
            result.update(stand='capsule_support_gap_or_drop',drop=dict(
                accepted=a[0]==b[0] and b[1]<=-96 and b[2]==.25,delta=8.,max_drop=118.75))
        if b[2]>a[2]+18.25:result['stand']='capsule_support_step_or_drop'
        return result

    def probe(self,a,b,change=None):
        value=self.result(a,b);jumps=jump_records(a,b) if b[2]>a[2]+18.25 else None
        if change:change(a,b,value,jumps)
        return parse_native_probe(raw_reply(a,b,value,jumps),a,b,24)

    def test_new_landing_reprojects_then_rechecks_and_keeps_old_positions_and_flags(self):
        doc,native=self.fixture();before=copy.deepcopy(doc);calls=[]
        def probe(a,b):calls.append((a,b));return self.probe(a,b)
        result,evidence=propose_descents(doc,native,probe)
        self.assertEqual(doc,before);self.assertEqual(evidence['added_nodes'],1)
        route=evidence['routes'][0];self.assertEqual(route['chain'],[3,121,99])
        self.assertEqual(route['return_chain'],[99,121,3])
        self.assertLess(calls.index(([0,0,25.25],[0,-110,4.25])),calls.index(([0,0,25.25],[0,-110,.25])))
        self.assertIn(([0,-110,.25],[64,-110,4.25]),calls)
        self.assertIn(([64,-110,4.25],[0,-110,.25]),calls)
        nodes={n['id']:n for n in result['nodes']};landing=nodes[121]
        self.assertEqual(landing['position'],[0.,-110.,.25]);self.assertEqual(landing['radius'],2)
        self.assertEqual(landing['nav_flags'],1<<27);self.assertEqual(landing['editor_role'],65)
        self.assertIn(dict(kind='standing_jump',role='launch',landing_node=3),landing['action_roles'])
        self.assertEqual(landing['action_properties'],{'tce_stand_jump':'1'})
        self.assertEqual({tuple(e)for e in evidence['replaced_rejected_edges']},{(3,99),(99,3)})
        for old in before['nodes']:
            for key in('position','nav_flags','name','facing','way_guid','source_reason'):
                self.assertEqual(nodes[old['id']][key],old[key])
        self.assertEqual(nodes[99],before['nodes'][1]);self.assertEqual(nodes[120],before['nodes'][2])
        self.assertEqual(nodes[3]['action_properties']['note'],'retained metadata')
        self.assertEqual(route['return_chain_proof_sha256'],hashlib.sha256(json.dumps(route['return_proof'],sort_keys=True,separators=(',',':')).encode()).hexdigest())
        self.assertEqual(route['return_proof']['kind'],'standing_jump')
        self.assertTrue(route['return_proof']['action_probe']['jump']['standing']['accepted'])

    def test_suggested_floor_needs_fresh_exact_import_and_drop_proof(self):
        for mode in('rejected_import','changed_xy','nonexact_fresh','rejected_fresh_drop','changed_launch','unsafe_drop'):
            doc,native=self.fixture()
            def change(a,b,value,jumps):
                if 'drop' not in value:return
                if mode=='rejected_import':value['imports']['end']['stand']['accepted']=False
                elif mode=='changed_xy':value['imports']['end']['stand']['feet'][0]+=1
                elif mode=='nonexact_fresh' and b[2]==.25:value['imports']['end']['stand'].update(exact=False,feet=b[:2]+[.5])
                elif mode=='rejected_fresh_drop' and b[2]==.25:value['drop']['accepted']=False
                elif mode=='changed_launch':value['imports']['start']['stand']['exact']=False
                elif mode=='unsafe_drop':value['imports']['end']['stand']['feet'][2]=-200
            result,evidence=propose_descents(doc,native,lambda a,b:self.probe(a,b,change))
            with self.subTest(mode=mode):
                self.assertEqual(result['nodes'],doc['nodes']);self.assertEqual(result['edges'],sorted(doc['edges'],key=lambda e:(e['from'],e['to'])))
                self.assertEqual(evidence['added_nodes'],0);self.assertLessEqual(len(evidence['queries']),96)

    def test_new_upper_staging_import_height_requires_exact_reprobe_and_both_walk_directions(self):
        for mode in ('valid','reverse_blocked','fresh_nonexact','changed_xy'):
            doc,native=self.fixture();before=copy.deepcopy(doc);calls=[]
            old=[0,0,25.25];asked=[64,0,25.25];top=[64,0,21.25]
            def probe(a,b):
                calls.append((a,b));value=self.result(a,b)
                value['stand']='capsule_walk_sweep'
                if a[2]<5 and b[2]<5 or(a==old and b==top)or(a==top and b==old):value['stand']='accepted'
                if 'drop' in value:value['drop']['accepted']=a==top and b[2]==.25
                if b==asked:
                    value['imports']['end']['stand'].update(exact=False,feet=top[:])
                    if mode=='changed_xy':value['imports']['end']['stand']['feet'][0]+=1
                if mode=='fresh_nonexact' and b==top:value['imports']['end']['stand'].update(exact=False,feet=top[:2]+[21.5])
                if mode=='reverse_blocked' and a==top and b==old:value['stand']='capsule_walk_sweep'
                rows=jump_records(a,b) if b[2]>a[2]+18.25 else None
                return parse_native_probe(raw_reply(a,b,value,rows),a,b,24)
            result,evidence=propose_descents(doc,native,probe)
            with self.subTest(mode=mode):
                self.assertEqual(doc,before);self.assertLessEqual(len(evidence['queries']),96)
                if mode=='valid':
                    self.assertGreater(evidence['added_nodes'],0)
                    self.assertEqual(evidence['routes'][0]['upper_approach'],top)
                    self.assertLess(calls.index((old,asked)),calls.index((old,top)))
                    self.assertIn((top,old),calls)
                    self.assertEqual(result['nodes'][0]['position'],old)
                else:self.assertEqual(evidence['added_nodes'],0)

    def test_height_difference_does_not_bypass_forward_or_reverse_walk_proof(self):
        for mode in('forward','reverse'):
            doc,native=self.fixture()
            def change(a,b,value,jumps):
                if (mode=='forward' and b==[64.,-110.,4.25]) or(mode=='reverse' and a==[64.,-110.,4.25]):
                    value['stand']='capsule_walk_sweep'
            result,evidence=propose_descents(doc,native,lambda a,b:self.probe(a,b,change))
            with self.subTest(mode=mode):
                if mode=='forward':self.assertEqual(evidence['added_nodes'],0)
                else:
                    self.assertIsNone(evidence['routes'][0]['return_chain'])
                    self.assertIn({'from':99,'to':3},result['edges'])
                    self.assertFalse(any(n['editor_role']&64 for n in result['nodes']))

    def test_return_needs_standing_proof_and_never_promotes_running_only(self):
        doc,native=self.fixture()
        def change(a,b,value,jumps):
            if jumps:
                jumps[1].update(accepted=True,reason='accepted')
                jumps[2].update(accepted=False,reason='blocked_hull_arc');jumps.pop()
        result,evidence=propose_descents(doc,native,lambda a,b:self.probe(a,b,change))
        self.assertIsNone(evidence['routes'][0]['return_chain'])
        self.assertIn({'from':99,'to':3},result['edges'])
        self.assertFalse(any(n['editor_role']&64 for n in result['nodes']))

    def test_accepted_unknown_hazard_or_protected_reverse_edge_is_never_withdrawn(self):
        for mode in('accepted','unknown','hazard','protected'):
            doc,native=self.fixture();row=native['edges'][-1]
            if mode=='accepted':row.update(accepted=True,reason='accepted')
            elif mode=='unknown':row['reason']='unknown'
            elif mode=='hazard':row['reason']='liquid_hazard'
            else:doc['base']['protected_action_edges']=[[99,3]]
            result,evidence=propose_descents(doc,native,self.probe)
            with self.subTest(mode=mode):
                self.assertEqual(evidence['routes'][0]['return_chain'],[99,121,3])
                self.assertIn({'from':99,'to':3},result['edges'])
                self.assertNotIn([99,3],evidence['replaced_rejected_edges'])

    def test_standing_launch_and_drop_target_export_as_existing_format5_recipes(self):
        from edit import Drafts
        doc,native=self.fixture();proposal,_=propose_descents(doc,native,self.probe)
        with tempfile.TemporaryDirectory() as folder:
            store=Drafts(Path(folder)/'drafts');store.documents[doc['map']]=copy.deepcopy(doc)
            current=store.apply_repair(doc['map'],doc['revision'],doc['base']['environment'],proposal)
            store.save(doc['map'],current['revision'],doc['base']['environment'])
            candidate=store.export_candidate(doc['map'],current['revision'],doc['base']['environment'],Path(folder)/'preview')
            lines=candidate.read_text().splitlines();header=lines[0].split()
            nodes={int(row[0]):row for line in lines[1:1+int(header[3])] if (row:=line.split())}
            self.assertEqual(int(nodes[121][8]),1<<27);self.assertEqual(int(nodes[121][10]),65)
            self.assertEqual(int(nodes[121][11]),4)  # exact existing source3 +1
            self.assertEqual(int(nodes[3][10]),9);self.assertEqual(int(nodes[3][11]),122)
            self.assertEqual(list(map(float,nodes[99][1:4])),doc['nodes'][1]['position'])
            restored=store.change(doc['map'],dict(op='undo',revision=current['revision']),doc['base']['environment'])
            self.assertEqual(restored['nodes'],doc['nodes'])
            self.assertEqual({(e['from'],e['to'])for e in restored['edges']},{(e['from'],e['to'])for e in doc['edges']})

    def test_quarantined_source_without_entry_continues_until_paired_return_is_proved(self):
        for allow_later in(False,True):
            doc,native=self.fixture();node=doc['nodes'][0];row=native['nodes'][0]
            node['action_properties']={}
            row.update(accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn',
                       nav_flags=0,crouch=False,editor_role=0,
                       native_input=dict(floor_accepted=True,status='missing_spawn_route',initial_incoming=0))
            native['edges'][0]['reason']='capsule_support_gap_or_drop'
            native['native_input_coverage']=dict(capacity=dict(input_validation_complete=True,input_truncated=False))
            for other in native['nodes'][1:]:
                other.update(nav_flags=next(n['nav_flags']for n in doc['nodes']if n['id']==other['id']),crouch=False,
                             editor_role=0,native_input=dict(floor_accepted=True))
            def change(a,b,value,jumps):
                if jumps and (not allow_later or a[1]==-110):
                    jumps[2].update(accepted=False,reason='blocked_hull_arc');jumps.pop()
            result,evidence=propose_descents(doc,native,lambda a,b:self.probe(a,b,change))
            with self.subTest(allow_later=allow_later):
                self.assertTrue(evidence['routes'][0]['requires_proved_return_to_quarantined_source'])
                self.assertLessEqual(len(evidence['queries']),96)
                if allow_later:
                    self.assertTrue(evidence['routes'][0]['return_chain'])
                    self.assertNotEqual(result['nodes'][-1]['position'][1],-110)
                else:
                    self.assertEqual(evidence['added_nodes'],0);self.assertEqual(result['nodes'],doc['nodes'])
                    self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_free_climb_counterpart_also_requires_every_fresh_mounting_walk(self):
        for blocked in(False,True):
            doc,native=fixtures.fixture();doc['edges'].append({'from':99,'to':3})
            native['edges'].append(dict(**{'from':99,'to':3},accepted=False,reason='blocked_step_up'))
            def probe(a,b):
                if blocked and a==[0,-64,.25]:return dict(stand='blocked_sweep')
                return fixtures.NativeDescentRepairTests().probe(a,b)
            result,evidence=propose_descents(doc,native,probe)
            with self.subTest(blocked=blocked):
                if blocked:
                    self.assertIn({'from':99,'to':3},result['edges'])
                    self.assertIsNone(evidence['routes'][0]['return_proof'])
                else:
                    self.assertNotIn({'from':99,'to':3},result['edges'])
                    self.assertEqual(evidence['routes'][0]['return_chain'],[99,121,122,3])
                    self.assertEqual(evidence['routes'][0]['return_proof']['kind'],'free_climb')


if __name__=='__main__':unittest.main()
