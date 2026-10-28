"""Narrow presentation exceptions must not hide changed UI behavior."""
from pathlib import Path
import unittest

import semantic as sem
from layout_common import hover_feedback
from verify_package import compare_semantics

ROOT=Path(__file__).resolve().parents[2]


class LayoutContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original={p:d for p,d in sem.source_files(ROOT/'pak').items() if Path(p).suffix in sem.SEMANTIC_SUFFIXES}

    def setUp(self):self.reference=dict(self.original);self.candidate=dict(self.reference)

    def check(self):return compare_semantics(self.reference,self.candidate)

    def replace(self,path,old,new):
        self.assertIn(old,self.candidate[path]);self.candidate[path]=self.candidate[path].replace(old,new,1)

    def widgets(self):
        self.candidate[sem.PRIVATE_WIDGETS]=self.original[sem.ORIGINAL_WIDGETS]
        path='ui/options_controls.menu'
        self.replace(path,b'"ui/menumacros.h"',b'"ui/modern_widgets.h"')

    def navigation(self,path='ui/options_controls.menu'):
        unit=sem.parse(path,self.reference[path]);back=sem._back_action(unit)
        blocks=[]
        for key,target in sem.NAV_TARGETS.items():
            active=sem.SETTINGS_NAV[path]==key
            body='decoration' if active else 'type ITEM_TYPE_BUTTON action { '+' '.join(back+[';','close','options',';','open',target])+' }'
            if not active:body+=hover_feedback('modern_nav_'+key)
            blocks.append('itemDef { name "modern_nav_'+key+'" text "'+key.upper()+'" visible 1 group GROUP_NAME '+
                          'rect 280 12 132 30 textfont UI_FONT_ARIBLK_16 textscale .24 '+body+' }')
        data=self.candidate[path];at=data.rfind(b'}')
        self.candidate[path]=data[:at]+'\n'.join(blocks).encode()+data[at:]
        return path

    def test_real_list_positions_widths_and_new_numeric_geometry_constants(self):
        path='ui/playonline.menu'
        self.replace(path,b'#define SERVERNAME_WIDTH\t250',b'#define SERVERNAME_WIDTH 350\n#define COLUMN_PADDING 12')
        self.replace(path,b'7\t0 SERVERNAME_WIDTH 39',b'7 4 $evalfloat(SERVERNAME_WIDTH+COLUMN_PADDING) 39')
        self.assertEqual(self.check(),75)

    def test_column_count_order_and_maxchars_remain_semantic(self):
        path='ui/ingame_serverinfo.menu'
        for old,new in ((b'columns\t\t\t4',b'columns 3'),(b'40\t20',b'40 21')):
            with self.subTest(new=new):
                self.candidate=dict(self.reference);self.replace(path,old,new)
                with self.assertRaises(ValueError):self.check()
        self.candidate=dict(self.reference)
        self.replace('ui/playonline.menu',b'SERVERNAME_WIDTH 39',b'SERVERNAME_WIDTH 13')
        self.replace('ui/playonline.menu',b'MAPNAME_WIDTH 13',b'MAPNAME_WIDTH 39')
        with self.assertRaisesRegex(ValueError,'semantic data changed'):self.check()

    def test_column_expressions_reject_commands_strings_unknown_names_and_cycles(self):
        for bad in (b'"open attacker"',b'$evalfloat(open(4))',b'UNKNOWN_WIDTH',b'$evalfloat(1<2)',b'$evalfloat(1e999)',b'evalfloat'):
            with self.subTest(bad=bad):
                self.candidate=dict(self.reference)
                self.replace('ui/playonline.menu',b'7\t0 SERVERNAME_WIDTH 39',b'7 0 '+bad+b' 39')
                with self.assertRaises(ValueError):self.check()
        self.candidate=dict(self.reference)
        self.replace('ui/playonline.menu',b'#define SERVERNAME_WIDTH\t250',b'#define SERVERNAME_WIDTH COLUMN_PADDING\n#define COLUMN_PADDING SERVERNAME_WIDTH')
        with self.assertRaisesRegex(ValueError,'Cyclic'):self.check()

    def test_column_geometry_define_cannot_change_functional_or_new_unrelated_data(self):
        self.replace('ui/playonline.menu',b'#define SERVERNAME_WIDTH\t250',b'#define SERVERNAME_WIDTH 350\n#define UNRELATED_FUNCTIONAL 7')
        with self.assertRaisesRegex(ValueError,'semantic data changed'):self.check()
        self.candidate=dict(self.reference)
        self.replace('ui/menudef.h',b'#define ITEM_TYPE_BIND',b'#define ITEM_TYPE_BIND_BROKEN')
        with self.assertRaisesRegex(ValueError,'semantic data changed'):self.check()

    def test_private_widget_paint_and_single_include_substitution_accepted(self):
        self.widgets()
        self.replace(sem.PRIVATE_WIDGETS,b'backcolor\t.1 .1 .1 .95',b'backcolor .035 .045 .065 .98')
        self.assertEqual(self.check(),76)

    def test_private_widget_changed_actions_and_macro_contract_not_allowed(self):
        for old,new in ((b'play "sound/menu/select.wav"',b'exec "quit"'),
                        (b'BUTTON_ACTION',b'BUTTON_ACTION_BROKEN')):
            with self.subTest(new=new):
                self.candidate=dict(self.reference);self.widgets()
                self.replace(sem.PRIVATE_WIDGETS,old,new)
                with self.assertRaises(ValueError):self.check()

    def test_private_header_must_be_exactly_named_and_never_retarget_main_global(self):
        for path in ('ui/main.menu','ui_obj/main.menu','ui_tdm/global.menu'):
            with self.subTest(path=path):
                self.candidate=dict(self.reference);self.widgets()
                if b'"ui/menumacros.h"' in self.candidate[path]:
                    self.replace(path,b'"ui/menumacros.h"',b'"ui/modern_widgets.h"')
                else:self.candidate[path]=b'#include "ui/modern_widgets.h"\n'+self.candidate[path]
                with self.assertRaisesRegex(ValueError,'Unapproved private widget include'):self.check()
        self.candidate=dict(self.reference);self.candidate['ui/unapproved_widgets.h']=self.original[sem.ORIGINAL_WIDGETS]
        with self.assertRaisesRegex(ValueError,'coverage differs'):self.check()

    def test_all_four_menu_specific_nav_contracts_accepted_with_inert_active_tab(self):
        for path in sem.SETTINGS_NAV:self.navigation(path)
        self.assertEqual(self.check(),75)

    def test_nav_uses_exact_original_back_action_and_target(self):
        for bad in (b'open attacker',b'exec quit',b'open options_customise_hud'):
            with self.subTest(bad=bad):
                self.candidate=dict(self.reference);path=self.navigation()
                # Only mutate the new inactive GAME target, not original Back.
                at=self.candidate[path].index(b'name "modern_nav_game"')
                tail=self.candidate[path][at:].replace(b'open options_customise_game',bad,1)
                self.candidate[path]=self.candidate[path][:at]+tail
                with self.assertRaisesRegex(ValueError,'exact original exit/target'):self.check()
        self.candidate=dict(self.reference);path=self.navigation('ui/options_system.menu')
        at=self.candidate[path].index(b'name "modern_nav_game"')
        self.candidate[path]=self.candidate[path][:at]+self.candidate[path][at:].replace(b'uiScript vidReset ;',b'',1)
        with self.assertRaisesRegex(ValueError,'exact original exit/target'):self.check()

    def test_nav_no_additional_cvars_feeders_scripts_or_changed_active_behavior(self):
        for bad in (b'cvar "evil"',b'feeder 9',b'mouseEnter { exec quit ; }',b'type ITEM_TYPE_BUTTON action { clearFocus ; }'):
            with self.subTest(bad=bad):
                self.candidate=dict(self.reference);path=self.navigation()
                self.replace(path,b'name "modern_nav_controls"',b'name "modern_nav_controls" '+bad)
                with self.assertRaises(ValueError):self.check()

    def test_nav_incomplete_duplicate_unknown_labels_and_outside_allowlist_fail(self):
        for kind in ('incomplete','duplicate','label','visible','outside'):
            with self.subTest(kind=kind):
                self.candidate=dict(self.reference);path=self.navigation()
                if kind=='incomplete':self.replace(path,b'name "modern_nav_view"',b'name "other_view"')
                elif kind=='duplicate':self.replace(path,b'name "modern_nav_view"',b'name "modern_nav_game"')
                elif kind=='label':self.replace(path,b'text "GAME"',b'text "NEW GAME"')
                elif kind=='visible':
                    at=self.candidate[path].index(b'name "modern_nav_game"')
                    self.candidate[path]=self.candidate[path][:at]+self.candidate[path][at:].replace(b'visible 1',b'visible 0',1)
                else:
                    self.candidate['ui/options.menu']=self.candidate[path]
                with self.assertRaises(ValueError):self.check()


if __name__=='__main__':unittest.main()
