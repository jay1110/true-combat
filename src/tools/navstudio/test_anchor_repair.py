"""Source-bound, bounded ordinary anchor correction; no engine or project IO."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from edit import Drafts
from repair import propose_anchor_relocations,parse_native_probe
import test_rejected_ordinary_feedback as ordinary_fixtures
import test_path_repair as path_fixtures


class AnchorRepairTests(unittest.TestCase):
    def bind(self,doc,native):return path_fixtures.PathRepairTests().bind(doc,native)

    def fixture(self):
        factory=ordinary_fixtures.RejectedOrdinaryFeedbackTests()
        doc,native,_=factory.fixture(rejected={(1,2):'capsule_walk_sweep',(3,2):'blocked_sweep'})
        doc['base']['movement_profile']=dict(radius=16,standing_height=87,crouching_height=61,origin_above_feet=30,step=18)
        native['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        node=doc['nodes'][1];row=native['nodes'][1]
        node.update(position=[64.,0.,4.4727],name='',facing=[0.,0.,0.],source_reason='external_native_validated_floor',
                    nav_flags=64,crouch=True)
        row.update(raw_position=[64.,0.,34.4727],position=[64.,0.,34.4727],name='',facing=[0.,0.,0.],
                   nav_flags=64,crouch=True,accepted=False,spawn_reachable=False,
                   reason='quarantined_no_validated_route_from_spawn')
        row['native_input'].update(floor_accepted=True,status='missing_spawn_route')
        for edge in native['edges']:
            if edge['from']==2:edge.update(accepted=False,reason='quarantined_unreachable_endpoint')
        doc['edges'].sort(key=lambda e:(e['from'],e['to']))
        native['edges'].sort(key=lambda e:(e['from'],e['to']))
        return doc,native,self.bind(doc,native)

    def probe(self,a,b):
        def imported(p):
            feet=list(p)
            if abs(p[0]-64)<=8 and abs(p[1])<=8:feet[2]=.25
            return dict(stand=dict(accepted=True,exact=feet==p,feet=feet),crouch=dict(accepted=True,exact=feet==p,feet=feet))
        point=[64.,4.,.25]
        valid=a==point or b==point
        result=dict(stand='accepted' if valid else 'capsule_walk_sweep',
                    crouch='accepted' if valid else 'capsule_walk_sweep',
                    imports=dict(start=imported(a),end=imported(b)))
        result['reply_sha256']=hashlib.sha256(json.dumps([a,b,result],sort_keys=True).encode()).hexdigest()
        return result

    def run_repair(self,parts,probe=None,**kw):
        return propose_anchor_relocations(parts[0],parts[1],probe or self.probe,parts[2],**kw)

    def test_literal_roundtrip_floor_and_every_directed_old_edge_preserve_all_other_fields(self):
        parts=self.fixture();before=copy.deepcopy(parts);proposal,evidence=self.run_repair(parts)
        self.assertEqual(parts,before)
        expected=copy.deepcopy(parts[0]);expected['nodes'][1]['position']=[64.,4.,.25]
        self.assertEqual(proposal,expected)
        self.assertEqual(evidence['moves'][0]['retained_accepted_incident_edges'],[[2,1]])
        self.assertEqual(evidence['moves'][0]['way_guid'],2)
        self.assertTrue(evidence['native_revalidation_required']);self.assertFalse(evidence['installed'])
        self.assertTrue(any(q['end']==[64.,4.,4.4727] for q in evidence['queries']))
        self.assertTrue(any(q['start']==[64.,4.,.25] or q['end']==[64.,4.,.25] for q in evidence['queries']))
        self.assertEqual(evidence['source_native_report'],hashlib.sha256(parts[2]['native_report_bytes']).hexdigest())

    def test_previously_accepted_quarantined_direction_must_not_break(self):
        parts=self.fixture()
        def reject_return(a,b):
            result=self.probe(a,b)
            if a==[64.,4.,.25] and b==[0.,0.,0.]:result.update(stand='blocked_sweep',crouch='blocked_sweep')
            return result
        proposal,evidence=self.run_repair(parts,reject_return)
        self.assertEqual(proposal,parts[0]);self.assertEqual(evidence['moves'],[])

    def test_even_old_accepted_incident_edge_to_nonspawn_component_is_preserved(self):
        parts=self.fixture();doc,native,_=parts
        doc['edges'].append(dict(**{'from':2,'to':3}))
        native['edges'].append(dict(**{'from':2,'to':3},accepted=True,reason='external_native_validated_walk',crouch=False))
        parts=(doc,native,self.bind(doc,native))
        def reject_other(a,b):
            result=self.probe(a,b)
            if a==[64.,4.,.25] and b==[32.,32.,0.]:result.update(stand='blocked_sweep',crouch='blocked_sweep')
            return result
        proposal,evidence=self.run_repair(parts,reject_other)
        self.assertEqual(proposal,doc);self.assertEqual(evidence['moves'],[])

    def test_protected_source_fields_never_trigger_probes(self):
        for fields in (dict(locked=True),dict(spawn=True),dict(name='keep me'),dict(facing=[0,1,0]),
                       dict(properties={'note':'user'}),dict(action_properties={'note':'user'}),
                       dict(action_roles=[dict(kind='walk_drop')]),dict(door_recipe={'mode':3}),
                       dict(editor_role=1),dict(generated_guide=True),dict(nav_flags=512,crouch=False),
                       dict(source_reason='manual_added'),dict(source_reason='unknown_machine_reason')):
            doc,native,_=self.fixture();doc['nodes'][1].update(fields)
            row=native['nodes'][1]
            row.update(nav_flags=doc['nodes'][1]['nav_flags'],crouch=doc['nodes'][1]['crouch'],
                       name=doc['nodes'][1]['name'],facing=doc['nodes'][1]['facing'])
            from edit import custom_way_properties
            row['custom_properties']=custom_way_properties(doc['nodes'][1])
            parts=(doc,native,self.bind(doc,native))
            with self.subTest(fields=fields):
                proposal,evidence=self.run_repair(parts,lambda a,b:self.fail('Protected point was probed'))
                self.assertEqual(proposal,doc);self.assertEqual(evidence['queries'],[])

    def test_native_action_membership_and_protected_incident_edge_block_move(self):
        for mode in ('route','native_role','drop','door','protected_edge'):
            doc,native,_=self.fixture()
            if mode=='route':native['traversal_routes']=[dict(kind='jump',landing=2)]
            elif mode=='native_role':native['nodes'][1]['editor_role']=1
            elif mode=='drop':native['nodes'][1]['editor_drop_target']=3
            elif mode=='door':native['nodes'][1]['door_recipe']={'mode':3}
            else:doc['base']['protected_action_edges']=[[2,1]]
            parts=(doc,native,self.bind(doc,native))
            with self.subTest(mode=mode):
                proposal,evidence=self.run_repair(parts,lambda a,b:self.fail('Action was probed'))
                self.assertEqual(proposal,doc);self.assertEqual(evidence['moves'],[])

    def test_only_known_floor_quarantine_is_eligible(self):
        for mode in ('accepted','floor','reason','status','native_stance'):
            doc,native,_=self.fixture();row=native['nodes'][1]
            if mode=='accepted':row.update(accepted=True,spawn_reachable=True)
            elif mode=='floor':row['native_input']['floor_accepted']=False
            elif mode=='reason':row['reason']='no_floor_near_seed'
            elif mode=='status':row['native_input']['status']='geometry_rejected'
            else:row['crouch']=False
            parts=(doc,native,self.bind(doc,native))
            with self.subTest(mode=mode):
                if mode=='floor':
                    with self.assertRaises(RuntimeError):self.run_repair(parts)
                else:
                    proposal,evidence=self.run_repair(parts,lambda a,b:self.fail('Unproved point was probed'))
                    self.assertEqual(proposal,doc);self.assertEqual(evidence['queries'],[])

    def test_radius_quarter_and_absolute_eight_unit_limits_are_hard(self):
        for radius,z in ((10,.25),(35,-4.),(35,13.)):
            parts=self.fixture();doc,native,_=parts;doc['nodes'][1]['radius']=radius
            parts=(doc,native,self.bind(doc,native))
            def outside(a,b):
                result=self.probe(a,b)
                for endpoint in ('start','end'):
                    for stance in ('stand','crouch'):result['imports'][endpoint][stance]['feet'][2]=z
                return result
            proposal,evidence=self.run_repair(parts,outside)
            with self.subTest(radius=radius,z=z):
                self.assertEqual(proposal,doc);self.assertEqual(evidence['moves'],[])
                for query in evidence['queries']:
                    target=query['end'];self.assertLessEqual(abs(target[2]-doc['nodes'][1]['position'][2]),min(8,radius*.25))

    def test_three_dimensional_total_displacement_not_only_separate_axes_is_bounded(self):
        parts=self.fixture()
        def diagonal_outside(a,b):
            result=self.probe(a,b)
            for endpoint in ('start','end'):
                for stance in ('stand','crouch'):
                    result['imports'][endpoint][stance].update(feet=[68.,4.,-1.5273],exact=True)
            return result
        proposal,evidence=self.run_repair(parts,diagonal_outside)
        self.assertEqual(proposal,parts[0]);self.assertEqual(evidence['moves'],[])

    def test_import_rejection_missing_stance_or_drifting_roundtrip_never_moves(self):
        for mode in ('rejected','missing','drift','nan','outside_xy'):
            parts=self.fixture()
            def invalid(a,b):
                result=self.probe(a,b)
                if mode=='missing':result.pop('imports');return result
                for endpoint,p in (('start',a),('end',b)):
                    for stance in ('stand','crouch'):
                        row=result['imports'][endpoint][stance]
                        if mode=='rejected':row['accepted']=False
                        elif mode=='drift':row['feet'][2]=p[2]+.125
                        elif mode=='nan':row['feet'][2]=float('nan')
                        else:row['feet'][0]=99
                return result
            with self.subTest(mode=mode):
                proposal,evidence=self.run_repair(parts,invalid)
                self.assertEqual(proposal,parts[0]);self.assertEqual(evidence['moves'],[])

    def test_standing_boundary_cannot_use_only_duck_proof(self):
        parts=self.fixture()
        def duck_only(a,b):
            result=self.probe(a,b);result['stand']='blocked_clearance';return result
        proposal,evidence=self.run_repair(parts,duck_only)
        self.assertEqual(proposal,parts[0]);self.assertEqual(evidence['moves'],[])

    def test_valid_crouch_to_crouch_keeps_authored_flags(self):
        doc,native,_=self.fixture()
        for node,row in zip(doc['nodes'],native['nodes']):
            node.update(nav_flags=64,crouch=True);row.update(nav_flags=64,crouch=True)
        parts=(doc,native,self.bind(doc,native))
        def duck_only(a,b):
            result=self.probe(a,b);result['stand']='blocked_clearance';return result
        proposal,evidence=self.run_repair(parts,duck_only)
        self.assertEqual(len(evidence['moves']),1);self.assertTrue(all(n['nav_flags']==64 for n in proposal['nodes']))

    def test_incoming_must_originate_at_spawn_reachable_retained_graph(self):
        doc,native,_=self.fixture()
        for i in (0,2):native['nodes'][i]['spawn_reachable']=False
        parts=(doc,native,self.bind(doc,native))
        proposal,evidence=self.run_repair(parts,lambda a,b:self.fail('No reached entry'))
        self.assertEqual(proposal,doc);self.assertEqual(evidence['queries'],[])

    def test_without_retained_directed_outgoing_no_move_is_proposed(self):
        doc,native,_=self.fixture()
        doc['edges']=[e for e in doc['edges'] if e['from']!=2];native['edges']=[e for e in native['edges'] if e['from']!=2]
        parts=(doc,native,self.bind(doc,native))
        proposal,evidence=self.run_repair(parts,lambda a,b:self.fail('No retained exit'))
        self.assertEqual(proposal,doc);self.assertEqual(evidence['moves'],[])

    def test_unknown_hazard_or_budget_rejections_do_not_request_relocation(self):
        for reason in ('unknown','trace_budget','hurt_trigger','water_contents'):
            doc,native,_=self.fixture();native['edges'][0]['reason']=reason
            parts=(doc,native,self.bind(doc,native))
            with self.subTest(reason=reason):
                proposal,evidence=self.run_repair(parts,lambda a,b:self.fail('Unsafe reason'))
                self.assertEqual(proposal,doc);self.assertEqual(evidence['queries'],[])

    def test_source_hash_and_current_metadata_are_bound_before_probe(self):
        for mode in ('native','candidate','metadata','revision','uid'):
            parts=self.fixture();doc,native,binding=parts
            if mode=='native':binding['native_report_bytes']+=b' '
            elif mode=='candidate':binding['candidate_bytes']+=b' '
            elif mode=='metadata':binding['candidate_metadata_bytes']+=b' '
            elif mode=='revision':doc['revision']+=1
            else:doc['nodes'][1]['way_guid']=999
            with self.subTest(mode=mode),self.assertRaises(RuntimeError):
                self.run_repair(parts,lambda a,b:self.fail('Stale source was probed'))

    def test_probe_budget_and_missing_raw_hash_fail_closed(self):
        parts=self.fixture();calls=[]
        def reject(a,b):
            calls.append((a,b));result=self.probe(a,b);result.update(stand='blocked_sweep',crouch='blocked_sweep');return result
        proposal,evidence=self.run_repair(parts,reject,max_probes=3)
        self.assertEqual(len(calls),3);self.assertEqual(proposal,parts[0]);self.assertEqual(evidence['moves'],[])
        with self.assertRaisesRegex(RuntimeError,'Probenbeleg'):
            self.run_repair(parts,lambda a,b:dict(stand='accepted',crouch='accepted'))

    def test_actual_draft_patch_can_undo_only_the_position_with_uid_and_edges_preserved(self):
        parts=self.fixture();doc,native,_=parts;proposal,evidence=self.run_repair(parts)
        with tempfile.TemporaryDirectory() as folder:
            drafts=Drafts(Path(folder));drafts.documents[doc['map']]=copy.deepcopy(doc)
            applied=drafts.apply_repair(doc['map'],doc['revision'],doc['base']['environment'],proposal)
            self.assertEqual(applied['edges'],doc['edges']);self.assertEqual(applied['nodes'][1]['way_guid'],2)
            self.assertEqual(applied['undo'][-1]['added'],[]);self.assertEqual(applied['undo'][-1]['removed'],[])
            undone=drafts.change(doc['map'],dict(op='undo',revision=applied['revision']),doc['base']['environment'])
            self.assertEqual(undone['nodes'],doc['nodes']);self.assertEqual(undone['edges'],doc['edges'])

    def reply(self,start,end,changes=None):
        result=self.probe(start,end)
        if changes:changes(result)
        vector=lambda p:'('+' '.join('%.5f'%v for v in p)+')'
        reply='navgen probe -1->-1 feet='+vector(start)+'->'+vector(end)+' stand='+result['stand']+' crouch='+result['crouch']+' traces=20 quarantined=0/0 (diagnostic only; graph unchanged)\n'
        for i,endpoint in enumerate(('start','end')):
            for duck,stance in enumerate(('stand','crouch')):
                row=result['imports'][endpoint][stance]
                reply+=f'navgen probe import endpoint={i} crouch={duck} accepted={int(row["accepted"])} exact={int(row["exact"])} reason=external_native_validated_floor feet='+vector(row['feet'])+'\n'
        return reply

    def test_real_probe_parser_binds_four_stances_and_exact_literal_roundtrip(self):
        parts=self.fixture()
        def parsed(a,b):return parse_native_probe(self.reply(a,b),a,b,30)
        proposal,evidence=self.run_repair(parts,parsed)
        self.assertEqual(proposal['nodes'][1]['position'],[64.,4.,.25])
        for query in evidence['queries']:
            self.assertEqual(query['proof']['reply_sha256'],hashlib.sha256(self.reply(query['start'],query['end']).encode()).hexdigest())
        self.assertEqual(set(evidence['queries'][0]['proof']['imports']),{'start','end'})

    def test_probe_parser_rejects_partial_duplicate_or_wrong_exact_imports(self):
        start,end=[32.,32.,0.],[64.,4.,.25];reply=self.reply(start,end)
        lines=reply.splitlines(True)
        for altered in (''.join(lines[:-1]),reply+lines[-1],reply.replace('endpoint=1 crouch=1','endpoint=0 crouch=1'),
                        reply.replace('feet=(64.00000 4.00000 0.25000)','feet=(64.00000 4.00000 1.25000)'),
                        reply.replace('accepted=1 exact=1','accepted=0 exact=1',1)):
            with self.subTest(reply=altered),self.assertRaises(RuntimeError):parse_native_probe(altered,start,end,30)

    def test_missing_import_fields_remain_legacy_compatible_but_cannot_move_anchor(self):
        parts=self.fixture()
        def old_engine(a,b):
            reply=self.reply(a,b).splitlines(True)[0]
            return parse_native_probe(reply,a,b,30)
        proposal,evidence=self.run_repair(parts,old_engine)
        self.assertEqual(proposal,parts[0]);self.assertEqual(evidence['moves'],[])


if __name__=='__main__':unittest.main()
