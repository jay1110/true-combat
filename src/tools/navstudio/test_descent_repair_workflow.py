"""Explicit ordinary-first repair resumes native descents once, never Mesh."""
import copy
import hashlib
import json
from pathlib import Path
from unittest import TestCase,main
from unittest.mock import Mock,patch

import test_native_descent_repair as descent_tests
import test_path_repair_workflow as workflow_tests
import test_path_repair as path_tests
import test_rejected_ordinary_feedback as ordinary_tests
import test_spawn_terminal_descent as spawn_tests
from edit import graph_hash


class DescentRepairWorkflowTests(TestCase):
    def complete_spawn_fixture(self):
        helper=workflow_tests.PathRepairWorkflowTests();self.addCleanup(helper.doCleanups)
        s,_,_,_,_=helper.fixture();prototype,_,positions=spawn_tests.SpawnTerminalTests().fixture()
        factory=ordinary_tests.RejectedOrdinaryFeedbackTests()
        pairs=[(e['from'],e['to'])for e in prototype['edges']]
        doc,native,_=factory.fixture(positions,pairs,{(-1,-1):'unused'})
        doc['base']['movement_profile']=dict(radius=16,standing_height=87,crouching_height=61,origin_above_feet=30,step=18)
        native.update(generation_complete=True,traversal_physics=dict(gravity=1000))
        native['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        for node,row in zip(doc['nodes'],native['nodes']):
            node.update(spawn=node['id']in(958,862),source_reason='map_entity_spawn_seed'if node['id']in(958,862)else'external_native_validated_floor')
            row.update(reason=node['source_reason'],editor_role=0)
            row['native_input'].update(floor_reason=node['source_reason'],status='spawn_reachable')
        s.drafts.documents['obj_test']=doc
        def bind(current,report):
            current['saved_revision']=current['revision'];bound=path_tests.PathRepairTests().bind(current,report)
            bound['attempt']['generation_complete']=report['generation_complete']
            s.native_path('obj_test').write_bytes(bound['native_report_bytes'])
            candidate=s.candidate('obj_test');candidate.write_bytes(bound['candidate_bytes'])
            candidate.with_suffix('.json').write_bytes(bound['candidate_metadata_bytes'])
            (s.work/'obj_test.native-attempt.json').write_text(json.dumps(bound['attempt']))
            s.proof_status.return_value=dict(valid=True,inputs=bound['inputs']);s.report_status.return_value=bound['report_status']
            return bound
        binding=bind(doc,native);query,_=spawn_tests.SpawnTerminalTests().probe(positions,'drop_only')
        vector=lambda p:'('+' '.join('%.5f'%v for v in p)+')'
        def command(text,read_only):
            self.assertTrue(read_only);self.assertTrue(text.startswith('tce_navgen_probe feet '))
            values=list(map(float,text.split()[2:]));a,b=values[:3],values[3:];proof=query(a,b)
            reply=f'navgen probe -1->-1 feet={vector(a)}->{vector(b)} stand={proof["stand"]} crouch={proof["stand"]} traces=10 quarantined=0/0 (diagnostic only; graph unchanged)'
            for endpoint,key in((0,'start'),(1,'end')):
                for crouch,stance in((0,'stand'),(1,'crouch')):
                    row=proof['imports'][key][stance]
                    reply+=f'\nnavgen probe import endpoint={endpoint} crouch={crouch} accepted={int(row["accepted"])} exact={int(row["exact"])} reason={row["reason"]} feet={vector(row["feet"])}'
                row=proof['action_imports'][key]['drop_launch'];walk=proof['action_walks'][key]['drop_launch']
                reply+=f'\nnavgen probe action_import kind=drop_launch endpoint={endpoint} accepted={int(row["accepted"])} exact={int(row["exact"])} reason={row["reason"]} feet={vector(row["feet"])}'
                reply+=f'\nnavgen probe action_walk kind=drop_launch endpoint={endpoint} accepted={int(walk["accepted"])} reason={walk["reason"]}'
            if b[2]<a[2]-18.25:
                drop=proof['drop'];reply+=f'\nnavgen probe drop -1->-1 accepted={int(drop["accepted"])} reason=probe crash_limit=38 max_drop=148.4375 delta={drop["delta"]} landing={vector(b[:2]+[b[2]+30])} (readonly samples)'
            if 'free_climb'in proof or b[2]-a[2]>=63:
                climb=proof.get('free_climb',dict(accepted=False,launch=b,duration=0,stamina_used=0))
                launch=climb['launch'];reply+=f'\nnavgen probe freeclimb -1->-1 accepted={int(climb["accepted"])} launch={vector(launch[:2]+[launch[2]+30])} max_ledge=126.25 duration={climb["duration"]} stamina_used={climb["stamina_used"]} (readonly3speed samples; graph unchanged)'
            return reply
        s.rcon=Mock(side_effect=command);s.native_edit_identities=Mock(side_effect=AssertionError('No UID adoption'))
        s.recover_rejected_shortcuts=Mock(return_value=None);s.recover_rejected_ordinary=Mock(return_value=None)
        s.repair_pending_jump_draft=Mock(return_value=None);s.repair_path_draft=Mock(return_value=None)
        s.repair_anchor_draft=Mock(return_value=None)
        return s,copy.deepcopy(doc),native,binding,bind

    def fixture(self):
        helper=workflow_tests.PathRepairWorkflowTests();self.addCleanup(helper.doCleanups)
        studio,_,_,_,_=helper.fixture()
        factory=ordinary_tests.RejectedOrdinaryFeedbackTests()
        doc,native,_=factory.fixture(
            positions={3:[0.,0.,102.25],99:[64.,-110.,.25],120:[256.,0.,.25],
                       130:[500.,0.,102.25],131:[564.,0.,102.25],132:[532.,32.,102.25]},
            edges=[(3,99),(120,99),(130,131),(130,132),(132,131),(131,130)],
            rejected={(3,99):'capsule_support_gap_or_drop',(130,131):'capsule_support_gap_or_drop'})
        doc['base']['movement_profile']=dict(radius=16,standing_height=87,crouching_height=61,origin_above_feet=30,step=18)
        native['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        native['traversal_physics']=dict(gravity=1000)
        for node,row in zip(doc['nodes'],native['nodes']):
            node.update(name='',facing=[0.,0.,0.],source_reason='external_native_validated_floor')
            row.update(name='',facing=[0.,0.,0.],editor_role=0)
            row['native_input']['status']='spawn_reachable'
            if node['id']==99:
                row.update(accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn')
                row['native_input']['status']='missing_spawn_route'
            if node['id']==120:
                node.update(nav_flags=(1<<27)|(1<<55),generated_guide=True)
                row.update(nav_flags=node['nav_flags'],editor_role=1)
        for edge in native['edges']:
            if (edge['from'],edge['to'])==(120,99):
                edge.update(accepted=False,reason='quarantined_unreachable_endpoint')
        studio.drafts.documents['obj_test']=doc
        def bind(current,report):
            current['saved_revision']=current['revision']
            bound=path_tests.PathRepairTests().bind(current,report)
            studio.native_path('obj_test').write_bytes(bound['native_report_bytes'])
            candidate=studio.candidate('obj_test');candidate.write_bytes(bound['candidate_bytes'])
            candidate.with_suffix('.json').write_bytes(bound['candidate_metadata_bytes'])
            (studio.work/'obj_test.native-attempt.json').write_text(json.dumps(bound['attempt']))
            studio.proof_status.return_value=dict(valid=True,inputs=bound['inputs'])
            studio.report_status.return_value=bound['report_status']
            return bound
        binding=bind(doc,native)
        studio.native_edit_identities=Mock(side_effect=AssertionError('Do not adopt existing UIDs'))
        def command(text,read_only):
            self.assertTrue(read_only);self.assertTrue(text.startswith('tce_navgen_probe feet '))
            values=list(map(float,text.split()[2:]));self.assertEqual(len(values),6)
            return self.reply(values[:3],values[3:])
        studio.rcon=Mock(side_effect=command)
        studio.recover_rejected_shortcuts=Mock(return_value=None)
        studio.repair_path_draft=Mock(return_value=None)
        studio.repair_anchor_draft=Mock(return_value=None)
        return studio,copy.deepcopy(doc),native,binding,bind

    def reply(self,a,b,rejected=False):
        result=descent_tests.NativeDescentRepairTests().probe(a,b)
        if rejected:result=dict(stand='capsule_support_gap_or_drop')
        vector=lambda p:'('+' '.join('%.5f'%v for v in p)+')'
        reply=(f'navgen probe -1->-1 feet={vector(a)}->{vector(b)} stand={result["stand"]} '
               f'crouch={result["stand"]} traces=10 quarantined=0/0 (diagnostic only; graph unchanged)')
        if b[2]<a[2]-18.25:
            drop=result.get('drop',{})
            reply+=(f'\nnavgen probe drop -1->-1 accepted={int(drop.get("accepted",False))} reason=probe '
                    f'crash_limit=38 max_drop=148.4375 delta={drop.get("delta",0)} '
                    f'landing={vector(b[:2]+[b[2]+30])} (readonly samples)')
        if b[2]-a[2]>=63:
            climb=result.get('free_climb',{})
            reply+=(f'\nnavgen probe freeclimb -1->-1 accepted={int(climb.get("accepted",False))} '
                    'launch=(0 -64 30.25) max_ledge=126.25 duration=0.96 stamina_used=870 '
                    '(readonly3speed samples; graph unchanged)')
        return reply

    def test_actual_ordinary_first_caller_probes_remaining_q_descent_then_rechecks_whole_graph(self):
        s,doc,native,_,bind=self.fixture();events=[];checks=[]
        original=s.rcon.side_effect
        def probe(*args,**kwargs):events.append('probe');return original(*args,**kwargs)
        s.rcon.side_effect=probe
        s.repair_path_draft.side_effect=lambda job:events.append('path')
        s.repair_anchor_draft.side_effect=lambda job:events.append('anchor')
        def check(job):
            current=s.drafts.get('obj_test');checks.append(copy.deepcopy(current));events.append('check')
            if len(checks)==1:
                report=copy.deepcopy(native)
                pairs={(e['from'],e['to'])for e in current['edges']}
                report['edges']=[e for e in report['edges']if(e['from'],e['to'])in pairs]
                bind(current,report)
                raise RuntimeError('checked floor 99 still has no path')
            return dict(audit=dict(unresolved_nodes=[],fresh=True),proof='current native fixture proof')
        s.action_native_check=Mock(side_effect=check)
        with patch('repair.propose',side_effect=AssertionError('No new Mesh shortcuts')):
            result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(len(checks),2)
        self.assertEqual(checks[0]['nodes'],doc['nodes'])
        self.assertNotIn({'from':130,'to':131},checks[0]['edges'])
        self.assertIn({'from':3,'to':99},checks[0]['edges'])
        self.assertNotIn({'from':3,'to':99},checks[1]['edges'])
        self.assertEqual(len(checks[1]['nodes']),len(doc['nodes'])+2)
        self.assertEqual(events[:3],['check','path','anchor'])
        self.assertEqual(events[-1],'check');self.assertIn('probe',events[3:-1])
        self.assertEqual(result['repair']['native_action_repairs'][0]['quarantined_floor_probe_source_ids'],[99])
        self.assertTrue(result['audit']['fresh']);self.assertFalse(result['installed'])
        s.install.assert_not_called();s.native_edit_identities.assert_not_called()

    def test_complete_physical_source_allows_only_bound_native_spawn_component_not_stale_or_other_complete_graph(self):
        for mode in('spawn','not_native_spawn','already_connected','stale_candidate'):
            s,doc,native,binding,bind=self.complete_spawn_fixture()
            if mode=='not_native_spawn':
                row=next(n for n in native['nodes']if n['id']==958)
                row['native_input']['floor_reason']='external_native_validated_floor';binding=bind(s.drafts.get('obj_test'),native)
            elif mode=='already_connected':
                for a,b in((957,852),(852,957)):
                    s.drafts.get('obj_test')['edges'].append({'from':a,'to':b})
                    native['edges'].append(dict(**{'from':a,'to':b},accepted=True,reason='external_native_validated_walk',crouch=False))
                binding=bind(s.drafts.get('obj_test'),native)
            elif mode=='stale_candidate':s.candidate('obj_test').write_bytes(b'changed')
            before=copy.deepcopy(s.drafts.get('obj_test'));job=dict(map='obj_test',action='repair_check',log='')
            with self.subTest(mode=mode):
                if mode=='stale_candidate':
                    with self.assertRaises(RuntimeError):s.repair_descent_draft(job)
                    s.rcon.assert_not_called();self.assertEqual(s.drafts.get('obj_test'),before)
                else:
                    result=s.repair_descent_draft(job)
                    if mode=='spawn':
                        self.assertTrue(result['added_nodes']);self.assertTrue(job['_native_descent_search_attempted'])
                        self.assertEqual(result['connected_native_spawn_components'],[[957,958]])
                        route=next(r for r in result['routes']if r.get('chain'))
                        self.assertTrue(route['spawned_entry_only']);self.assertIsNone(route['return_chain'])
                        archived=json.loads(Path(result['source_evidence']['native_report_bytes']['path']).read_bytes())
                        self.assertTrue(archived['generation_complete']);self.assertEqual(archived,native)
                        self.assertTrue(result['native_revalidation_required']);self.assertFalse(result['installed'])
                        self.assertEqual(result['source_inputs'],binding['inputs']);self.assertLessEqual(s.rcon.call_count,96)
                    else:self.assertIsNone(result);s.rcon.assert_not_called();self.assertEqual(s.drafts.get('obj_test'),before)
            s.install.assert_not_called();s.native_edit_identities.assert_not_called()

    def test_successful_physical_import_with_stale_clean_audit_still_repairs_spawn_cycle_then_rechecks_full_graph(self):
        for final_fails in(False,True):
            s,doc,native,_,bind=self.complete_spawn_fixture();events=[];checks=[]
            s.action_repair=Mock(return_value=dict(repairs={}))
            command=s.rcon.side_effect
            def probe(*args,**kwargs):events.append('probe');return command(*args,**kwargs)
            s.rcon.side_effect=probe
            def check(job):
                checks.append(copy.deepcopy(s.drafts.get('obj_test')));events.append('check')
                if len(checks)==1:
                    # The native source is physically complete, while its
                    # actual accepted graph has SCC[4,2]. A cached clean UI
                    # audit must not prevent the bounded real repair call.
                    return dict(audit=dict(unresolved_nodes=[],fresh=True),proof='physical-only proof')
                self.assertGreater(len(checks[-1]['nodes']),len(doc['nodes']))
                if final_fails:raise RuntimeError('new whole graph failed after spawn repair')
                merged=copy.deepcopy(native);origin=30
                old={n['id']:n for n in merged['nodes']}
                for node in checks[-1]['nodes']:
                    if node['id']not in old:
                        row=copy.deepcopy(merged['nodes'][0]);row.update(id=node['id'],raw_position=node['position'][:2]+[node['position'][2]+origin],
                            position=node['position'][:2]+[node['position'][2]+origin],nav_flags=node['nav_flags'],crouch=node['crouch'],
                            import_uid=0,way_guid=0,name='',facing=[0.,0.,0.],custom_properties={})
                        merged['nodes'].append(row)
                merged['edges']=[dict(e,accepted=True,reason='manual_native_transition_live_pending',crouch=False)for e in checks[-1]['edges']]
                bind(s.drafts.get('obj_test'),merged)
                return dict(audit=dict(unresolved_nodes=[],fresh=True),proof='fresh repaired whole-graph proof')
            s.action_native_check=Mock(side_effect=check)
            with self.subTest(final_fails=final_fails):
                if final_fails:
                    with self.assertRaisesRegex(RuntimeError,'whole graph failed'):s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
                else:
                    result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
                    self.assertEqual(result['proof'],'fresh repaired whole-graph proof')
                    self.assertEqual(result['repair']['native_action_repairs'][0]['connected_native_spawn_components'],[[957,958]])
                    self.assertFalse(result['installed'])
                self.assertEqual(len(checks),2);self.assertEqual(events[0],'check');self.assertEqual(events[-1],'check')
                self.assertIn('probe',events[1:-1]);self.assertLessEqual(s.rcon.call_count,96)
                s.install.assert_not_called();s.native_edit_identities.assert_not_called()

    def test_literal_sources_attempt_and_probe_replies_are_immutable_and_replayable(self):
        s,doc,_,binding,_=self.fixture();job=dict(map='obj_test',action='repair_check',log='')
        evidence=s.repair_descent_draft(job)
        self.assertEqual(evidence['added_nodes'],2);self.assertTrue(job['_native_descent_search_attempted'])
        self.assertEqual(evidence['source_graph_sha256'],graph_hash(doc))
        self.assertEqual(evidence['source_inputs'],binding['inputs'])
        self.assertEqual(evidence['draft_revision'],doc['revision'])
        self.assertEqual(evidence['candidate'],binding['attempt']['candidate'])
        for key,item in evidence['source_evidence'].items():
            raw=Path(item['path']).read_bytes();self.assertEqual(hashlib.sha256(raw).hexdigest(),item['sha256'])
            if key in binding:self.assertEqual(raw,binding[key])
        replies=json.loads(Path(evidence['source_evidence']['probe_reply_bytes']['path']).read_bytes())
        from repair import parse_native_probe,propose_descents
        replay=iter(replies)
        def probe(a,b):
            row=next(replay);self.assertEqual((a,b),(row['start'],row['end']))
            self.assertEqual(hashlib.sha256(row['reply'].encode()).hexdigest(),row['sha256'])
            return parse_native_probe(row['reply'],a,b,30)
        replayed,receipt=propose_descents(doc,json.loads(binding['native_report_bytes']),probe)
        self.assertEqual(receipt['routes'],evidence['routes'])
        self.assertEqual(replayed['nodes'],s.drafts.get('obj_test')['nodes'])
        self.assertLessEqual(len(replies),96)
        self.assertIsNone(s.repair_descent_draft(job));self.assertEqual(s.rcon.call_count,len(replies))

    def test_check_generate_et_and_already_attempted_do_not_probe(self):
        for mode in('native_check','generate','et','already'):
            s,doc,_,_,_=self.fixture();job=dict(map='obj_test',action='repair_check',log='')
            if mode in('native_check','generate'):job['action']=mode
            elif mode=='et':s.game_mode='et'
            else:job['_native_descent_search_attempted']=True
            with self.subTest(mode=mode):self.assertIsNone(s.repair_descent_draft(job))
            self.assertEqual(s.drafts.get('obj_test'),doc);s.rcon.assert_not_called()

    def test_rejected_native_probes_preserve_graph_and_do_not_publish_success(self):
        s,doc,_,_,_=self.fixture()
        def reject(text,read_only):
            values=list(map(float,text.split()[2:]));return self.reply(values[:3],values[3:],True)
        s.rcon.side_effect=reject
        evidence=s.repair_descent_draft(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(evidence['added_nodes'],0);self.assertEqual(evidence['replaced_rejected_edges'],[])
        self.assertEqual(s.drafts.get('obj_test'),doc);self.assertLessEqual(s.rcon.call_count,96)
        s.install.assert_not_called()

    def test_stale_binding_rejects_before_first_probe(self):
        s,doc,_,_,_=self.fixture();s.candidate('obj_test').write_bytes(b'changed')
        with self.assertRaises(RuntimeError):s.repair_descent_draft(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(s.drafts.get('obj_test'),doc);s.rcon.assert_not_called()

    def test_source_and_profile_races_cannot_apply_native_action(self):
        for mode in('native','candidate','metadata','attempt','draft','inputs','profile','freshness'):
            s,doc,_,_,_=self.fixture();command=s.rcon.side_effect;changed=[]
            def race(text,read_only):
                reply=command(text,read_only)
                if not changed:
                    changed.append(True)
                    if mode in('native','candidate','metadata','attempt'):
                        path={'native':s.native_path('obj_test'),'candidate':s.candidate('obj_test'),
                              'metadata':s.candidate('obj_test').with_suffix('.json'),
                              'attempt':s.work/'obj_test.native-attempt.json'}[mode]
                        path.write_bytes(path.read_bytes()+b' ')
                    elif mode=='draft':s.drafts.documents['obj_test']['nodes'][0]['radius']=36
                    elif mode=='inputs':s.proof_status.return_value=copy.deepcopy(s.proof_status.return_value);s.proof_status.return_value['inputs']['game_module']='0'*64
                    elif mode=='profile':s.require_bbox.return_value=dict(s.require_bbox.return_value,radius=17)
                    else:s.report_status.return_value=dict(s.report_status.return_value,current=False)
                return reply
            s.rcon.side_effect=race
            with self.subTest(mode=mode),self.assertRaisesRegex(RuntimeError,'Grundlage'):
                s.repair_descent_draft(dict(map='obj_test',action='repair_check',log=''))
            current=s.drafts.get('obj_test')
            self.assertEqual(current['edges'],doc['edges']);self.assertEqual(current['revision'],doc['revision'])
            self.assertEqual(len(current['nodes']),len(doc['nodes']));s.install.assert_not_called()

    def test_fifth_failed_full_check_does_not_start_an_uncheckable_descent_patch(self):
        s,_,_,_,_=self.fixture()
        s.action_repair=Mock(return_value=dict(repairs={}))
        s.recover_rejected_ordinary=Mock(return_value=dict(model='native_rejected_edge_alternate_v2'))
        s.action_native_check=Mock(side_effect=RuntimeError('still unsafe'))
        s.repair_descent_draft=Mock(side_effect=AssertionError('No remaining full check'))
        with self.assertRaisesRegex(RuntimeError,'still unsafe'):
            s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(s.action_native_check.call_count,5);s.repair_descent_draft.assert_not_called()

    def test_failed_full_check_after_descent_keeps_proposal_uninstalled_and_never_reprobes(self):
        s,_,_,_,_=self.fixture();job=dict(map='obj_test',action='repair_check',log='')
        s.action_repair=Mock(return_value=dict(repairs={}))
        s.recover_rejected_ordinary=Mock(return_value=None)
        s.action_native_check=Mock(side_effect=RuntimeError('whole graph still unsafe'))
        with patch('repair.native_crouch_feedback',return_value=None),self.assertRaisesRegex(RuntimeError,'still unsafe'):
            s.action_repair_check(job)
        self.assertEqual(s.action_native_check.call_count,2)
        self.assertTrue(job['_native_descent_search_attempted']);self.assertLessEqual(s.rcon.call_count,96)
        self.assertEqual(len(list(s.work.glob('*.descent-repair-*.receipt.json'))),1)
        self.assertFalse(s.native_path('obj_test').with_suffix('.way').exists());s.install.assert_not_called()


if __name__=='__main__':main()
