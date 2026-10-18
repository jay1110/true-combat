"""Pure native CROUCH feedback may request a retry, never authorize export."""
import copy
import json
from pathlib import Path
import tempfile
from unittest import TestCase, main
from unittest.mock import Mock

from repair import native_crouch_feedback


class NativeCrouchFeedbackTests(TestCase):
    def fixture(self, complete=False):
        doc = dict(map='obj_test', mode='manual_draft', revision=6, nodes=[], edges=[])
        for identity in (1, 2, 3):
            doc['nodes'].append(dict(id=identity, position=[identity * 64., 0., -4.],
                nav_flags=0, crouch=False, radius=35, generated_guide=False,
                editor_role=0, action_roles=[], action_properties={}, name='', facing=[0, 0, 0],
                way_guid=371 if identity == 1 else 0))
        doc['edges'] = [dict(zip(('from', 'to'), p)) for p in ((1, 2), (2, 1), (2, 3), (3, 2))]
        native = dict(map='obj_test', candidate_mode='manual_draft', generation_complete=complete,
            waypoint_metadata_contract='TCE_NODE_METADATA_1', traces=516782, trace_budget=2315296,
            bbox=dict(origin_above_feet=30), traversal_routes=[], nodes=[], edges=[],
            # The restricted arrival guard can exhaust this separate phase.
            # It is not a WAY certificate and does not negate checked input.
            arrival_regions=dict(budget_exhausted=True),
            native_input_coverage=dict(input_nodes=3, native_floor_accepted=3, native_floor_rejected=0,
                initial_directed_links=4, initial_accepted_links=4, initial_rejected_links=0,
                connectivity_missing_input_nodes=0, ordinary_without_exit=[], geometry_rejections=[],
                link_rejections=[], capacity=dict(input_validation_complete=True, input_truncated=False)))
        for node in doc['nodes']:
            native['nodes'].append(dict(id=node['id'], accepted=True,
                raw_position=[*node['position'][:2], 26.], position=[*node['position'][:2], 30.25],
                native_input=dict(floor_accepted=True), nav_flags=0, crouch=node['id'] in (1, 2),
                editor_role=0, custom_properties={}, name='', facing=[0, 0, 0],
                import_uid=node['way_guid'], way_guid=(node['way_guid'] or node['id']) if complete else 0))
        native['edges'] = [dict(e, accepted=True, crouch=(e['from'], e['to']) == (2, 1),
            reason='external_native_validated_walk') for e in doc['edges']]
        return doc, native

    def test_complete_checked_input_with_crouch_mismatch_requests_only_a_retry(self):
        for complete in (False, True):
            with self.subTest(complete=complete):
                doc, native = self.fixture(complete)
                before = copy.deepcopy((doc, native))
                evidence = native_crouch_feedback(doc, native)
                self.assertEqual(evidence['required_crouch_flags'], [1, 2])
                self.assertEqual(evidence['accepted_crouch_edges'], [[2, 1]])
                self.assertEqual(evidence['generation_complete'], complete)
                self.assertTrue(evidence['native_revalidation_required'])
                self.assertFalse(evidence['adopts_way_uids']); self.assertFalse(evidence['changes_positions'])
                self.assertEqual((doc, native), before)

    def test_repeat_after_only_flag_promotion_is_noop(self):
        doc, native = self.fixture()
        for node in doc['nodes'][:2]: node.update(nav_flags=64, crouch=True)
        for row in native['nodes'][:2]: row['nav_flags'] = 64
        self.assertIsNone(native_crouch_feedback(doc, native))

    def test_incomplete_input_rejects_floor_links_connectivity_and_hard_budget_failures(self):
        cases = [lambda n: n['nodes'][0].update(accepted=False),
            lambda n: n['nodes'][0]['native_input'].update(floor_accepted=False),
            lambda n: n['nodes'].pop(), lambda n: n['edges'].pop(),
            lambda n: n['edges'][0].update(accepted=False),
            lambda n: n['native_input_coverage']['capacity'].update(input_validation_complete=False),
            lambda n: n['native_input_coverage']['capacity'].update(input_truncated=True),
            lambda n: n['native_input_coverage'].update(ordinary_without_exit=[3]),
            lambda n: n['native_input_coverage'].update(connectivity_missing_input_nodes=1),
            lambda n: n['native_input_coverage'].update(initial_accepted_links=3),
            lambda n: n['native_input_coverage'].update(geometry_rejections=[{'id': 3}]),
            lambda n: n['native_input_coverage'].update(link_rejections=[{'from': 1, 'to': 2}]),
            lambda n: n.update(traces=n['trace_budget'])]
        for mutate in cases:
            doc, native = self.fixture(); mutate(native)
            self.assertIsNone(native_crouch_feedback(doc, native))

    def test_stale_map_positions_uid_and_metadata_are_not_duck_feedback(self):
        cases = [lambda n: n.update(map='obj_other'),
            lambda n: n.update(candidate_mode='generated'),
            lambda n: n['nodes'][0].update(raw_position=[]),
            lambda n: n['nodes'][0].update(raw_position=[64.1, 0, 26]),
            lambda n: n['nodes'][0].update(position=[64, 0, 60]),
            lambda n: n['nodes'][0].update(import_uid=372),
            lambda n: n['nodes'][0].update(way_guid=372),
            lambda n: n['nodes'][0].update(facing=[1, 0, 0]),
            lambda n: n['nodes'][0].update(custom_properties={'note': 'different'}),
            lambda n: n['nodes'].append(copy.deepcopy(n['nodes'][0]))]
        for mutate in cases:
            doc, native = self.fixture(); mutate(native)
            self.assertIsNone(native_crouch_feedback(doc, native))
        doc, native = self.fixture(True); native['nodes'][0]['way_guid'] = 0
        self.assertIsNone(native_crouch_feedback(doc, native))

    def test_actions_team_properties_or_native_traversal_never_gain_crouch(self):
        for fields in [dict(nav_flags=1 << 45), dict(nav_flags=1 << 27), dict(editor_role=1),
                       dict(generated_guide=True), dict(action_roles=[dict(kind='walk_drop', role='landing')]),
                       dict(action_properties={'paththrough': 'switch:door'})]:
            doc, native = self.fixture(); doc['nodes'][0].update(fields)
            native['nodes'][0]['nav_flags'] = doc['nodes'][0]['nav_flags']
            self.assertIsNone(native_crouch_feedback(doc, native))
        for mutate in [lambda n: n['nodes'][0].update(editor_role=128),
                       lambda n: n['nodes'][0].update(door_recipe={'validated': True}),
                       lambda n: n.update(traversal_routes=[dict(kind='jump', landing=1)])]:
            doc, native = self.fixture(); mutate(native)
            self.assertIsNone(native_crouch_feedback(doc, native))

    def test_actual_accepted_crouched_walk_evidence_is_required(self):
        for mutate in [lambda n: n['edges'][1].update(crouch=False),
                       lambda n: n['edges'][1].update(reason='manual_native_transition_live_pending')]:
            doc, native = self.fixture(); mutate(native)
            self.assertIsNone(native_crouch_feedback(doc, native))

    def test_unrelated_authored_opaque_metadata_is_checked_and_preserved(self):
        doc, native = self.fixture()
        doc['nodes'][2].update(name='Authored', facing=[1, 0, 0], action_properties={'note': 'keep me'})
        native['nodes'][2].update(name='Authored', facing=[1, 0, 0], custom_properties={'note': 'keep me'})
        before = copy.deepcopy(doc)
        self.assertEqual(native_crouch_feedback(doc, native)['required_crouch_flags'], [1, 2])
        self.assertEqual(doc, before)
        native['nodes'][2]['name'] = 'Changed'
        self.assertIsNone(native_crouch_feedback(doc, native))


