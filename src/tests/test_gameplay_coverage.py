import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import audit_gameplay_coverage as audit

class CoverageTests(unittest.TestCase):
    def test_enum_keeps_sdk_and_tc_names_separate(self):
        text='typedef enum { WP_NONE, WP_A, WP_RIFLE=39, WP_ALIAS=WP_RIFLE, WP_END } weapon_t;'
        self.assertEqual(audit.weapon_enum(text)['WP_RIFLE'],39)
        self.assertEqual(audit.weapon_enum(text)['WP_ALIAS'],39)
        self.assertEqual(audit.weapon_enum(text)['WP_END'],40)
    def test_unknown_enum_expression_rejected(self):
        with self.assertRaises(ValueError): audit.weapon_enum('typedef enum { WP_NONE, WP_X=1<<4 } weapon_t;')
    def test_functions_ignore_fake_calls_comments_strings_and_braces(self):
        text='/* int fake() { WP_A; } */\nvoid real() { if (x) { use(WP_B); } const char *s="void false() { WP_C; }"; }\nint other(void) { return CVAR; }'
        rows=list(audit.functions(text))
        self.assertEqual([r[0] for r in rows],['real','other'])
        self.assertIn('WP_B',rows[0][2]);self.assertNotIn('WP_C',rows[0][2])
    def test_unbalanced_body_rejected(self):
        with self.assertRaises(ValueError): list(audit.functions('void f() {'))

if __name__=='__main__': unittest.main()
