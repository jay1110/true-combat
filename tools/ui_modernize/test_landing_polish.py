"""Landing-card paint order and real native solid-divider contracts."""
from pathlib import Path
import re
import unittest

import layout_common
import layout_settings
import restyle
from test_widget_continuations import physical_macros


ROOT=Path(__file__).resolve().parents[2]


class LandingPolishTests(unittest.TestCase):
    def test_gold_icons_follow_opaque_original_buttons_and_remain_decorations(self):
        source=restyle.read(ROOT/'pak/ui/options.menu')
        result=layout_settings.apply(restyle.visual(source),Path('ui/options.menu'))
        buttons=list(restyle.macro_calls(result,{'BUTTON'}))
        last_button=max(end for _,end,_,_ in buttons)
        icons={}
        for start,end in restyle.blocks(result):
            block=result[start:end]
            name=re.search(r'(?m)^\s*name\s+"(modern_options_icon_[^"]+)"',block)
            if not name:continue
            self.assertGreater(start,last_button)
            self.assertRegex(block,r'\brect\s+\d+\s+\d+\s+64\s+64\b')
            self.assertIn('forecolor .94 .75 .27 1',block)
            self.assertRegex(block,r'(?m)^\s*decoration\s*$')
            self.assertNotRegex(block,r'\b(?:action|cvar|ownerdraw|feeder|ITEM_TYPE_BUTTON)\b')
            icons[name[1]]=re.search(r'background\s+"([^"]+)"',block)[1]
        self.assertEqual(icons,{'modern_options_icon_game':'tce2_ui/icons/play',
                                'modern_options_icon_view':'tce2_ui/icons/graphics',
                                'modern_options_icon_controls':'tce2_ui/icons/controls',
                                'modern_options_icon_system':'tce2_ui/icons/settings'})
        self.assertEqual([a[4] for _,_,_,a in buttons],
                         [a[4] for _,_,_,a in restyle.macro_calls(source,{'BUTTON'})])

    def test_private_header_title_rules_are_solid_without_sparse_texture(self):
        original=restyle.read(ROOT/'pak/ui/menumacros.h')
        widgets=layout_common.widgets(restyle.macros(original))
        rules=[]
        for start,end in restyle.blocks(widgets):
            block=widgets[start:end]
            if not re.search(r'\bname\s+"titlebargradient"',block):continue
            rules.append(block)
            self.assertRegex(block,r'\bstyle\s+WINDOW_STYLE_FILLED\b')
            self.assertRegex(block,r'\bbackcolor\s+\.94\s+\.75\s+\.27\s+\.75\b')
            self.assertNotRegex(block,r'\bbackground\b')
            self.assertIn('24 46 $evalint(WINDOW_WIDTH-48) 2',block)
        self.assertEqual(len(rules),2) # FUI and ingame variants
        physical_macros(widgets)


if __name__=='__main__':
    unittest.main()
