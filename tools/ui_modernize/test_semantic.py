"""Real 69-menu coverage plus positive styling and hostile semantic controls."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import semantic as s

ROOT = Path(__file__).resolve().parents[2]
MACROS = b'''#define BUTTON(X,Y,W,H,T,S,B,A) itemDef { name "button"##T rect X Y W H text T textscale S textaligny B visible 1 action { A } }
#define LABEL(X,Y,W,H,T,S,AL,AX,AY) itemDef { name "label" rect X Y W H text T textscale S textalign AL textalignx AX textaligny AY visible 1 }
'''
HEADER = b'''#define ITEM_TYPE_BIND 4
#define ITEM_TEXTSTYLE_NORMAL 0
#define ITEM_TEXTSTYLE_SHADOWED 3
#define ITEM_ALIGN_LEFT 0
#define UI_FONT_ARIBLK_16 0
#define UI_FONT_COURBD_21 0
'''
MENU = b'''#include "ui/menudef.h"
#include "ui/menumacros.h"
#define WINDOW_WIDTH 100
#define CMD "stats"
menuDef { name "main" rect 1 2 WINDOW_WIDTH 40 backcolor 0 0 0 .8
 onOpen { setitemcolor "bttn"##"Go" forecolor .1 .2 .3 1 ; setcvar "mode" "safe" ;
 conditionalScript state 0 ( "setitemcolor mark forecolor 0 0 0 1 ; open main" ) ( "open other" ) }
 onESC { close main ; } onClose { exec CMD ; }
 itemDef { name "bind" type ITEM_TYPE_BIND cvar "+stats" text "Stats" visible 1
 rect 2 3 90 12 textfont UI_FONT_ARIBLK_16 textstyle ITEM_TEXTSTYLE_SHADOWED
 feeder 4 ownerdraw 17 cvarFloatList { "Off" 0 "On" 1 }
 action { setmenuitemcolor main "bttn"##"Go" backcolor 0 0 0 1 ; exec "hello" ; } }
 BUTTON( 2, 20, 90, 12, "Go", .2, 10, open other ; )
 LABEL( 2, 40, 90, 12, "Choice", .2, ITEM_ALIGN_LEFT cvarTest "mode" hideCvar { 5 }, 0, 8 )
}
'''


class Semantics(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.original = Path(self.temp.name)/'original'
        self.edited = Path(self.temp.name)/'edited'
        for directory in s.DIRECTORIES:
            base = self.original/directory
            base.mkdir(parents=True)
            (base/'main.menu').write_bytes(MENU)
        (self.original/'ui/menumacros.h').write_bytes(MACROS)
        (self.original/'ui/menudef.h').write_bytes(HEADER)
        (self.original/'ui/original-art.bin').write_bytes(b'original immutable pixels')
        shutil.copytree(self.original, self.edited)
        self.manifest = s.inventory(self.original)

    def change(self, old, new, path='ui/main.menu'):
        p = self.edited/path
        data = p.read_bytes()
        self.assertIn(old, data)
        p.write_bytes(data.replace(old, new, 1))

    def review(self, styled=False):
        return s.compare(self.manifest, self.original, self.edited, styled)

    def test_identical_complete_mirror(self):
        self.assertTrue(self.review()['passed'])

    def test_actual_all_menu_inventory_and_identity(self):
        manifest = s.inventory(ROOT/'pak')
        self.assertEqual(manifest['file_count'], 102)
        self.assertEqual(manifest['menu_count'], 69)
        self.assertEqual(manifest['semantic_file_count'], 75)
        self.assertEqual({k:v['menus'] for k,v in manifest['counts'].items()},
                         {'ui':63,'ui_obj':2,'ui_robj':2,'ui_tdm':2})
        self.assertIn('ui/tce_omnibot.menu', {r['path'] for r in manifest['files']})
        self.assertTrue(s.compare(manifest, ROOT/'pak', ROOT/'pak')['passed'])

    def test_layout_palette_font_background_cursor_are_paint(self):
        for old,new in ((b'rect 1 2 WINDOW_WIDTH 40',b'rect 8 12 WINDOW_WIDTH 50'),
                        (b'backcolor 0 0 0 .8',b'backcolor .05 .08 .1 .95'),
                        (b'#define WINDOW_WIDTH 100',b'#define WINDOW_WIDTH 200'),
                        (b'textstyle ITEM_TEXTSTYLE_SHADOWED',b'textstyle ITEM_TEXTSTYLE_NORMAL'),
                        (b'BUTTON( 2, 20, 90, 12, "Go", .2, 10,',b'BUTTON( 6, 30, 100, 16, "Go", .3, 12,')):
            self.change(old,new)
        self.assertTrue(self.review()['passed'], self.review()['errors'])

    def test_script_colors_preserve_concatenated_targets_and_commands(self):
        self.change(b'forecolor .1 .2 .3 1', b'forecolor .9 .7 .2 .8')
        self.change(b'forecolor 0 0 0 1 ; open main', b'forecolor .9 .7 .2 1 ; open main')
        self.change(b'backcolor 0 0 0 1 ; exec', b'backcolor .05 .08 .1 .9 ; exec')
        self.assertTrue(self.review()['passed'], self.review()['errors'])
        self.change(b'"bttn"##"Go"', b'"bttn"##"Different"')
        self.assertFalse(self.review()['passed'])

    def test_actions_labels_cvars_lists_feeders_ownerdraw_bindings_are_protected(self):
        for old,new in ((b'exec "hello"',b'exec "quit"'), (b'open other',b'open attacker'),
                        (b'"mode" "safe"',b'"mode" "unsafe"'),(b'text "Stats"',b'text "Other"'),
                        (b'cvar "+stats"',b'cvar "+attack"'),(b'"On" 1',b'"On" 2'),
                        (b'feeder 4',b'feeder 9'),(b'ownerdraw 17',b'ownerdraw 18'),
                        (b'visible 1',b'visible 0'),(b'name "main"',b'name "different"'),
                        (b'#define CMD "stats"',b'#define CMD "quit"')):
            with self.subTest(old=old):
                (self.edited/'ui/main.menu').write_bytes(MENU.replace(old,new,1))
                self.assertFalse(self.review()['passed'])

    def test_mixed_style_macro_argument_retains_conditional_data(self):
        self.change(b'ITEM_ALIGN_LEFT cvarTest', b'1 cvarTest')
        self.assertTrue(self.review()['passed'])
        self.change(b'hideCvar { 5 }', b'hideCvar { 7 }')
        self.assertFalse(self.review()['passed'])

    def test_macro_body_actions_cannot_be_hidden_as_geometry(self):
        self.change(b'action { A }',b'action { exec "quit" ; }','ui/menumacros.h')
        self.assertFalse(self.review()['passed'])

    def test_new_semantic_suffix_in_paint_argument_is_not_dropped(self):
        self.change(b'BUTTON( 2,',b'BUTTON( 2 action { exec quit ; },')
        self.assertFalse(self.review()['passed'])

    def test_font_slots_only_native_range_and_functional_constants_exact(self):
        self.change(b'UI_FONT_ARIBLK_16 0',b'UI_FONT_ARIBLK_16 3','ui/menudef.h')
        self.assertTrue(self.review()['passed'])
        self.change(b'ITEM_TYPE_BIND 4',b'ITEM_TYPE_BIND 5','ui/menudef.h')
        self.assertFalse(self.review()['passed'])
        (self.edited/'ui/menudef.h').write_bytes(HEADER.replace(b'UI_FONT_ARIBLK_16 0',b'UI_FONT_ARIBLK_16 6'))
        with self.assertRaisesRegex(ValueError,'font slot'):
            self.review()

    def test_paint_usage_changes_cannot_reclassify_identical_header(self):
        self.change(b'textstyle ITEM_TEXTSTYLE_SHADOWED',b'textstyle ITEM_TEXTSTYLE_NORMAL')
        self.assertTrue(self.review()['passed'], self.review()['errors'])
        self.change(b'ITEM_TEXTSTYLE_NORMAL 0',b'ITEM_TEXTSTYLE_NORMAL 7','ui/menudef.h')
        self.assertFalse(self.review()['passed'])

    def test_decorative_only_additions_allowed_but_labels_actions_and_name_reuse_not(self):
        decoration=b'itemDef { name "new-accent" rect 0 0 4 40 background "theme/fill" backcolor .9 .7 .2 1 visible 1 decoration }'
        self.change(b' onESC',decoration+b' onESC')
        self.assertTrue(self.review()['passed'],self.review()['errors'])
        for extra in (b'text "New label"', b'action { exec quit ; }', b'cvar "new"'):
            with self.subTest(extra=extra):
                data=MENU.replace(b' onESC',decoration.replace(b'decoration }',b'decoration '+extra+b' }')+b' onESC')
                (self.edited/'ui/main.menu').write_bytes(data)
                self.assertFalse(self.review()['passed'])
        (self.edited/'ui/main.menu').write_bytes(MENU.replace(b' onESC',decoration.replace(b'new-accent',b'bind')+b' onESC'))
        self.assertFalse(self.review()['passed'])

    def test_original_art_or_text_change_invalidates_inventory(self):
        (self.original/'ui/original-art.bin').write_bytes(b'changed')
        self.assertFalse(self.review()['passed'])

    def test_missing_unused_menu_and_extra_menu_outside_roots_fail(self):
        (self.edited/'ui_obj/main.menu').unlink()
        self.assertFalse(self.review()['passed'])
        (self.edited/'ui_obj/main.menu').write_bytes(MENU)
        extra=self.edited/'theme'
        extra.mkdir();(extra/'hidden.menu').write_bytes(MENU)
        self.assertFalse(self.review()['passed'])

    def test_font_license_is_an_asset_not_an_extra_menu(self):
        extra=self.edited/'assets/fonts'
        extra.mkdir(parents=True);(extra/'LICENSE.txt').write_text('Font license only.')
        result=self.review()
        self.assertTrue(result['passed'],result['errors'])
        self.assertIn('assets/fonts/LICENSE.txt',result['additional_assets'])

    def test_comment_only_is_not_a_styled_menu(self):
        self.change(b'menuDef',b'// changed palette\nmenuDef')
        result=self.review(True)
        self.assertFalse(result['passed'])
        self.assertEqual(len(result['unstyled_menus']),4)
        for directory in s.DIRECTORIES:
            p=self.edited/directory/'main.menu'
            p.write_bytes(p.read_bytes().replace(b'rect 1 2 WINDOW_WIDTH 40',b'rect 4 5 WINDOW_WIDTH 40'))
        self.assertTrue(self.review(True)['passed'])

    def test_legacy_label_bytes_cannot_be_silently_transcoded(self):
        original=MENU.replace(b'text "Stats"',b'text "Gr\xf6sse"')
        for root in (self.original,self.edited):
            (root/'ui/main.menu').write_bytes(original)
        self.manifest=s.inventory(self.original)
        self.assertTrue(self.review()['passed'])
        (self.edited/'ui/main.menu').write_bytes(original.decode('latin-1').encode('utf-8'))
        self.assertFalse(self.review()['passed'])

    def test_crlf_whitespace_and_comments_do_not_change_semantics(self):
        self.change(b'menuDef',b'/* style comment */\nmenuDef')
        p=self.edited/'ui/menudef.h';p.write_bytes(p.read_bytes().replace(b'\n',b'\r\n'))
        self.assertTrue(self.review()['passed'])

    def test_cli_fail_closed_on_changed_action(self):
        manifest=Path(self.temp.name)/'manifest.json';manifest.write_text(json.dumps(self.manifest))
        output=Path(self.temp.name)/'result.json'
        command=[sys.executable,str(ROOT/'tools/ui_modernize/check.py'),'--baseline',str(manifest),'--original',str(self.original),'--edited',str(self.edited),'--out',str(output)]
        good=subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(good.returncode,0,good.stdout+good.stderr)
        self.change(b'exec "hello"',b'exec "quit"')
        bad=subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(bad.returncode,1)
        self.assertFalse(json.loads(output.read_text())['passed'])

    def test_cli_never_writes_its_receipt_into_original_or_edited_sources(self):
        manifest=Path(self.temp.name)/'manifest.json';manifest.write_text(json.dumps(self.manifest))
        forbidden=self.original/'ui/inventory.json'
        cmd=[sys.executable,str(ROOT/'tools/ui_modernize/inventory.py'),'--source',str(self.original),'--out',str(forbidden)]
        self.assertEqual(subprocess.run(cmd,capture_output=True).returncode,2)
        self.assertFalse(forbidden.exists())
        for folder in (self.original,self.edited):
            forbidden=folder/'ui/check.json'
            cmd=[sys.executable,str(ROOT/'tools/ui_modernize/check.py'),'--baseline',str(manifest),
                 '--original',str(self.original),'--edited',str(self.edited),'--out',str(forbidden)]
            self.assertEqual(subprocess.run(cmd,capture_output=True).returncode,2)
            self.assertFalse(forbidden.exists())


if __name__ == '__main__':
    unittest.main()
