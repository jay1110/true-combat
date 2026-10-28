"""Actual input/error dialog and mod-selector geometry/behavior contracts."""
from pathlib import Path
import re
import unittest

import layout_misc
import restyle
import semantic
from test_layout_settings import semantic_context


ROOT=Path(__file__).resolve().parents[2]
PAGES=('popup_password.menu','playonline_connecttoip.menu',
       'popup_hostgameerrormessage.menu','mods.menu','tcemods.menu')


def source(name):
    return restyle.read(ROOT/'pak/ui'/name)


def styled(name):
    return layout_misc.apply(restyle.visual(source(name)),Path('ui')/name)


class InputDialogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contracts,cls.names=semantic_context()

    def test_each_actual_page_keeps_all_original_semantics(self):
        for name in PAGES:
            with self.subTest(name=name):
                path='ui/'+name
                before,_=semantic.signatures([semantic.parse(path,source(name).encode('latin1'))],self.contracts)
                after,_=semantic.signatures([semantic.parse(path,styled(name).encode('latin1'))],self.contracts,self.names)
                self.assertEqual(before,after)

    def test_native_edit_focus_labels_limits_and_cvars_survive_with_large_fields(self):
        for name in PAGES[:2]:
            with self.subTest(name=name):
                result=styled(name)
                old=list(restyle.macro_calls(source(name),{'EDITFIELDLEFT'}))
                new=list(restyle.macro_calls(result,{'EDITFIELDLEFT'}))
                self.assertEqual(len(new),1)
                args=new[0][3]
                self.assertEqual(args[:7],['228','216','396','28',old[0][3][4],'.26','20'])
                self.assertEqual(args[7:],old[0][3][7:])
                self.assertIn('setEditFocus '+('"efleftPassword:"' if name.startswith('popup') else '"efleftServer IP:"'),result)
                buttons=list(restyle.macro_calls(result,{'BUTTON'}))
                self.assertEqual(len(buttons),2)
                for _,_,_,button in buttons:
                    self.assertEqual(button[1:4],['288','190','30'])
                    self.assertGreaterEqual(float(button[1])-244,24)
                backs=[result[s:e] for s,e in restyle.blocks(result) if '"efleftback' in result[s:e]]
                self.assertEqual(len(backs),1)
                self.assertRegex(backs[0],r'\brect\s+(?:284|288)\s+216\s+(?:336|340)\s+28\b')
                self.assertRegex(backs[0],r'\bborder\s+WINDOW_BORDER_FULL\b')

    def test_dynamic_error_text_has_wrapping_space_above_its_footer(self):
        result=styled('popup_hostgameerrormessage.menu')
        labels=list(restyle.macro_calls(result,{'CVARLABEL'}))
        self.assertEqual(len(labels),1)
        args=labels[0][3]
        self.assertEqual(args[:6],['228','154','396','160','"com_errorMessage"','.26'])
        button=list(restyle.macro_calls(result,{'BUTTON'}))[0][3]
        self.assertEqual(button[:4],['228','338','190','30'])
        self.assertGreaterEqual(float(button[1])-(float(args[1])+float(args[3])),24)

    def test_mod_list_keeps_feeder_and_double_click_with_wide_readable_rows(self):
        result=styled('mods.menu')
        block=next(result[s:e] for s,e in restyle.blocks(result) if 'FEEDER_MODS' in result[s:e])
        for snippet in ('24 66 804 350','elementheight\t26','columns 1 0 780 33','uiScript RunMod'):
            self.assertIn(snippet,block)

    def test_tce_modes_retains_original_native_selector_not_a_new_listbox(self):
        result=styled('tcemods.menu')
        self.assertNotIn('ITEM_TYPE_LISTBOX',result)
        old=list(restyle.macro_calls(source('tcemods.menu'),{'MULTIACTION'}))[0][3]
        new=list(restyle.macro_calls(result,{'MULTIACTION'}))[0][3]
        self.assertEqual(new[:4],['168','204','516','32'])
        self.assertEqual(new[4],old[4]);self.assertEqual(new[7:],old[7:])
        self.assertIn('modern_tce_mode_card',result)

    def test_rename_keeps_separate_back_and_commit_actions(self):
        result=restyle.menu(source('profile_rename.menu'),Path('ui/profile_rename.menu'))
        buttons=[a for _,_,_,a in restyle.macro_calls(result,{'BUTTON'})]
        self.assertEqual(len(buttons),2)
        self.assertNotEqual(buttons[0][:2],buttons[1][:2])
        self.assertEqual({a[4] for a in buttons},{'"BACK"','"RENAME"'})
        self.assertEqual({a[1] for a in buttons},{'442'})

    def test_punkbuster_action_has_its_own_row_above_error_dialog_footer(self):
        result=styled('popup_errormessage.menu')
        named=[a for _,_,_,a in restyle.macro_calls(result,{'NAMEDBUTTON'})]
        buttons=[a for _,_,_,a in restyle.macro_calls(result,{'BUTTON'})]
        self.assertEqual(len(named),1);self.assertEqual(len(buttons),2)
        self.assertEqual(named[0][0],'"bttn_pbenable"')
        self.assertEqual(named[0][2],'SUBWINDOW_Y+SUBWINDOW_HEIGHT-80')
        self.assertTrue(all(a[1]=='SUBWINDOW_Y+SUBWINDOW_HEIGHT-42' for a in buttons))
        self.assertNotEqual(buttons[0][0],buttons[1][0])
        field=list(restyle.macro_calls(result,{'CVARLABEL'}))[0][3]
        self.assertLess(60+int(field[3]),308-80)

    def test_single_named_modal_action_is_large_and_parent_has_no_double_offset(self):
        result=styled('ingame_tapoutlms.menu')
        self.assertRegex(result,r'#define WINDOW_X 0\b')
        self.assertRegex(result,r'#define WINDOW_WIDTH 852\b')
        named=list(restyle.macro_calls(result,{'NAMEDBUTTON'}))[0][3]
        width=int(re.search(r'#define SUBWINDOW_WIDTH (\d+)',result)[1])
        offset=int(named[1].removeprefix('SUBWINDOW_X+'))
        self.assertEqual(offset+190/2,width/2)
        self.assertEqual(named[2:5],['SUBWINDOW_Y+SUBWINDOW_HEIGHT-42','190','30'])


if __name__=='__main__':
    unittest.main()
