"""Coverage, duplicate and dynamic-registration regression tests for the docs."""
from __future__ import annotations
import csv
import importlib.util
from pathlib import Path
import re
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('cvar_inventory', HERE/'cvar_inventory.py')
inventory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)
spec = importlib.util.spec_from_file_location('render_cvars', HERE/'render_cvars.py')
renderer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)


class LexicalRegistrationTests(unittest.TestCase):
    def scan(self, source, path='src/cgame/fixture.c'):
        return inventory.scan_text(path, source)

    def test_comments_strings_and_lines(self):
        source = '''// trap_Cvar_Register(&x, "false", "9", 0);
/* multi-line
trap_Cvar_Register(&x,"also_false","9",0); */
trap_Cvar_Register(&x, "true_name", "a//b", CVAR_ARCHIVE | CVAR_USERINFO);
'''
        rows, unresolved = self.scan(source)
        self.assertFalse(unresolved)
        self.assertEqual([(r['name'],r['default'],r['line'],r['flag_bits']) for r in rows],
                         [('true_name','a//b',4,3)])

    def test_table_and_dispatcher_no_double_count(self):
        rows, unresolved = self.scan('''
cvarTable_t declarations[] = {
 { &cv, "cg_Mixed", ".85", CVAR_ARCHIVE, 0 },
 { NULL, "", "0", 64, 0 },
};
trap_Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
''')
        self.assertFalse(unresolved)
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[1]['name'], '')
        self.assertEqual(rows[1]['flags'], 'ROM')

    def test_guards_and_scope(self):
        rows, unresolved = self.scan('''
#ifdef FEATURE_NAVSTUDIO
trap_Cvar_Register(&x,"editor","1",0);
#elif defined(OTHER)
trap_Cvar_Register(&x,"other","0",0);
#else
trap_Cvar_Register(&x,"game","0",0);
#endif
''')
        self.assertFalse(unresolved)
        self.assertEqual(rows[0]['scope'], 'Editor')
        self.assertIn('!(defined(FEATURE_NAVSTUDIO))', rows[1]['conditions'])
        self.assertEqual(rows[2]['scope'], 'Game/Editor')

    def test_literal_concatenation_and_prototype(self):
        rows, unresolved = self.scan('''
void trap_Cvar_Register(vmCvar_t *a, const char *b, const char *c, int d);
trap_Cvar_Register(&x, "cg_" "split", "line\\nquote\\\"", CVAR_CHEAT);
''')
        self.assertFalse(unresolved)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['name'], 'cg_split')
        self.assertEqual(rows[0]['default'], 'line\nquote"')

    def test_paired_arrays_preserve_c_strings(self):
        rows, unresolved = self.scan(r'''
const char *names[] = { "ui_one", "ui_two" };
const char *values[] = { "don't", "a\\b\"c" };
trap_Cvar_Register(NULL,names[i],values[i],CVAR_ARCHIVE);
''')
        self.assertFalse(unresolved)
        self.assertEqual([r['kind'] for r in rows], ['paired-array']*2)
        self.assertEqual([r['default'] for r in rows], ["don't", 'a\\b"c'])

    def test_formatted_rgb_array(self):
        rows, unresolved = self.scan('''
const char *channels[] = {"R","G","B"};
const char *defaults[] = {".1",".2",".3"};
trap_Cvar_Register(NULL,va("ui_theme%s",channels[i]),defaults[i],CVAR_ARCHIVE);
''')
        self.assertFalse(unresolved)
        self.assertEqual([r['name'] for r in rows], ['ui_themeR','ui_themeG','ui_themeB'])

    def test_uncovered_dynamic_call_is_not_silently_ignored(self):
        rows, unresolved = self.scan('trap_Cvar_Register(NULL,name,lookup(i),0);')
        self.assertFalse(rows)
        self.assertEqual(len(unresolved), 1)
        self.assertEqual(unresolved[0]['name'], 'name')
        with self.assertRaisesRegex(ValueError, 'Uncovered table dispatcher'):
            self.scan('trap_Cvar_Register(cv->vmCvar,cv->cvarName,cv->defaultString,cv->flags);')

    def test_casefold_equivalent_defaults_and_binding_changes(self):
        rows, _ = self.scan('''
trap_Cvar_Register(&x,"CG_Name",".85",CVAR_ARCHIVE);
trap_Cvar_Register(&x,"cg_name","0.850",CVAR_ARCHIVE);
''')
        signatures = inventory.signatures(rows)
        self.assertEqual(len(signatures), 1)
        self.assertEqual(len(signatures['cg_name']), 1)
        rows[1]['flag_bits'] = 3
        self.assertEqual(len(inventory.signatures(rows)['cg_name']), 2)

    def test_numeric_and_symbolic_flags(self):
        self.assertEqual(inventory.flag_bits('(CVAR_ARCHIVE | CVAR_CHEAT) & ~CVAR_USERINFO'), 513)
        self.assertEqual(inventory.flag_names(513,''), 'ARCHIVE|CHEAT')
        self.assertIsNone(inventory.flag_bits('UNKNOWN_FLAG'))


class PublishedCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows, cls.unresolved = inventory.snapshot('WORKTREE')
        cls.rows = inventory.annotated(cls.rows)
        cls.by_name = {}
        for row in cls.rows:
            cls.by_name.setdefault(row['name'].casefold(), []).append(row)
        def read(name):
            with (inventory.ROOT/'Docs'/name).open(encoding='utf-8',newline='') as stream:
                return list(csv.DictReader(stream,delimiter='\t'))
        cls.history = read('cvars-history.tsv')
        cls.descriptions = read('cvars-descriptions.tsv')

    def test_no_unresolved_registrations_or_stale_inventory(self):
        self.assertFalse(self.unresolved)
        actual = (inventory.ROOT/'Docs/cvars-registration.tsv').read_text(encoding='utf-8')
        self.assertEqual(actual, inventory.tsv(self.rows, inventory.FIELDS+['origin','engine_mirror']))

    def test_descriptions_cover_added_and_changed_names(self):
        described = [r['name'].casefold() for r in self.descriptions]
        required = {r['name'].casefold() for r in self.rows if r['origin']!='ET SDK inherited'}
        required |= {e['name'].casefold() for e in self.history if e['action']=='changed'} & self.by_name.keys()
        self.assertEqual(len(described), len(set(described)))
        self.assertFalse(required-set(described))

    def test_public_editor_boundary_and_dynamic_families(self):
        for name in ('bot_editwaypoints','cg_omnibotdrawing','cg_omnibot_render_distance','omnibot_tce_explore_debug'):
            self.assertEqual({r['scope'] for r in self.by_name[name]}, {'Editor'})
        self.assertEqual({r['scope'] for r in self.by_name['omnibot_enable']}, {'Game/Editor'})
        for name in ('ui_themeaccentr','ui_themeaccentg','ui_themeaccentb',
                     'ui_themebackgroundr','ui_themebackgroundg','ui_themebackgroundb'):
            self.assertEqual({r['kind'] for r in self.by_name[name]}, {'formatted-array'})
        self.assertEqual({r['kind'] for r in self.by_name['ui_backgroundcountry']}, {'paired-array','direct'})

    def test_inherited_engine_names_not_mislabeled_as_new(self):
        for name in ('g_antilag','cg_drawcompass','cg_drawfireteamoverlay'):
            self.assertEqual({r['origin'] for r in self.by_name[name]}, {'ET SDK inherited'})
        self.assertEqual({r['engine_mirror'] for r in self.by_name['r_fastsky']}, {'yes'})
        self.assertEqual({r['engine_mirror'] for r in self.by_name['mapname']}, {'yes'})
        self.assertEqual({r['engine_mirror'] for r in self.by_name['g_oss']}, {'no'})

    def test_removed_experiment_and_reachable_versions(self):
        events = [e for e in self.history if e['name'].casefold()=='cg_weaponlight']
        self.assertEqual([(e['action'],e['commit']) for e in events],
                         [('added','3e9b943'),('removed','0fa43eb')])
        self.assertNotIn('cg_weaponlight', self.by_name)
        self.assertEqual({e['name'].casefold() for e in self.history if e['version']=='1.0.4'},
                         {'cg_commandmapscale'})

    def test_rendered_reference_current_and_local_links_resolve(self):
        document = (inventory.ROOT/'Docs/cvars.md').read_text(encoding='utf-8')
        self.assertEqual(document, renderer.render())
        for target in re.findall(r'\]\(([^)]+)\)', document):
            if not target.startswith(('https:','http:','#')):
                self.assertTrue((inventory.ROOT/'Docs'/target.split('#')[0]).is_file(), target)
        for target in re.findall(r'\{download\}`[^`]+ <([^>]+)>`', document):
            self.assertTrue((inventory.ROOT/'Docs'/target).is_file(), target)

    def test_unregistered_inputs_not_presented_as_native(self):
        document = (inventory.ROOT/'Docs/cvars.md').read_text(encoding='utf-8')
        for name in ('ui_backgroundweatherstamp','ui_backgroundutcoffset','ui_backgroundsunrise',
                     'ui_backgroundsunset','ui_backgroundsnow','ui_backgroundlocation',
                     'ui_backgroundstatus','omnibot_tce_roamcamp','omnibot_tce_objective_debug'):
            self.assertNotIn(name, self.by_name)
            self.assertIn('`'+name+'`', document.lower())
        self.assertEqual(len(set(re.findall(r'`(ui_tceGfx_[^`]+)`', document))), 20)


if __name__ == '__main__':
    unittest.main()
