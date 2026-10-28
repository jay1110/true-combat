"""Independent expanded-item geometry checks, including old origin regressions."""
from pathlib import Path
import tempfile
import unittest

import geometry_audit as audit


class Geometry(unittest.TestCase):
    def test_numeric_geometry_is_bounded_arithmetic_only(self):
        self.assertEqual(audit.arithmetic('rect $evalfloat(12+2*(8-3)) 3 $evalint(9/2) 8'),'rect 22.0 3 4 8')
        for bad in ('run(1)','os.environ[0]','1e999','x+1','2**100','1/0'):
            with self.subTest(bad=bad),self.assertRaises((ValueError,ZeroDivisionError)):
                audit.number(bad)

    def test_item_origin_is_relative_to_parent_and_scripts_cannot_change_parsed_rect(self):
        menus=audit.parse('''menuDef { name "dialog" rect 106 80 640 300
          onOpen { setitemcolor text forecolor 1 1 1 1 ; }
          itemDef { name "button" rect 700 30 120 24 visible 1 type 1
           action { exec "rect 0 0 1 1" ; } }
        }''')
        issues,count=audit.inspect(menus,'ui/dialog.menu')
        self.assertEqual(count,1)
        self.assertEqual(issues[0]['rect'],[806,110,120,24])
        self.assertEqual(issues[0]['kind'],'item_outside_canvas')

    def test_no_double_parent_offset_or_false_positive_for_fullscreen(self):
        menus=audit.parse('''menuDef { name "screen" fullscreen 1 rect 106 80 640 300
          itemDef { name "button" rect 700 30 120 24 visible 1 type 1 } }''')
        self.assertEqual(audit.inspect(menus,'ui/dialog.menu')[0],[])

    def test_initially_hidden_and_conditionally_visible_controls_are_still_reviewed(self):
        menus=audit.parse('''menuDef { name "screen" rect 0 0 852 480
          itemDef { name "conditional" rect 820 30 120 24 visible 0 cvarTest "authLevel" showCvar { 2 } } }''')
        issues,_=audit.inspect(menus,'ui/dialog.menu')
        self.assertEqual(issues[0]['initial_visible'],0)
        self.assertEqual(issues[0]['item'],'conditional')

    def test_name_token_paste_matches_et_string_value_concatenation(self):
        menus=audit.parse('menuDef { name "screen" itemDef { name "bttn" "Back" text "Back" rect 1 2 30 12 } }')
        self.assertEqual(menus[0]['items'][0]['name'],'bttnBack')

    def test_real_shipped_fonts_distinguish_condensed_header_and_regular_body(self):
        base=audit.ROOT/'ui-modern/assets/fonts/tce2_ui'
        condensed=audit.Font(base/'BarlowCondensed-SemiBold.ttf')
        regular=audit.Font(base/'Barlow-Regular.ttf')
        self.assertLess(condensed.width('Deployment',.3),regular.width('Deployment',.3))
        self.assertEqual(regular.width('^1Deployment',.3),regular.width('Deployment',.3))
        self.assertEqual(regular.width('Deployment',.6),2*regular.width('Deployment',.3))

    def test_static_button_text_overflow_is_detected_and_wrapped_paragraph_is_distinct(self):
        class Metric:
            def width(self,text,scale):return len(text)*10
        menus=audit.parse('''menuDef { name "screen" rect 0 0 852 480
          itemDef { name "narrow" rect 20 30 20 20 type 1 text "Long label" textalign 1 textalignx 10 }
          itemDef { name "paragraph" rect 20 80 50 100 text "An intentionally long wrapped paragraph" autowrapped } }''')
        issues,_=audit.inspect(menus,'ui/dialog.menu',{0:Metric()})
        self.assertTrue(any(x['kind']=='button_label_outside_tile' and x['item']=='narrow' for x in issues))
        self.assertFalse(any(x['item']=='paragraph' for x in issues))


if __name__=='__main__':unittest.main()
