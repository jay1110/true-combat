"""Real menu navigation, fixed native slider geometry and numeric field fit."""
from pathlib import Path
import re
import unittest

import layout_common
import layout_settings
import restyle
import semantic as sem
from test_layout_settings import font_width
from verify_package import compare_semantics

ROOT=Path(__file__).resolve().parents[2]


def item_blocks(text):
    result={}
    for start,end in restyle.blocks(text):
        block=text[start:end]
        match=re.search(r'\bname\s+"([^"]+)"',block)
        if match:result[match[1]]=block
    return result


def styled(filename,base='pak'):
    return restyle.menu(restyle.read(ROOT/base/'ui'/filename),Path('ui')/filename)


class NavigationPolish(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original={p:d for p,d in sem.source_files(ROOT/'pak').items()
                      if Path(p).suffix in sem.SEMANTIC_SUFFIXES}
        cls.edited={p:d for p,d in sem.source_files(ROOT/'ui-modern',all_files=True).items()
                    if Path(p).suffix in sem.SEMANTIC_SUFFIXES}
        _,cls.contracts=sem.signatures([sem.parse(p,d) for p,d in cls.original.items()])

    def test_all_five_settings_pages_have_full_tabs_and_real_mouse_keyboard_feedback(self):
        for current,(_,page) in layout_common.SETTINGS.items():
            text=((ROOT/'ui-modern/ui/modern_appearance.menu').read_text(encoding='latin1')
                  if current=='appearance' else styled(page+'.menu'))
            items=item_blocks(text)
            rectangles=[]
            for key in layout_common.SETTINGS:
                name='modern_nav_'+key;self.assertIn(name,items)
                fields=sem._navigation_fields(sem.parse('item.menu',items[name].encode('latin1')).body[2:-1],self.contracts)
                if key==current:
                    self.assertTrue(fields['decoration']);self.assertNotIn('action',fields)
                else:
                    self.assertEqual(fields['type'],'ITEM_TYPE_BUTTON')
                    for event in sem.HOVER_FIELDS:self.assertTrue(fields[event])
                    self.assertIn(layout_common.SETTINGS[key][1],fields['action'])
                x,y,w,h=map(float,re.search(r'\brect\s+([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',items[name]).groups())
                self.assertGreaterEqual(x,252);self.assertLessEqual(x+w,828)
                self.assertEqual((y,h),(12,30))
                for px,_,pw,_ in rectangles:self.assertGreaterEqual(x,px+pw+4)
                rectangles.append((x,y,w,h))

    def test_hover_cannot_execute_commands_or_paint_other_controls(self):
        candidate=dict(self.edited);path='ui/options.menu'
        for old,new in ((b'setitemcolor "modern_appearance_link" backcolor',b'exec quit ; setitemcolor "modern_appearance_link" backcolor'),
                        (b'setitemcolor "modern_appearance_link" backcolor',b'setitemcolor "window" backcolor')):
            with self.subTest(new=new):
                candidate=dict(self.edited)
                self.assertIn(old,candidate[path]);candidate[path]=candidate[path].replace(old,new,1)
                with self.assertRaisesRegex(ValueError,'hover'):compare_semantics(self.original,candidate)

    def test_controls_numeric_value_has_a_separate_row_and_every_card_has_bottom_padding(self):
        for base in ('pak','src/assets'):
            raw=restyle.read(ROOT/base/'ui/options_controls.menu')
            result=layout_settings.apply(restyle.visual(raw),Path('ui/options_controls.menu'))
            calls=list(restyle.macro_calls(result,layout_settings.CALLS))
            groups=layout_settings._sections(calls)
            for name,group in groups.items():
                x,y,w,h=map(float,group['header'][3][:4])
                self.assertLessEqual(y+h,432)
                for _,_,macro,args in group['rows']:
                    baseline=float(args[7] if macro.startswith('CHECKBOXALIGNX') else args[6])
                    painted_bottom=float(args[1])+baseline+3
                    self.assertGreaterEqual(y+h-painted_bottom,2,(base,name,args[4]))
                for _,_,_,args in group['labels']:
                    self.assertGreaterEqual(y+h-(float(args[1])+float(args[8])+3),3)
            slider=next(args for _,_,name,args in calls if name=='SLIDER' and args[7].startswith('"sensitivity"'))
            readout=next(args for _,_,name,args in calls if name=='CVARFLOATLABEL' and args[4]=='"sensitivity"')
            # Native track/thumb heights are10/12 and width96; changing its
            # source rect width does not shrink the actual rendered track.
            self.assertGreaterEqual(float(readout[1]),float(slider[1])+12+3)
            self.assertLessEqual(float(slider[0])+float(slider[2])/2+8+96+6,554)

    def test_vote_subpages_retain_all_three_routes_and_original_authority_variants(self):
        for page,current in layout_common.VOTE_PAGES.items():
            text=styled(page+'.menu');items=item_blocks(text)
            names={n for n in items if n.startswith('modern_vote_nav_')}
            expected={'modern_vote_nav_'+current}|{
                'modern_vote_nav_'+key+'_'+str(case)
                for key in ('misc','map','players') if key!=current for case in (0,1)}
            self.assertEqual(names,expected)
            for name in names:
                block=items[name]
                fields=sem._navigation_fields(sem.parse('item.menu',block.encode('latin1')).body[2:-1],self.contracts)
                if name=='modern_vote_nav_'+current:
                    self.assertTrue(fields['decoration']);continue
                self.assertEqual(fields['cvartest'],'"authLevel"')
                self.assertIn('showcvar',fields)
                for event in sem.HOVER_FIELDS:self.assertTrue(fields[event])
        self.assertEqual(compare_semantics(self.original,self.edited),77)

    def test_vote_tabs_cannot_bypass_privilege_flags_or_redirect_to_other_actions(self):
        path='ui/ingame_vote_map.menu';text=self.edited[path].decode('latin1')
        block=item_blocks(text)['modern_vote_nav_players_0']
        for old,new in (('RL_NONE','RL_REFEREE'),('open ingame_vote_players','open quit')):
            with self.subTest(new=new):
                candidate=dict(self.edited)
                changed=block.replace(old,new,1);self.assertNotEqual(changed,block)
                candidate[path]=text.replace(block,changed,1).encode('latin1')
                with self.assertRaisesRegex(ValueError,'Vote navigation'):compare_semantics(self.original,candidate)

    def test_vote_timelimit_value_and_maximum_six_digits_stay_inside_background(self):
        for name in ('ingame_vote_misc.menu','ingame_vote_misc_refrcon.menu'):
            text=styled(name)
            field=list(restyle.macro_calls(text,{'NUMERICFIELDLEFTEXT'}))[0][3]
            background=item_blocks(text)['nfleftbackTimelimit:']
            x,y,w,h=map(float,re.search(r'\brect\s+([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',background).groups())
            value_x=float(field[0])+font_width(field[4].strip('"'),float(field[5]))+8
            self.assertGreaterEqual(value_x,x+2)
            self.assertLessEqual(value_x+font_width('999999',float(field[5])),x+w-2)
            self.assertEqual(y,float(field[1]));self.assertEqual(h,float(field[3]))

    def test_main_navigation_is_narrower_without_changing_original_actions(self):
        for folder in ('ui','ui_obj','ui_robj','ui_tdm'):
            path=Path(folder)/'main.menu';raw=restyle.read(ROOT/'pak'/path)
            result=restyle.menu(raw,path)
            old=list(restyle.macro_calls(raw,{'BUTTON','BUTTONTEXTONLY'}))
            new=list(restyle.macro_calls(result,{'BUTTON','BUTTONTEXTONLY'}))
            self.assertEqual(len(new),len(old))
            for (_,_,_,before),(_,_,_,after) in zip(old,new):
                self.assertEqual(after[4],before[4]);self.assertEqual(after[7:],before[7:])
                self.assertEqual(after[2],'128')
                self.assertLessEqual(40+font_width(after[4].strip('"'),float(after[5]),
                                                 'BarlowCondensed-SemiBold.ttf')+16,128)


if __name__=='__main__':unittest.main()
