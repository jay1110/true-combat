"""A rejected automatic addition is recoverable without undoing later work."""
import copy
import hashlib
import json
import unittest
from unittest.mock import Mock

from repair import rejected_historical_shortcut_feedback
import test_rejected_shortcut_feedback as shortcut_tests
from test_rejected_shortcut_feedback import refresh_coverage


class HistoricalShortcutTests(unittest.TestCase):
    def fixture(self):
        owner = shortcut_tests.RejectedShortcutTests()
        self.addCleanup(owner.doCleanups)
        studio, _ = owner.fixture()

        def report(doc, rejected=()):
            result = dict(map='obj_test', candidate_mode='manual_draft', generation_complete=False,
                waypoint_metadata_contract='TCE_NODE_METADATA_1', bbox=dict(origin_above_feet=30),
                traces=100, trace_budget=1000,
                nodes=[dict(id=n['id'], accepted=True, spawn_reachable=True,
                    raw_position=n['position'][:2]+[n['position'][2]+30], radius=n['radius'],
                    nav_flags=n['nav_flags'], crouch=n['crouch'], native_input=dict(floor_accepted=True))
                    for n in doc['nodes']],
                edges=[dict(e, accepted=(e['from'],e['to']) not in rejected,
                    reason='steep_ground' if (e['from'],e['to']) in rejected else 'accepted',
                    crouch=any(n['crouch'] for n in doc['nodes'] if n['id'] in (e['from'],e['to'])))
                    for e in doc['edges']])
            refresh_coverage(result)
            return result

        original = studio.drafts.get('obj_test')
        source = report(original)
        raw = json.dumps(source).encode()
        digest = hashlib.sha256(raw).hexdigest()
        studio.path('obj_test', '.repair-source-'+digest[:12]+'.native.json').write_bytes(raw)
        added = copy.deepcopy(original)
        added['edges'] += [dict(zip(('from','to'), pair)) for pair in ((1,3),(3,1))]
        added['nodes'][1].update(crouch=True, nav_flags=64)
        added = studio.drafts.apply_repair('obj_test', original['revision'], {}, added)
        posture = copy.deepcopy(added)
        posture['nodes'][0].update(crouch=True, nav_flags=64)
        posture = studio.drafts.apply_repair('obj_test', added['revision'], {}, posture)
        # This receipt intentionally replaces the addition receipt, as in the
        # interrupted real workflow. The content-hashed predecessor survives.
        studio.path('obj_test', '.repair-proposal.json').write_text(json.dumps(dict(
            draft_revision=posture['revision'], required_crouch_flags=[1], added_directed_edges=0)))
        withdrawn = copy.deepcopy(posture)
        withdrawn['edges'] = [e for e in withdrawn['edges'] if (e['from'],e['to']) != (2,3)]
        doc = studio.drafts.apply_repair('obj_test', posture['revision'], {}, withdrawn)
        native = report(doc, {(1,3),(3,1)})
        studio.current_native = native
        studio.native_path('obj_test').write_text(json.dumps(native))
        return studio, doc, native, {digest:raw}

    def test_later_posture_and_edge_withdrawal_are_preserved(self):
        _, doc, native, sources = self.fixture()
        before = copy.deepcopy((doc,native,sources))
        proposal, evidence = rejected_historical_shortcut_feedback(doc,native,sources)
        self.assertEqual((doc,native,sources),before)
        self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual(proposal['edges'],[e for e in doc['edges'] if (e['from'],e['to']) not in ((1,3),(3,1))])
        self.assertNotIn(dict(zip(('from','to'),(2,3))),proposal['edges'])
        self.assertEqual(evidence['history_origins'][0]['undo_index'],0)
        self.assertFalse(evidence['changes_node_metadata'])
        self.assertTrue(evidence['native_revalidation_required'])

    def test_accepted_automatic_direction_and_old_bad_authored_edge_remain(self):
        _, doc, native, sources = self.fixture()
        next(e for e in native['edges'] if (e['from'],e['to'])==(3,1)).update(accepted=True,reason='accepted')
        next(e for e in native['edges'] if (e['from'],e['to'])==(1,2)).update(accepted=False,reason='capsule_support_step_or_drop')
        refresh_coverage(native)
        proposal,evidence=rejected_historical_shortcut_feedback(doc,native,sources)
        self.assertEqual([e['edge'] for e in evidence['rejected_edges']],[[1,3]])
        self.assertIn({'from':3,'to':1},proposal['edges'])
        self.assertIn({'from':1,'to':2},proposal['edges'])

    def test_fresh_unapplied_native_crouch_requirement_does_not_rewrite_node(self):
        _,doc,native,sources=self.fixture()
        native['nodes'][2]['crouch']=True
        next(e for e in native['edges'] if (e['from'],e['to'])==(3,2))['crouch']=True
        proposal,_=rejected_historical_shortcut_feedback(doc,native,sources)
        self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual(proposal['nodes'][2]['nav_flags'],0)

    def test_effective_native_radius_clamp_preserves_authored_requested_radius(self):
        _,doc,native,sources=self.fixture()
        native['nodes'][0].update(radius=10,arrival=dict(requested_radius=35))
        self.assertIsNotNone(rejected_historical_shortcut_feedback(doc,native,sources))
        native['nodes'][0]['arrival']['requested_radius']=36
        self.assertIsNone(rejected_historical_shortcut_feedback(doc,native,sources))

    def test_manual_or_unaccounted_edits_and_missing_proofs_fail_closed(self):
        for case in ('manual','position','radius','uid','flags','custom','action','bad_patch','redo',
                     'source_hash','source_graph','source_metadata','missing','truncated','floor','budget','history_limit'):
            _,doc,native,sources=self.fixture()
            if case=='manual':doc['undo'][-1]['op']='disconnect'
            elif case in ('position','radius','uid','flags','custom','action'):
                key,value={'position':('position',[33,0,0]),'radius':('radius',36),
                    'uid':('way_guid',123),'flags':('nav_flags',65),'custom':('properties',{'owner':'user'}),
                    'action':('action_roles',[{'kind':'walk_drop','role':'launch'}])}[case]
                doc['nodes'][0][key]=value
            elif case=='bad_patch':doc['undo'][-1]['removed'].append([2,1])
            elif case=='redo':doc['redo']=[{}]
            elif case=='source_hash':sources={next(iter(sources)):next(iter(sources.values()))+b' '}
            elif case in ('source_graph','source_metadata'):
                source=json.loads(next(iter(sources.values())))
                if case=='source_graph':source['edges'].pop();refresh_coverage(source)
                else:source['nodes'][0]['name']='foreign'
                raw=json.dumps(source).encode();sources={hashlib.sha256(raw).hexdigest():raw}
            elif case=='missing':sources={}
            elif case=='truncated':native['native_input_coverage']['capacity']['input_truncated']=True
            elif case=='floor':native['nodes'][0]['native_input']['floor_accepted']=False
            elif case=='budget':native['traces']=native['trace_budget']
            with self.subTest(case=case):
                self.assertIsNone(rejected_historical_shortcut_feedback(doc,native,sources,max_history=2 if case=='history_limit' else 16))

    def test_restart_recovers_then_requires_a_complete_native_check(self):
        studio,doc,_,_=self.fixture()
        studio.action_repair=Mock()
        studio.action_native_check=Mock(return_value=dict(audit=dict(unresolved_nodes=[]),proof='fresh'))
        result=studio.action_repair_check(dict(map='obj_test',action='repair_check',log=''))
        studio.action_repair.assert_not_called()
        studio.action_native_check.assert_called_once()
        studio.install.assert_not_called()
        self.assertFalse(result['installed'])
        self.assertEqual(studio.drafts.get('obj_test')['nodes'],doc['nodes'])
        self.assertEqual(len(studio.drafts.get('obj_test')['edges']),3)
        self.assertTrue(studio.path('obj_test','.historical-shortcut-withdrawal.json').is_file())
        self.assertEqual(len(list(studio.work.glob('*.repair-proposal-r*.json'))),1)

    def test_receipt_archives_are_literal_revision_bound_and_immutable(self):
        studio,_,_,_=self.fixture()
        source=studio.path('obj_test','.repair-proposal.json')
        raw=source.read_bytes();first=studio.archive_repair_proposal('obj_test')
        source.write_text('{"draft_revision": 99, "added_directed_edges": 0}\n')
        second=studio.archive_repair_proposal('obj_test')
        from pathlib import Path
        self.assertEqual(Path(first['path']).read_bytes(),raw)
        self.assertEqual(Path(second['path']).read_bytes(),source.read_bytes())
        self.assertNotEqual(first['path'],second['path'])
        Path(second['path']).write_bytes(b'changed archive')
        with self.assertRaises(RuntimeError):studio.archive_repair_proposal('obj_test')


if __name__=='__main__':unittest.main()