class NativeCrouchWorkflowTests(TestCase):
    def fixture(self):
        from edit import Drafts
        from server import Studio
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        s = Studio.__new__(Studio)
        s.drafts = Drafts(root / 'drafts')
        source, report = NativeCrouchFeedbackTests().fixture()
        s.drafts.start('obj_test', dict(nodes=source['nodes'], edges=source['edges']), dict(environment={}))
        s.draft_environment = lambda name: {}
        s.action_load_map = Mock(); s.quiet_generation = Mock()
        s.update = lambda job, **kw: job.update(kw)
        s.path = lambda name, suffix: root / (name + suffix)
        s.native_path = lambda name: root / 'native.json'
        s.native_path('obj_test').write_text(json.dumps(report))
        s.file_hash = lambda path: 'fresh-bound-report-fixture'
        s.native = Mock(return_value=report)
        s.report_status = Mock(return_value=dict(current=True))
        s.prepare_draft = Mock()
        s.repair_path_draft = Mock(return_value=None)
        s.recover_rejected_ordinary = Mock(return_value=None)
        s.install = Mock(side_effect=AssertionError('A posture retry must never install'))
        return s, report

    def test_proposal_crouch_failure_promotes_only_flags_then_requires_full_native_success(self):
        s, report = self.fixture()
        # Initially there is no duck mismatch; a new route is proposed once.
        initial = copy.deepcopy(report)
        for row in initial['nodes']: row['crouch'] = False
        for edge in initial['edges']: edge['crouch'] = False
        s.native.return_value = initial
        proposed = []
        def repair(job):
            doc = s.drafts.get('obj_test'); proposal = copy.deepcopy(doc)
            proposal['edges'] += [dict(**{'from': 1, 'to': 3}), dict(**{'from': 3, 'to': 1})]
            s.drafts.apply_repair('obj_test', doc['revision'], {}, proposal)
            proposed.append(copy.deepcopy(s.drafts.get('obj_test')))
            return dict(repairs=dict(added_directed_edges=2))
        s.action_repair = Mock(side_effect=repair)
        checks = []
        def check(job):
            checks.append(copy.deepcopy(s.drafts.get('obj_test')))
            if len(checks) == 1:
                current = copy.deepcopy(report)
                current['edges'] += [dict(**{'from': 1, 'to': 3}, accepted=True, crouch=False,
                                         reason='external_native_validated_walk'),
                                     dict(**{'from': 3, 'to': 1}, accepted=True, crouch=False,
                                         reason='external_native_validated_walk')]
                current['native_input_coverage'].update(initial_directed_links=6, initial_accepted_links=6)
                s.native.return_value = current
                raise RuntimeError('point 1 needs CROUCH; no edited WAY published')
            return dict(audit=dict(unresolved_nodes=[], fresh_full_native_check=True), proof='fresh proof')
        s.action_native_check = Mock(side_effect=check)
        result = s.action_repair_check(dict(map='obj_test', action='repair_check', log=''))
        s.action_repair.assert_called_once(); self.assertEqual(s.action_native_check.call_count, 2)
        self.assertEqual(checks[1]['edges'], proposed[0]['edges'])
        self.assertEqual(checks[1]['next_id'], proposed[0]['next_id'])
        for before, after in zip(proposed[0]['nodes'], checks[1]['nodes']):
            expected = copy.deepcopy(before)
            if before['id'] in (1, 2): expected.update(nav_flags=64, crouch=True)
            self.assertEqual(after, expected)
        self.assertTrue(result['audit']['fresh_full_native_check']); self.assertFalse(result['installed'])
        self.assertEqual(result['repair']['automatic_native_crouch'][0]['required_crouch_flags'], [1, 2])
        s.install.assert_not_called()

    def test_rejected_or_stale_feedback_cannot_promote_or_return_a_previous_proof(self):
        for failure in ('blocked', 'stale', 'still_failed_after_promotion'):
            with self.subTest(failure=failure):
                s, report = self.fixture(); before = copy.deepcopy(s.drafts.get('obj_test'))
                if failure == 'blocked': report['edges'][0].update(accepted=False, reason='blocked_clearance')
                if failure == 'stale': s.report_status.return_value = dict(current=False)
                s.action_repair = Mock(return_value=dict(repairs={}))
                s.action_native_check = Mock(side_effect=RuntimeError('no current WAY'))
                with self.assertRaisesRegex(RuntimeError, 'no current WAY'):
                    s.action_repair_check(dict(map='obj_test', action='repair_check', log=''))
                if failure != 'still_failed_after_promotion':
                    self.assertEqual(s.drafts.get('obj_test'), before)
                    self.assertEqual(s.action_native_check.call_count, 1)
                else:
                    # Resuming a current failure avoids another topology pass,
                    # but a subsequent native failure still grants no proof.
                    s.action_repair.assert_not_called()
                    self.assertEqual(s.drafts.get('obj_test')['edges'], before['edges'])
                s.install.assert_not_called()


if __name__ == '__main__': main()
