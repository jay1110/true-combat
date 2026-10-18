from unittest import TestCase, main
from objective_evidence import parse_reply, summarize


def bot(plants=0, defuses=0, plant_time=0, defuse_time=0, plant_bomb=0,
        defuse_bomb=0, plant_objective=0, planted=0, team=1):
    return (f'TCEOBJECTIVE_BOT 4 team {team} carrier 1 planted {planted} hint 10 '
            f'plants {plants} defuses {defuses} plant_time {plant_time} defuse_time {defuse_time} '
            f'plant_bomb {plant_bomb} defuse_bomb {defuse_bomb} plant_objective {plant_objective}')


class ObjectiveEvidenceTests(TestCase):
    def test_native_own_plant_and_exact_defuse_receipts(self):
        result = summarize([dict(time=0, reply=bot()),
            dict(time=20, reply=bot(plants=1, plant_time=501, plant_bomb=70, plant_objective=120, planted=1)),
            dict(time=40, reply=bot(plants=1, plant_time=501, plant_bomb=70, plant_objective=120,
                                  defuses=1, defuse_time=601, defuse_bomb=82, planted=1))])
        self.assertEqual([r['action'] for r in result['completions']], ['PLANTED', 'DEFUSED'])
        self.assertEqual(result['completions'][0]['objective'], 120)
        self.assertEqual(result['completions'][1]['bomb'], 82)
        self.assertEqual(result['completions'][0]['actor'], 4)
        self.assertTrue(result['observer_available'])
        self.assertFalse(result['mission_behavior_complete'])

    def test_first_snapshot_and_bomb_disappearance_are_not_completion(self):
        first = bot(plants=1, plant_time=501, plant_bomb=70, plant_objective=120, planted=1)
        result = summarize([dict(time=0, reply=first), dict(time=20, reply=first.replace('planted 1', 'planted 0'))])
        self.assertEqual(result['completions'], [])
        self.assertEqual(summarize([dict(time=0, reply=first)])['completions'], [])

    def test_counter_reset_team_change_and_missing_receipts_do_not_confirm(self):
        first = bot(plants=2, plant_time=501, plant_bomb=70, plant_objective=120)
        reset = summarize([dict(time=0, reply=first), dict(time=20, reply=bot())])
        self.assertEqual(reset['completions'], [])
        self.assertEqual(reset['counter_resets'][0]['counter'], 'plants')
        changed = summarize([dict(time=0, reply=bot()), dict(time=20, reply=first.replace('team 1', 'team 2'))])
        self.assertEqual(changed['completions'], [])
        stale = summarize([dict(time=0, reply=bot()), dict(time=20, reply=bot(plants=1))])
        self.assertEqual(stale['completions'], [])
        self.assertIn('without a fresh native', stale['warnings'][0])

    def test_nullable_legacy_stats_and_duplicate_fields_are_explicit(self):
        result = summarize([dict(time=0, reply=bot().replace('plants 0', 'plants null'))])
        self.assertFalse(result['observer_available'])
        parsed = parse_reply(bot() + ' plants 5')
        self.assertEqual(parsed['bots'], {})
        self.assertIn('duplicate native field', parsed['warnings'][0])
        conflict = parse_reply(bot() + '\n' + bot(plants=1))
        self.assertEqual(conflict['bots'], {})
        self.assertIn('conflicting duplicate', conflict['warnings'][0])

    def test_flag_capture_requires_same_goal_entity_and_positive_counter_delta(self):
        first = 'TCEOBJECTIVE_STATUS FLAG_A entity 120 owner 1 state 0 bomb -1 captures 0'
        second = first.replace('captures 0', 'captures 1')
        result = summarize([dict(time=0, reply=first), dict(time=20, reply=second)])
        self.assertEqual(result['completions'][0]['action'], 'CAPTURED')
        self.assertIsNone(result['completions'][0]['actor'])
        replaced = summarize([dict(time=0, reply=first), dict(time=20, reply=second.replace('entity 120', 'entity 121'))])
        self.assertEqual(replaced['completions'], [])

    def test_multiple_actions_do_not_invent_identity_for_earlier_events(self):
        result = summarize([dict(time=0, reply=bot()),
                            dict(time=20, reply=bot(defuses=3, defuse_time=1234, defuse_bomb=99))])
        event = result['completions'][0]
        self.assertEqual(event['count_delta'], 3)
        self.assertTrue(event['last_action_only'])
        self.assertEqual(event['bomb'], 99)

    def test_out_of_order_samples_do_not_prove_events(self):
        result = summarize([dict(time=20, reply=bot()),
                            dict(time=10, reply=bot(defuses=1, defuse_time=1234, defuse_bomb=99))])
        self.assertEqual(result['completions'], [])
        self.assertTrue(result['warnings'])


if __name__ == '__main__':
    main()
