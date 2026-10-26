"""Explicit native path repair is append-only, source-bound and never installs."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

from edit import Drafts
from server import Studio, MOVEMENT_PROFILES
import test_rejected_ordinary_feedback as ordinary_tests


class PathRepairWorkflowTests(unittest.TestCase):
    def fixture(self):
        source=ordinary_tests.RejectedOrdinaryFeedbackTests()
        doc,native,_=source.fixture()
        native['bbox'].update(half_width=16,stand_height=87,crouch_height=61)
        binding=source.bind(doc,native)
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=Path(temp.name)
        studio=Studio.__new__(Studio);studio.work=root;studio.drafts=Drafts(root/'drafts')
        studio.drafts.documents[doc['map']]=copy.deepcopy(doc);studio.game_mode='tce2'
        studio.path=lambda name,suffix:root/(name+suffix)
        original=root/'native.json';original.write_bytes(binding['native_report_bytes'])
        candidate=root/binding['report_status']['candidate'];candidate.write_bytes(binding['candidate_bytes'])
        candidate.with_suffix('.json').write_bytes(binding['candidate_metadata_bytes'])
        (root/'obj_test.native-attempt.json').write_text(json.dumps(binding['attempt']))
        runtime=root/'runtime.json';runtime.write_text('{}')
        nav=root/'engine';nav.mkdir()
        studio.native_path=lambda name:original;studio.runtime_native_path=lambda name:runtime
        studio.native=lambda name:json.loads(original.read_bytes())
        studio.candidate=lambda name:candidate;studio.prepare_draft=Mock(return_value=candidate)
        studio.engine_nav_root=lambda:nav;studio.atomic_copy=shutil.copyfile
        studio.file_hash=lambda p:hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None
        studio.candidate_identity=lambda p:dict(sha256=studio.file_hash(p),metadata=studio.file_hash(p.with_suffix('.json')))
        studio.proof_status=Mock(return_value=dict(valid=True,inputs=binding['inputs']))
        studio.report_status=Mock(return_value=binding['report_status'])
        studio.require_loaded=Mock();studio.require_bbox=Mock(return_value=dict(MOVEMENT_PROFILES[1]))
        studio.require_profile_inputs=Mock();studio.update=lambda job,**kw:job.update(kw)
        studio.draft_environment=lambda name:source.environment;studio.native_edit_identities=lambda name:{}
        studio.install=Mock();studio.action_load_map=Mock();studio.quiet_generation=Mock()
        # Inherited anchor/descent fixtures share this deliberate boundary.
        studio.reconcile_objective_anchors=Mock(return_value=None)
        report=copy.deepcopy(native);report['path_repair']=dict(explicit_repair=True)
        def command(text):runtime.write_text(json.dumps(report));return 'native path repair proposal'
        studio.rcon_with_followup=Mock(side_effect=command)
        studio.wait_fresh=Mock(side_effect=lambda *args:json.loads(runtime.read_bytes()))
        proposal=copy.deepcopy(doc);new=copy.deepcopy(doc['nodes'][0])
        new.update(id=doc['next_id'],position=[32,16,0],way_guid=0,generated_guide=True,editor_role=1)
        proposal['nodes'].append(new);proposal['next_id']+=1
        proposal['edges'] += [{'from':1,'to':new['id']},{'from':new['id'],'to':2}]
        # The merge module has its own geometry/metadata regression suite.
        # Here the seam returns an append-only proposal, allowing independent
        # tests of live-source races, immutable receipts and retry ordering.
        module=types.SimpleNamespace(validate_source=Mock(return_value=True),merge=Mock(return_value=(proposal,
            dict(added_node_ids=[new['id']],native_revalidation_required=True))))
        return studio,doc,native,module,report

    def test_current_partial_report_appends_without_complete_way_and_archives_literal_evidence(self):
        s,doc,native,module,_=self.fixture();before=s.native_path('obj_test').read_bytes()
        with patch.dict('sys.modules',pathrepair=module):
            evidence=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
        current=s.drafts.get('obj_test')
        self.assertEqual(current['nodes'][:len(doc['nodes'])],doc['nodes'])
        self.assertTrue({(e['from'],e['to']) for e in doc['edges']} <= {(e['from'],e['to']) for e in current['edges']})
        self.assertEqual(s.native_path('obj_test').read_bytes(),before)
        self.assertFalse(s.native_path('obj_test').with_suffix('.way').exists())
        self.assertFalse(native['generation_complete'])
        s.rcon_with_followup.assert_called_once_with('tce_navgen_import navgen/obj_test_candidate_edit.txt repair_paths 4')
        s.install.assert_not_called()
        for item in evidence['source_evidence'].values():
            self.assertEqual(hashlib.sha256(Path(item['path']).read_bytes()).hexdigest(),item['sha256'])
        attempt=json.loads(Path(evidence['source_evidence']['native_attempt_bytes']['path']).read_bytes())
        self.assertEqual(evidence['source_inputs'],attempt['inputs'])
        from edit import graph_hash
        self.assertEqual(evidence['source_graph_sha256'],graph_hash(doc))
        self.assertEqual(evidence['draft_revision'],doc['revision'])
        self.assertEqual(evidence['candidate'],attempt['candidate'])
        self.assertEqual(len(list(s.work.glob('*.path-repair-*.receipt.json'))),1)
        passed_binding=module.merge.call_args.args[-1]
        self.assertEqual(passed_binding['native_report_bytes'],before)
        self.assertEqual(json.loads(passed_binding['repair_report_bytes']),json.loads(s.runtime_native_path('obj_test').read_bytes()))

    def test_check_generate_et_and_second_search_never_call_native_repair(self):
        for mode in ('native_check','generate','et','already'):
            s,doc,_,module,_=self.fixture();job=dict(map='obj_test',action='repair_check',log='')
            if mode in ('native_check','generate'):job['action']=mode
            elif mode=='et':s.game_mode='et'
            else:job['_native_path_search_attempted']=True
            with patch.dict('sys.modules',pathrepair=module):
                self.assertIsNone(s.repair_path_draft(job))
            self.assertEqual(s.drafts.get('obj_test'),doc);s.rcon_with_followup.assert_not_called()

    def test_identical_native_result_with_fresh_attempt_gets_separate_immutable_archive(self):
        s,doc,_,module,_=self.fixture()
        module.merge.side_effect=lambda *args:(copy.deepcopy(doc),dict(added_node_ids=[],added_directed_edges=0,native_revalidation_required=True))
        attempt=s.work/'obj_test.native-attempt.json'
        old=json.loads(attempt.read_bytes());old['created']=1792098920.8408003;attempt.write_text(json.dumps(old))
        with patch.dict('sys.modules',pathrepair=module):
            first=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
            original={p:p.read_bytes()for p in s.work.glob('*.path-repair-*')}
            repeated=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
            self.assertEqual(repeated,first)
            self.assertEqual({p:p.read_bytes()for p in s.work.glob('*.path-repair-*')},original)
            old['created']=1792102199.7054174;attempt.write_text(json.dumps(old))
            second=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(first['repair_report_sha256'],second['repair_report_sha256'])
        self.assertNotEqual(first['archive_bundle_sha256'],second['archive_bundle_sha256'])
        self.assertEqual(s.drafts.get('obj_test'),doc);s.install.assert_not_called()
        for path,data in original.items():self.assertEqual(path.read_bytes(),data)
        self.assertEqual(len(list(s.work.glob('*.path-repair-*.receipt.json'))),2)
        for evidence,created in ((first,1792098920.8408003),(second,1792102199.7054174)):
            sources=evidence['source_evidence']
            self.assertEqual(json.loads(Path(sources['native_attempt_bytes']['path']).read_bytes())['created'],created)
            hashes={key:item['sha256']for key,item in sources.items()}
            expected=hashlib.sha256(json.dumps(hashes,sort_keys=True,separators=(',',':')).encode('utf8')).hexdigest()
            self.assertEqual(evidence['archive_bundle_sha256'],expected)
            for item in sources.values():self.assertEqual(hashlib.sha256(Path(item['path']).read_bytes()).hexdigest(),item['sha256'])

    def test_changed_bundle_member_is_bound_and_existing_archive_corruption_is_never_overwritten(self):
        s,doc,_,module,_=self.fixture()
        module.merge.return_value=(copy.deepcopy(doc),dict(added_node_ids=[],added_directed_edges=0))
        with patch.dict('sys.modules',pathrepair=module):
            first=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
            archived=Path(first['source_evidence']['native_attempt_bytes']['path'])
            archived.write_bytes(b'preserve even this conflicting evidence')
            with self.assertRaisesRegex(RuntimeError,'Archivierter Pfadpr'):
                s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(archived.read_bytes(),b'preserve even this conflicting evidence')
        self.assertEqual(s.drafts.get('obj_test'),doc);s.install.assert_not_called()

    def test_proved_connection_between_existing_nodes_is_not_lost_for_having_no_new_node(self):
        s,doc,_,module,_=self.fixture();proposal=copy.deepcopy(doc)
        proposal['edges'].append({'from':2,'to':3})
        module.merge.return_value=(proposal,dict(added_node_ids=[],added_directed_edges=1,native_revalidation_required=True))
        with patch.dict('sys.modules',pathrepair=module):
            evidence=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
        current=s.drafts.get('obj_test')
        self.assertEqual(current['nodes'],doc['nodes'])
        self.assertIn({'from':2,'to':3},current['edges'])
        self.assertEqual(evidence['result_draft_revision'],doc['revision']+1)

    def test_actual_merge_and_preflight_consume_the_servers_literal_partial_report_binding(self):
        from test_path_repair import PathRepairTests
        s,_,_,_,_=self.fixture()
        doc,baseline,report,binding=PathRepairTests().fixture(guide=True)
        s.drafts.documents['obj_test']=copy.deepcopy(doc)
        s.native_path('obj_test').write_bytes(binding['native_report_bytes'])
        candidate=s.candidate('obj_test');candidate.write_bytes(binding['candidate_bytes'])
        candidate.with_suffix('.json').write_bytes(binding['candidate_metadata_bytes'])
        (s.work/'obj_test.native-attempt.json').write_text(json.dumps(binding['attempt']))
        s.proof_status.return_value=dict(valid=True,inputs=binding['inputs'])
        s.report_status.return_value=binding['report_status']
        def command(text):
            s.runtime_native_path('obj_test').write_bytes(binding['repair_report_bytes'])
            return 'native append-only path report'
        s.rcon_with_followup.side_effect=command
        evidence=s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
        current=s.drafts.get('obj_test')
        self.assertEqual(current['nodes'][:3],doc['nodes'])
        self.assertTrue({(e['from'],e['to'])for e in doc['edges']}<={(e['from'],e['to'])for e in current['edges']})
        self.assertEqual(evidence['added_node_ids'],[1000])
        self.assertEqual(evidence['added_directed_edges'],4)
        self.assertFalse(report['generation_complete']);self.assertTrue(evidence['native_revalidation_required'])
        s.install.assert_not_called()

    def test_changed_sources_or_profile_during_native_search_never_change_draft(self):
        for case in ('candidate','metadata','native','attempt','inputs','staged','map','profile','report_bytes','freshness'):
            s,doc,_,module,report=self.fixture();command=s.rcon_with_followup.side_effect
            def raced(text):
                response=command(text)
                if case in ('candidate','metadata','native','attempt','staged'):
                    path={'candidate':s.candidate('obj_test'),'metadata':s.candidate('obj_test').with_suffix('.json'),
                          'native':s.native_path('obj_test'),'attempt':s.work/'obj_test.native-attempt.json',
                          'staged':s.engine_nav_root()/s.candidate('obj_test').name}[case]
                    path.write_bytes(path.read_bytes()+b' ')
                elif case=='inputs':
                    value=copy.deepcopy(s.proof_status.return_value);value['inputs']['game_module']='changed'
                    s.proof_status.return_value=value
                elif case=='map':report['map']='obj_other';s.runtime_native_path('obj_test').write_text(json.dumps(report))
                elif case=='profile':s.require_bbox.return_value=dict(MOVEMENT_PROFILES[0])
                elif case=='report_bytes':s.wait_fresh.side_effect=lambda *args:dict(report,source='changed')
                elif case=='freshness':s.report_status.return_value=dict(current=False)
                return response
            s.rcon_with_followup.side_effect=raced
            with self.subTest(case=case),patch.dict('sys.modules',pathrepair=module):
                with self.assertRaisesRegex(RuntimeError,'Grundlage während der Pfadreparatur geändert'):
                    s.repair_path_draft(dict(map='obj_test',action='repair_check',log=''))
                self.assertEqual(s.drafts.get('obj_test'),doc);module.merge.assert_not_called()

    def test_full_check_precedes_alternate_withdrawal_and_final_complete_check(self):
        s,_,_,_,_=self.fixture()
        s.recover_rejected_shortcuts=Mock(return_value=None)
        # The initial source-bound recovery runs before topology discovery;
        # only the second failed complete import supplies the later alternate.
        s.recover_rejected_ordinary=Mock(side_effect=[None,None,dict(model='native_rejected_edge_alternate_v2')])
        s.action_repair=Mock(return_value=dict(repairs={}))
        s.repair_path_draft=Mock(return_value=dict(added_node_ids=[4],native_revalidation_required=True))
        s.action_native_check=Mock(side_effect=[RuntimeError('missing path'),RuntimeError('old edge rejected'),
                                              dict(audit=dict(unresolved_nodes=[],fresh=True),proof='current native fixture proof')])
        with patch('repair.native_crouch_feedback',return_value=None):
            result=s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        self.assertEqual(s.action_native_check.call_count,3);s.repair_path_draft.assert_called_once()
        self.assertTrue(result['audit']['fresh']);self.assertFalse(result['installed']);s.install.assert_not_called()
        self.assertEqual(len(result['repair']['native_path_repairs']),1)
        self.assertEqual(len(result['repair']['withdrawn_rejected_ordinary']),1)

    def test_persistent_failure_runs_only_one_path_search_and_does_not_publish_success(self):
        s,_,_,_,_=self.fixture()
        s.recover_rejected_shortcuts=Mock(return_value=None);s.recover_rejected_ordinary=Mock(return_value=None)
        s.action_repair=Mock(return_value=dict(repairs={}))
        s.repair_path_draft=Mock(return_value=dict(added_node_ids=[4]))
        s.action_native_check=Mock(side_effect=RuntimeError('still unsafe'))
        with patch('repair.native_crouch_feedback',return_value=None),self.assertRaisesRegex(RuntimeError,'still unsafe'):
            s.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        s.repair_path_draft.assert_called_once();self.assertEqual(s.action_native_check.call_count,2)
        s.install.assert_not_called()


if __name__=='__main__':unittest.main()
