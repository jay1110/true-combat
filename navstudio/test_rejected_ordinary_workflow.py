"""A current rejected walk can use a proved detour, but still needs a new import."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

from edit import Drafts
from server import Studio
from test_rejected_ordinary_feedback import RejectedOrdinaryFeedbackTests


class RejectedOrdinaryWorkflowTests(unittest.TestCase):
    def fixture(self,reason='capsule_support_gap_or_drop'):
        source=RejectedOrdinaryFeedbackTests();doc,native,binding=source.fixture(rejected={(1,2):reason})
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=Path(temp.name)
        s=Studio.__new__(Studio);s.work=root;s.drafts=Drafts(root/'drafts')
        s.drafts.documents['obj_test']=copy.deepcopy(doc)
        s.draft_environment=lambda name:source.environment
        s.path=lambda name,suffix:root/(name+suffix)
        report=root/'native.json';report.write_bytes(binding['native_report_bytes'])
        candidate=root/'obj_test_candidate_edit.txt';candidate.write_bytes(binding['candidate_bytes'])
        candidate.with_suffix('.json').write_bytes(binding['candidate_metadata_bytes'])
        (root/'obj_test.native-attempt.json').write_text(json.dumps(binding['attempt']))
        s.native_path=lambda name:report;s.native=lambda name:native;s.candidate=lambda name:candidate
        s.file_hash=lambda path:hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        s.candidate_identity=lambda path:dict(sha256=s.file_hash(path),metadata=s.file_hash(path.with_suffix('.json')))
        s.proof_status=Mock(return_value=dict(inputs=binding['inputs']))
        s.report_status=Mock(return_value=binding['report_status'])
        s.prepare_draft=Mock(return_value=candidate);s.native_edit_identities=lambda name:{}
        s.action_load_map=Mock();s.quiet_generation=Mock();s.install=Mock()
        # This isolated fixture exercises edge recovery, not cross-GT assets.
        s.reconcile_objective_anchors=Mock(return_value=None)
        s.repair_path_draft=Mock(return_value=None)
        s.update=lambda job,**kwargs:job.update(kwargs)
        return s,doc

    def test_repair_resumes_current_failure_then_checks_complete_retained_graph(self):
        s,before=self.fixture();observed=[]
        s.action_repair=lambda job:s.repair_manual_draft(job)
        def check(job):
            observed.append(copy.deepcopy(s.drafts.get('obj_test')))
            return dict(audit=dict(unresolved_nodes=[],fresh=True),proof='new native proof')
        s.action_native_check=Mock(side_effect=check)
        result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(len(observed),1)
        self.assertEqual(observed[0]['nodes'],before['nodes'])
        self.assertEqual({(e['from'],e['to']) for e in observed[0]['edges']},{(1,3),(3,2),(2,1)})
        self.assertTrue(result['audit']['fresh']);self.assertFalse(result['installed'])
        self.assertIsNotNone(observed[0]['undo'][-1]);s.install.assert_not_called()
        receipt=json.loads(s.path('obj_test','.ordinary-edge-withdrawals.json').read_text())
        self.assertEqual(receipt['withdrawn_edges'][0]['alternate_source_ids'],[1,3,2])
        self.assertTrue(receipt['native_revalidation_required'])
        for source in receipt['source_evidence'].values():
            self.assertEqual(hashlib.sha256(Path(source['path']).read_bytes()).hexdigest(),source['sha256'])
        saved=json.loads(Path(receipt['source_evidence']['native_report_bytes']['path']).read_bytes())
        self.assertFalse(saved['generation_complete'])
        self.assertFalse(s.path('obj_test','.repair-rejections.json').exists())

    def test_repair_does_not_install_when_new_native_import_still_fails(self):
        s,before=self.fixture();s.action_repair=lambda job:s.repair_manual_draft(job)
        s.action_native_check=Mock(side_effect=RuntimeError('remaining real geometry error'))
        with self.assertRaisesRegex(RuntimeError,'remaining real geometry error'):
            s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(s.drafts.get('obj_test')['nodes'],before['nodes'])
        s.install.assert_not_called()

    def repeated_native_revision_archives(self,fail_second=False):
        # A regenerated graph may have exactly the same native geometry report
        # and candidate text, while its correctly bound draft revision changes
        # the metadata. Native-report-only archive names used to collide here.
        s,original=self.fixture();source=RejectedOrdinaryFeedbackTests()
        source.environment=copy.deepcopy(original['base']['environment'])
        native=copy.deepcopy(s.native('obj_test'));snapshots=[];checks=[];receipts=[]
        report_bytes=s.native_path('obj_test').read_bytes()
        candidate_bytes=s.candidate('obj_test').read_bytes()
        previous_archives={}
        def check(job):
            current=s.drafts.get('obj_test')
            self.assertEqual(current['nodes'],original['nodes'])
            self.assertNotIn({'from':1,'to':2},current['edges'])
            receipt=json.loads(s.path('obj_test','.ordinary-edge-withdrawals.json').read_bytes())
            self.assertEqual(receipt['draft_revision'],current['revision']-1)
            self.assertTrue(receipt['native_revalidation_required'])
            for record in receipt['source_evidence'].values():
                raw=Path(record['path']).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(),record['sha256'])
            checks.append(current['revision'])
            if fail_second and len(checks)==2:raise RuntimeError('second fresh import still rejects geometry')
            return dict(audit=dict(unresolved_nodes=[],fresh=True),proof=f'fresh native import {len(checks)}')
        s.action_native_check=Mock(side_effect=check)
        s.repair_pending_jump_draft=Mock(return_value=None)
        s.repair_anchor_draft=Mock(return_value=None)
        s.repair_descent_draft=Mock(return_value=None)
        for revision in(4,14):
            doc=copy.deepcopy(original);doc.update(revision=revision,saved_revision=revision)
            # Use the real raw-byte fixture binder, including revision/hash/
            # saved export/attempt agreement. Neither attempt is stale.
            bound=source.bind(doc,native)
            self.assertEqual(bound['native_report_bytes'],report_bytes)
            self.assertEqual(bound['candidate_bytes'],candidate_bytes)
            s.drafts.documents['obj_test']=doc
            s.native_path('obj_test').write_bytes(bound['native_report_bytes'])
            candidate=s.candidate('obj_test');candidate.write_bytes(bound['candidate_bytes'])
            candidate.with_suffix('.json').write_bytes(bound['candidate_metadata_bytes'])
            s.path('obj_test','.native-attempt.json').write_text(json.dumps(bound['attempt']))
            s.report_status.return_value=bound['report_status']
            s.proof_status.return_value=dict(inputs=bound['inputs'])
            if fail_second and revision==14:
                with self.assertRaisesRegex(RuntimeError,'second fresh import still rejects geometry'):
                    s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
            else:
                result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
                self.assertEqual(result['proof'],f'fresh native import {len(checks)}')
                self.assertFalse(result['installed'])
            receipt=json.loads(s.path('obj_test','.ordinary-edge-withdrawals.json').read_bytes());receipts.append(receipt)
            files={record['path']:Path(record['path']).read_bytes()for record in receipt['source_evidence'].values()}
            for path,raw in previous_archives.items():self.assertEqual(Path(path).read_bytes(),raw)
            self.assertFalse(set(previous_archives)&set(files))
            snapshots.append(files);previous_archives.update(files)
        self.assertEqual(s.action_native_check.call_count,2);self.assertEqual(checks,[5,15])
        self.assertEqual(len(previous_archives),6)
        self.assertEqual(snapshots[0][receipts[0]['source_evidence']['native_report_bytes']['path']],
                         snapshots[1][receipts[1]['source_evidence']['native_report_bytes']['path']])
        self.assertNotEqual(receipts[0]['source_evidence']['candidate_metadata_bytes']['sha256'],
                            receipts[1]['source_evidence']['candidate_metadata_bytes']['sha256'])
        self.assertNotEqual(receipts[0]['archive_bundle_sha256'],receipts[1]['archive_bundle_sha256'])
        s.install.assert_not_called()

    def test_same_native_bytes_at_two_valid_revisions_archive_separately_and_both_need_fresh_check(self):
        self.repeated_native_revision_archives()

    def test_repeated_native_bytes_and_valid_archives_never_replace_failed_second_full_check(self):
        self.repeated_native_revision_archives(fail_second=True)

    def test_server_ordinary_reason_guard_matches_current_native_collision_feedback(self):
        for reason in ('capsule_support_surface','capsule_walk_step_up','blocked_clearance'):
            s,before=self.fixture(reason)
            s.action_native_check=Mock(return_value=dict(audit=dict(unresolved_nodes=[]),proof='current native fixture proof'))
            result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
            with self.subTest(reason=reason):
                evidence=result['repair']['repairs']['withdrawn_rejected_ordinary']
                self.assertEqual(evidence['withdrawn_edges'][0]['reason'],reason)
                self.assertEqual(s.drafts.get('obj_test')['nodes'],before['nodes'])
                s.action_native_check.assert_called_once();s.install.assert_not_called()

    def test_source_change_between_proposal_and_apply_preserves_draft(self):
        s,before=self.fixture();status=s.report_status.return_value;calls=[]
        def current(name):
            calls.append(name)
            return dict(status,current=len(calls)<3)
        s.report_status.side_effect=current
        with self.assertRaisesRegex(RuntimeError,'Grundlage der Kantenreparatur verändert'):
            s.recover_rejected_ordinary(dict(map='obj_test'))
        self.assertEqual(s.drafts.get('obj_test'),before)
        self.assertFalse(s.path('obj_test','.ordinary-edge-withdrawals.json').exists())


if __name__=='__main__':unittest.main()
