"""Browser control reachability, native help wrapping and unchanged actions."""
from pathlib import Path
import re
import unittest
import layout_browse
import restyle
from test_layout_settings import font_width

ROOT=Path(__file__).resolve().parents[2]


class BrowserFlow(unittest.TestCase):
    def setUp(self):
        self.original=restyle.read(ROOT/'pak_original_tce/ui/playonline.menu')
        self.output=restyle.menu(self.original,Path('ui/playonline.menu'))

    def test_five_actions_share_a_row_and_footer_contains_only_main_menu(self):
        found={}
        for _,_,name,args in restyle.macro_calls(self.output,{'BUTTON','NAMEDBUTTON'}):
            offset=int(name=='NAMEDBUTTON');label=args[offset+4].strip('"')
            found[label]=tuple(float(x) for x in args[offset:offset+4])
        labels=('Refresh List!','Connect to IP','Server Info','Enable PB','Join Server')
        for left,right in zip(labels,labels[1:]):
            self.assertEqual(found[left][1],448);self.assertLessEqual(found[left][0]+found[left][2],found[right][0])
        self.assertEqual(found['Disable PB'],found['Enable PB'])
        self.assertEqual(found['Join Server'],(712,448,112,22))
        self.assertEqual([label for label,rect in found.items() if rect[1]==442],['Main Menu'])

    def test_table_uses_two_more_complete_rows_and_scrollbar_reaches_footer(self):
        items={}
        for a,b in restyle.blocks(self.output):
            block=self.output[a:b];match=re.search(r'\bname\s+"([^"]+)"',block)
            if match:items[match[1]]=block
        def rect(name):
            return tuple(map(float,re.search(r'\brect\s+([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',items[name]).groups()))
        x,y,w,h=rect('serverList')
        self.assertEqual((x,y,w,h),(224,232,604,210))
        row_height=float(re.search(r'\belementheight\s+([\d.]+)',items['serverList'])[1])
        self.assertEqual(h/row_height,10)
        for column in ('server','map','player','gametype','ping','filters','favourites'):
            column_rect=rect(column+'Column')
            self.assertEqual((column_rect[1],column_rect[3]),(y,h))
        for stripe in range(1,7):
            stripe_rect=rect('row'+str(stripe))
            self.assertGreaterEqual(stripe_rect[1],y)
            self.assertLessEqual(stripe_rect[1]+stripe_rect[3],y+h)
        buttons={args[4+int(name=='NAMEDBUTTON')].strip('"'):tuple(map(float,args[int(name=='NAMEDBUTTON'):int(name=='NAMEDBUTTON')+4]))
                 for _,_,name,args in restyle.macro_calls(self.output,{'BUTTON','NAMEDBUTTON'})}
        self.assertEqual(buttons['Join Server'][1]-(y+h),6)
        self.assertEqual(buttons['Join Server'][1]+buttons['Join Server'][3],470)
        panel=next(args for _,_,name,args in restyle.macro_calls(self.output,{'SUBWINDOW'}) if args[4]=='"Server Browser"')
        self.assertEqual(float(panel[1])+float(panel[3]),474)

    def test_every_original_button_action_and_label_survives(self):
        def actions(text):
            return sorted((name,tuple(args[4+int(name=='NAMEDBUTTON'):5+int(name=='NAMEDBUTTON')]+args[7+int(name=='NAMEDBUTTON'):]))
                          for _,_,name,args in restyle.macro_calls(text,{'BUTTON','NAMEDBUTTON'}))
        self.assertEqual(actions(self.original),actions(self.output))

    def test_return_browser_matches_full_height_layout_and_keeps_original_actions(self):
        path=Path('ui/playonline_sub.menu')
        original=restyle.read(ROOT/'pak_original_tce'/path)
        generated=restyle.menu(original,path)
        artifact=restyle.read(ROOT/'ui-modern'/path)
        def rectangles(text):
            result={}
            for a,b in restyle.blocks(text):
                block=text[a:b];name=re.search(r'\bname\s+"([^"]+)"',block)
                if name and (name[1]=='serverList' or name[1].endswith('Column') or re.fullmatch(r'row[1-6]',name[1])):
                    result[name[1]]=tuple(map(float,re.search(r'\brect\s+([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',block).groups()))
            return result
        def button_rows(text,geometry):
            result=[]
            for _,_,name,args in restyle.macro_calls(text,{'BUTTON','NAMEDBUTTON'}):
                offset=int(name=='NAMEDBUTTON')
                label=args[offset+4].strip('"').rstrip('!')
                value=tuple(args[offset:offset+4]) if geometry else tuple(args[offset+4:offset+5]+args[offset+7:])
                result.append((label,value))
            return sorted(result)
        for text in (generated,artifact):
            self.assertEqual(rectangles(text),rectangles(self.output))
            self.assertEqual(button_rows(text,True),button_rows(self.output,True))
            self.assertEqual(button_rows(text,False),button_rows(original,False))
            self.assertEqual(layout_browse.apply(text,path),text)

    def test_filter_help_native_wrapping_fits_real_font_and_panel(self):
        block=next(self.output[a:b] for a,b in restyle.blocks(self.output) if re.search(r'\bname\s+"descriptionFilters"',self.output[a:b]))
        self.assertRegex(block,r'\bautowrapped\b');self.assertRegex(block,r'\bcvar\s+ui_filterdescription')
        self.assertRegex(block,r'\brect\s+36 356 160 68\b')
        descriptions=re.findall(r'setcvar ui_filterdescription "([^"]+)"',self.original)
        self.assertEqual(len(descriptions),8)
        for description in descriptions:
            lines=['']
            for word in description.split():
                proposal=(lines[-1]+' '+word).strip()
                if lines[-1] and font_width(proposal,.20)>160:lines.append(word)
                else:lines[-1]=proposal
            self.assertTrue(all(font_width(line,.20)<=160 for line in lines),description)
            self.assertLessEqual(14+(len(lines)-1)*15,68,description)
        self.assertEqual(layout_browse.apply(self.output,Path('ui/playonline.menu')),self.output)

    def test_return_browser_wrapping_cannot_change_bound_help_cvar(self):
        import semantic as sem
        from verify_package import compare_semantics
        original={p:v for p,v in sem.source_files(ROOT/'pak_original_tce').items() if Path(p).suffix in sem.SEMANTIC_SUFFIXES}
        edited={p:v for p,v in sem.source_files(ROOT/'ui-modern',all_files=True).items() if Path(p).suffix in sem.SEMANTIC_SUFFIXES}
        self.assertEqual(compare_semantics(original,edited),78)  # Includes the authorized background settings page.
        path='ui/playonline_sub.menu';text=edited[path].decode('latin1')
        block=next(text[a:b] for a,b in restyle.blocks(text) if re.search(r'\bname\s+"descriptionFilters"',text[a:b]))
        changed=block.replace('ui_filterdescription','sensitivity')
        self.assertNotEqual(block,changed)
        bad=dict(edited);bad[path]=text.replace(block,changed,1).encode('latin1')
        with self.assertRaisesRegex(ValueError,'Unbound filter-description wrapping'):
            compare_semantics(original,bad)


if __name__=='__main__':unittest.main()
