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
        self.original=restyle.read(ROOT/'pak/ui/playonline.menu')
        self.output=restyle.menu(self.original,Path('ui/playonline.menu'))

    def test_five_actions_share_a_row_and_footer_contains_only_main_menu(self):
        found={}
        for _,_,name,args in restyle.macro_calls(self.output,{'BUTTON','NAMEDBUTTON'}):
            offset=int(name=='NAMEDBUTTON');label=args[offset+4].strip('"')
            found[label]=tuple(float(x) for x in args[offset:offset+4])
        labels=('Refresh List!','Connect to IP','Server Info','Enable PB','Join Server')
        for left,right in zip(labels,labels[1:]):
            self.assertEqual(found[left][1],406);self.assertLessEqual(found[left][0]+found[left][2],found[right][0])
        self.assertEqual(found['Disable PB'],found['Enable PB'])
        self.assertEqual(found['Join Server'],(712,406,112,22))
        self.assertEqual([label for label,rect in found.items() if rect[1]==442],['Main Menu'])

    def test_every_original_button_action_and_label_survives(self):
        def actions(text):
            return sorted((name,tuple(args[4+int(name=='NAMEDBUTTON'):5+int(name=='NAMEDBUTTON')]+args[7+int(name=='NAMEDBUTTON'):]))
                          for _,_,name,args in restyle.macro_calls(text,{'BUTTON','NAMEDBUTTON'}))
        self.assertEqual(actions(self.original),actions(self.output))

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


if __name__=='__main__':unittest.main()
