"""Authoring-filter regressions: exact registrations, comments and gameplay."""
import unittest
from runtime_payload import command_spans, filter_commands, classify_commands, filter_diagnostics


class ScriptBoundary(unittest.TestCase):
    def test_nested_tokens_and_alias(self):
        source = '''// Commands["fake"] = { invalid;
Commands["runtime"] = { Func = function() { print("}; Commands[\\"x\\"] = {"); /* } */ } };
Commands["edit"] = { Func = function() { local x = { a = [1, 2] }; } };
Commands["alias"] = Commands["edit"];
'''
        result, audit = filter_commands(source, ['edit', 'alias'])
        self.assertEqual(set(command_spans(result)), {'runtime'})
        a, b = command_spans(source)['runtime']
        self.assertIn(source[a:b], result)
        self.assertEqual(audit['removed'], ['alias', 'edit'])

    def test_unknown_registration_rejected(self):
        with self.assertRaises(ValueError):
            filter_commands('Commands["runtime"] = {};', ['missing'])
        with self.assertRaises(ValueError):
            command_spans('Commands.edit = {};')
        for source in ('Commands[name] = {};', 'Commands[name + "x"] = {};'):
            with self.assertRaises(ValueError):
                command_spans(source)

    def test_malformed_registration_rejected(self):
        for source in ('Commands["edit"] = {;', 'Commands["edit"] = {}',
                       'Commands["edit"] = {}; Commands["edit"] = {};'):
            with self.assertRaises(ValueError):
                command_spans(source)

    def test_exhaustive_policy_and_alias(self):
        policy = {'schema': 2, 'public_commands': {'commands.gm': ['runtime']},
                  'editor_commands': {'commands.gm': ['edit']}, 'editor_files': []}
        payload = {'commands.gm': b'Commands["runtime"] = {}; Commands["edit"] = {};'}
        self.assertEqual(classify_commands(payload, policy)['commands.gm']['public'], ['runtime'])
        for extra in ('Commands["new"] = {};', ''):
            changed = dict(payload)
            changed['other.gm'] = ('Commands["unknown"] = {};' + extra).encode()
            with self.assertRaises(ValueError):
                classify_commands(changed, policy)
        payload['commands.gm'] = b'Commands["runtime"] = Commands["edit"]; Commands["edit"] = {};'
        with self.assertRaises(ValueError):
            classify_commands(payload, policy)

    def test_diagnostics_preserve_mission_and_recovery(self):
        policy = {'diagnostic_cvars': ['omnibot_tce_explore_debug'],
                  'editor_status_cvar': 'omnibot_navstudio_goal_token',
                  'editor_print_prefix': 'NAVSTUDIO_GOALS ',
                  'diagnostic_prints': {'goal.gm': ['TCE2STUCK TERMINAL']}}
        source = '''if(GetCvar("omnibot_tce_explore_debug")) { print("debug"); }
if(shouldKill) { print("TCE2STUCK TERMINAL", bot.GetGameId()); bot.Kill(); }
print("Mission announcement"); SetAvailableMapGoals(TEAM.ALLIES, true);
'''
        filtered, audit = filter_diagnostics(source, 'goal.gm', policy, [])
        self.assertEqual(len(audit), 2)
        self.assertIn('if(shouldKill)', filtered)
        self.assertIn('bot.Kill();', filtered)
        self.assertIn('print("Mission announcement"); SetAvailableMapGoals(TEAM.ALLIES, true);', filtered)
        self.assertNotIn('GetCvar(', filtered)
        with self.assertRaises(ValueError):
            filter_diagnostics('if(GetCvar("omnibot_tce_new_debug")) {}', 'other.gm', policy, [])

    def test_studio_status_requires_reviewed_standalone_pair(self):
        policy = {'diagnostic_cvars': [], 'diagnostic_prints': {},
                  'editor_status_cvar': 'omnibot_navstudio_goal_token',
                  'editor_print_prefix': 'NAVSTUDIO_GOALS '}
        source = '''function() {
SetCvar("omnibot_navstudio_goal_token", 123);
print("NAVSTUDIO_GOALS obj_test GT2 loaded");
Map.NavStudioTactics = { route = 1 }; }'''
        filtered, audit = filter_diagnostics(source, 'et/nav/obj_test_gt2.gm', policy, ['obj_test'])
        self.assertEqual(len(audit), 2)
        self.assertIn('Map.NavStudioTactics = { route = 1 };', filtered)
        with self.assertRaises(ValueError):
            filter_diagnostics(source.replace('SetCvar(', 'x = SetCvar('),
                               'et/nav/obj_test_gt2.gm', policy, ['obj_test'])


if __name__ == '__main__':
    unittest.main()
