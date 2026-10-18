"""Actual explicit anchor caller, source races, immutable evidence and recheck."""
import copy
import hashlib
import json
from unittest import TestCase,main
from unittest.mock import Mock

import test_anchor_repair as anchor_tests
import test_path_repair_workflow as workflow_tests


class AnchorRepairWorkflowTests(TestCase):
    def fixture(self):
        helper=workflow_tests.PathRepairWorkflowTests();self.addCleanup(helper.doCleanups)
        studio,_,_,_,_=helper.fixture()
        source=anchor_tests.AnchorRepairTests();doc,native,binding=source.fixture()
        studio.drafts.documents['obj_test']=copy.deepcopy(doc)
        studio.native_path('obj_test').write_bytes(binding['native_report_bytes'])
        candidate=studio.candidate('obj_test');candidate.write_bytes(binding['candidate_bytes'])
        candidate.with_suffix('.json').write_bytes(binding['candidate_metadata_bytes'])
        (studio.work/'obj_test.native-attempt.json').write_text(json.dumps(binding['attempt']))
        studio.proof_status.return_value=dict(valid=True,inputs=binding['inputs'])
        studio.report_status.return_value=binding['report_status']
        studio.native_edit_identities=Mock(side_effect=AssertionError('Relocation must preserve existing UID'))
        def command(text,read_only):
            self.assertTrue(read_only);self.assertTrue(text.startswith('tce_navgen_probe feet '))
            values=list(map(float,text.split()[2:]));self.assertEqual(len(values),6)
            return source.reply(values[:3],values[3:])
        studio.rcon=Mock(side_effect=command)
        studio.recover_rejected_ordinary=Mock(return_value=None)
        studio.recover_rejected_shortcuts=Mock(return_value=None)
        studio.repair_path_draft=Mock(return_value=None)
        studio.action_native_check=Mock(return_value=dict(audit=dict(unresolved_nodes=[],fresh=True),proof='current native fixture proof'))
        return studio,doc,source,command

    def test_actual_server_to_pure_repair_keeps_uid_all_edges_and_archives_raw_probe_bytes(self):
        s,doc,_,_=self.fixture();before=s.native_path('obj_test').read_bytes()
        evidence=s.repair_anchor_draft(dict(map='obj_test',action='repair_check',log=''))
        current=s.drafts.get('obj_test')
        self.assertEqual(current['nodes'][1]['position'],[64.,4.,.25])
        self.assertEqual(current['nodes'][1]['way_guid'],2);self.assertEqual(current['edges'],doc['edges'])
        self.assertEqual(s.native_path('obj_test').read_bytes(),before)
        s.native_edit_identities.assert_not_called();s.install.assert_not_called()
        self.assertTrue(evidence['native_revalidation_required']);self.assertFalse(evidence['installed'])
        for item in evidence['source_evidence'].values():
            from pathlib import Path
            self.assertEqual(hashlib.sha256(Path(item['path']).read_bytes()).hexdigest(),item['sha256'])
        from pathlib import Path
        replies=json.loads(Path(evidence['source_evidence']['probe_reply_bytes']['path']).read_bytes())
        self.assertEqual(len(replies),len(evidence['queries']))
        for row in replies:self.assertEqual(hashlib.sha256(row['reply'].encode()).hexdigest(),row['sha256'])
        attempt=json.loads(Path(evidence['source_evidence']['native_attempt_bytes']['path']).read_bytes())
        self.assertEqual(evidence['source_inputs'],attempt['inputs'])
        from edit import graph_hash
        self.assertEqual(evidence['source_graph_sha256'],graph_hash(doc))
        self.assertEqual(evidence['draft_revision'],doc['revision'])
        self.assertEqual(evidence['candidate'],attempt['candidate'])
        self.assertEqual(len(list(s.work.glob('*.anchor-repair-*.receipt.json'))),1)

    def test_check_generate_et_and_already_attempted_never_probe_or_move(self):
        for mode in ('native_check','generate','et','already'):
            s,doc,_,_=self.fixture();job=dict(map='obj_test',action='repair_check',log='')
            if mode in ('native_check','generate'):job['action']=mode
            elif mode=='et':s.game_mode='et'
            else:job['_native_anchor_search_attempted']=True
            with self.subTest(mode=mode):
                self.assertIsNone(s.repair_anchor_draft(job));self.assertEqual(s.drafts.get('obj_test'),doc)
                s.rcon.assert_not_called();s.install.assert_not_called()

    def test_source_or_profile_changes_during_probe_cannot_apply_position(self):
        for mode in ('native','candidate','metadata','attempt','draft','inputs','profile','freshness'):
            s,doc,_,command=self.fixture();changed=[]
            def race(text,read_only):
                reply=command(text,read_only)
                if not changed:
                    changed.append(True)
                    if mode=='native':s.native_path('obj_test').write_bytes(s.native_path('obj_test').read_bytes()+b' ')
                    elif mode=='candidate':s.candidate('obj_test').write_bytes(s.candidate('obj_test').read_bytes()+b' ')
                    elif mode=='metadata':
                        path=s.candidate('obj_test').with_suffix('.json');path.write_bytes(path.read_bytes()+b' ')
                    elif mode=='attempt':
                        path=s.work/'obj_test.native-attempt.json';path.write_bytes(path.read_bytes()+b' ')
                    elif mode=='draft':s.drafts.documents['obj_test']['nodes'][0]['radius']=36
                    elif mode=='inputs':s.proof_status.return_value=copy.deepcopy(s.proof_status.return_value);s.proof_status.return_value['inputs']['game_module']='0'*64
                    elif mode=='profile':s.require_bbox.return_value=dict(s.require_bbox.return_value,radius=17)
                    else:s.report_status.return_value=dict(s.report_status.return_value,current=False)
                return reply
            s.rcon.side_effect=race
            with self.subTest(mode=mode),self.assertRaisesRegex(RuntimeError,'Grundlage'):
                s.repair_anchor_draft(dict(map='obj_test',action='repair_check',log=''))
            current=s.drafts.get('obj_test');self.assertEqual(current['nodes'][1],doc['nodes'][1])
            self.assertEqual(current['revision'],doc['revision']);s.install.assert_not_called()

    def test_stale_binding_is_rejected_before_first_native_query(self):
        s,doc,_,_=self.fixture();s.candidate('obj_test').write_bytes(b'changed')
        with self.assertRaises(RuntimeError):s.repair_anchor_draft(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(s.drafts.get('obj_test'),doc);s.rcon.assert_not_called()

    def test_fresh_import_rejection_is_diagnostic_only_and_preserves_entire_draft(self):
        s,doc,source,_=self.fixture()
        def reject(text,read_only):
            values=list(map(float,text.split()[2:]))
            def change(result):
                for end in result['imports'].values():
                    for row in end.values():row.update(accepted=False,exact=False)
            return source.reply(values[:3],values[3:],change)
        s.rcon.side_effect=reject
        evidence=s.repair_anchor_draft(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(evidence['moves'],[]);self.assertEqual(s.drafts.get('obj_test'),doc)
        self.assertLessEqual(s.rcon.call_count,96);s.install.assert_not_called()

    def test_repair_check_runs_full_native_validation_after_actual_position_patch(self):
        s,doc,_,_=self.fixture();observed=[]
        def check(job):
            observed.append(copy.deepcopy(s.drafts.get('obj_test')))
            return dict(audit=dict(unresolved_nodes=[],fresh=True),proof='current native fixture proof')
        s.action_native_check.side_effect=check
        result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(len(observed),1);self.assertEqual(observed[0]['nodes'][1]['position'],[64.,4.,.25])
        self.assertEqual(observed[0]['edges'],doc['edges'])
        self.assertEqual(len(result['repair']['repairs']['native_anchor_relocations']['moves']),1)
        self.assertTrue(result['audit']['fresh']);self.assertFalse(result['installed']);s.install.assert_not_called()


if __name__=='__main__':main()
