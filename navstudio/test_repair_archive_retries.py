"""Every new immutable repair archive binds its complete literal source bundle."""
import copy
import hashlib
import json
from pathlib import Path
import unittest

import test_anchor_repair_workflow as anchor
import test_descent_repair_workflow as descent
import test_pending_jump_cleanup as pending
from server import repair_archive_bundle_digest


class RepairArchiveRetriesTests(unittest.TestCase):
    def fixture(self,kind):
        if kind=='anchor':
            helper=anchor.AnchorRepairWorkflowTests();self.addCleanup(helper.doCleanups)
            s,doc,source,_=helper.fixture()
            def command(text,read_only):
                values=list(map(float,text.split()[2:]))
                def reject(result):
                    for end in result['imports'].values():
                        for row in end.values():row.update(accepted=False,exact=False)
                return source.reply(values[:3],values[3:],reject)
            s.rcon.side_effect=command;method=s.repair_anchor_draft
        elif kind=='descent':
            helper=descent.DescentRepairWorkflowTests();self.addCleanup(helper.doCleanups)
            s,doc,_,_,_=helper.fixture()
            def command(text,read_only):
                values=list(map(float,text.split()[2:]))
                return helper.reply(values[:3],values[3:],rejected=True)
            s.rcon.side_effect=command;method=s.repair_descent_draft
        else:
            helper=pending.PendingJumpCleanupTests();self.addCleanup(helper.doCleanups)
            s,doc,_=helper.studio_fixture();method=s.repair_pending_jump_draft
        return s,doc,method

    def test_same_sources_reuse_but_new_created_attempt_has_own_literal_archive_in_all_callers(self):
        for kind in('anchor','descent','pending'):
            s,doc,method=self.fixture(kind);attempt=s.work/'obj_test.native-attempt.json'
            original=json.loads(attempt.read_bytes());original['created']=1792098920.8408003
            attempt.write_text(json.dumps(original))
            def invoke():
                # A byte-identical retry starts from that same source draft.
                # This also models a retry restored after an interrupted save.
                s.drafts.documents['obj_test']=copy.deepcopy(doc)
                return method(dict(map='obj_test',action='repair_check',log=''))
            with self.subTest(kind=kind):
                first=invoke();paths=[Path(item['path'])for item in first['source_evidence'].values()]
                saved={p:p.read_bytes()for p in paths}
                second=invoke();self.assertEqual(first,second)
                self.assertEqual({p:p.read_bytes()for p in paths},saved)
                original['created']=1792103066.083371;attempt.write_text(json.dumps(original))
                third=invoke();self.assertNotEqual(first['archive_bundle_sha256'],third['archive_bundle_sha256'])
                for p,data in saved.items():self.assertEqual(p.read_bytes(),data)
                for evidence in(first,third):
                    parts={k:v['sha256']for k,v in evidence['source_evidence'].items()}
                    expected=hashlib.sha256(json.dumps(parts,sort_keys=True,separators=(',',':')).encode('utf8')).hexdigest()
                    self.assertEqual(evidence['archive_bundle_sha256'],expected)
                    for item in evidence['source_evidence'].values():
                        self.assertEqual(hashlib.sha256(Path(item['path']).read_bytes()).hexdigest(),item['sha256'])
                s.install.assert_not_called()

    def test_existing_conflicting_archive_never_overwrites_or_changes_draft(self):
        for kind in('anchor','descent','pending'):
            s,doc,method=self.fixture(kind)
            with self.subTest(kind=kind):
                first=method(dict(map='obj_test',action='repair_check',log=''))
                archived=Path(first['source_evidence']['native_attempt_bytes']['path'])
                archived.write_bytes(b'existing conflicting evidence')
                s.drafts.documents['obj_test']=copy.deepcopy(doc)
                with self.assertRaisesRegex(RuntimeError,'Archivierter'):
                    method(dict(map='obj_test',action='repair_check',log=''))
                self.assertEqual(archived.read_bytes(),b'existing conflicting evidence')
                self.assertEqual(s.drafts.get('obj_test'),doc);s.install.assert_not_called()

    def test_every_named_raw_member_and_names_are_part_of_digest(self):
        binding={key:key.encode()for key in('native_report_bytes','candidate_bytes','candidate_metadata_bytes',
                                           'native_attempt_bytes','repair_report_bytes','original_native_bytes','probe_reply_bytes')}
        parts=[(key,'.unused')for key in binding]
        original=repair_archive_bundle_digest(binding,parts)
        self.assertEqual(original,repair_archive_bundle_digest(binding,list(reversed(parts))))
        for key in binding:
            changed=dict(binding);changed[key]+=b' '
            self.assertNotEqual(original,repair_archive_bundle_digest(changed,parts))
        self.assertNotEqual(original,repair_archive_bundle_digest(binding,parts[:-1]))


if __name__=='__main__':unittest.main()
