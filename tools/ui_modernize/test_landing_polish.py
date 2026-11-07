"""Header overlays, full original actions and real native text-fit contracts."""
from pathlib import Path
import re
import unittest

import layout_common
import layout_browse
import layout_settings
import restyle
import semantic as sem
from test_layout_settings import font_width
from test_widget_continuations import physical_macros


ROOT=Path(__file__).resolve().parents[2]


class LandingPolishTests(unittest.TestCase):
    def test_all_main_publisher_logos_fit_footer_after_native_parent_offset(self):
        names={'groovesix','teamterminator','atvi_logo','id_logo','sd_logo'}
        for directory in ('ui','ui_obj','ui_robj','ui_tdm'):
            path=Path(directory)/'main.menu'
            result=restyle.menu(restyle.read(ROOT/'pak_original_tce'/path),path)
            origin=[float(re.search(r'^#define '+key+r' ([\d.]+)$',result,re.M)[1])
                    for key in ('WINDOW_X','WINDOW_Y')]
            seen=set()
            for start,end in restyle.blocks(result):
                block=sem.strip_comments(result[start:end])
                match=re.search(r'\bname\s+"?(groovesix|teamterminator|atvi_logo|id_logo|sd_logo)\b',block)
                if not match:continue
                x,y,w,h=map(float,re.search(r'\brect\s+([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',block).groups())
                seen.add(match[1]);x+=origin[0];y+=origin[1]
                self.assertGreaterEqual(x,64);self.assertLessEqual(x+w,548)
                self.assertGreaterEqual(y,440);self.assertLessEqual(y+h,468)
            self.assertEqual(seen,names)

    def test_publisher_logos_use_only_small_alpha_shadows_without_footer_plate(self):
        shader=(ROOT/'ui-modern/assets/scripts/tce2_ui_footer.shader').read_text()
        self.assertNotIn('tce2_ui/logo_plate',shader)
        self.assertNotIn('$whiteimage',shader)
        self.assertNotIn('GL_ONE GL_ZERO',shader)
        self.assertNotIn('rgbGen vertex',shader);self.assertNotIn('alphaGen vertex',shader)
        declared=re.findall(r'^tce2_ui/([^\s]+)\s*$',shader,re.M)
        self.assertEqual(set(declared),{'publisher_'+name+suffix for name in ('atvi','id','sd') for suffix in ('','_shadow')})
        for short in ('atvi','id','sd'):
            for suffix in ('','_shadow'):
                name='tce2_ui/publisher_'+short+suffix
                body=re.search(r'^'+re.escape(name)+r'\s*\n(.*?)(?=\ntce2_ui/|\Z)',shader,re.M|re.S)[1]
                self.assertIn('clampmap ui/assets/logo_'+short+'.tga',body)
                self.assertIn('blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA',body)
                if suffix:
                    self.assertIn('rgbGen const ( 0 0 0 )',body)
                    self.assertIn('alphaGen const 0.8',body)
                else:
                    self.assertIn('rgbGen identity',body);self.assertIn('alphaGen identity',body)
                    self.assertNotIn('alphaGen const',body)
        for directory in ('ui','ui_obj','ui_robj','ui_tdm'):
            path=Path(directory)/'main.menu'
            source=restyle.read(ROOT/'pak_original_tce'/path);result=restyle.menu(source,path)
            self.assertNotIn('modern_footer_plate',result)
            self.assertNotIn('tce2_ui/logo_plate',result)
            self.assertEqual((ROOT/'ui-modern'/path).read_bytes(),result.encode('latin1'))
            source_paint=sem.strip_comments(source);result_paint=sem.strip_comments(result)
            item_blocks=[result[s:e] for s,e in restyle.blocks(result)]
            for short in ('atvi','id','sd'):
                self.assertEqual(result_paint.count('"tce2_ui/publisher_'+short+'"'),source_paint.count('ui/assets/logo_'+short+'_dark'))
                self.assertNotIn('ui/assets/logo_'+short+'_dark',result_paint)
                logo=next(block for block in item_blocks if re.search(r'\bname\s+"'+short+'_logo"',block))
                shadow=next(block for block in item_blocks if 'name "modern_publisher_shadow_'+short+'"' in block)
                self.assertIn('background "tce2_ui/publisher_'+short+'_shadow"',shadow)
                self.assertIn('decoration',shadow)
                self.assertNotRegex(shadow,r'\b(?:action|cvar|ownerdraw|type)\b')
                rect=lambda block:list(map(float,re.search(r'\brect\s+([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',block).groups()))
                x,y,w,h=rect(logo)
                self.assertEqual(rect(shadow),[x+1,y+1,w,h])
                self.assertLess(result.index(shadow),result.index(logo))
                self.assertLessEqual(w*h,1600) # Exact mark bounds, never a footer-wide surface.
            for resource in ('groovesixstudios.tga','ui/assets/logo_tt_dark.tga'):
                self.assertEqual(result.count(resource),source.count(resource))

    def test_only_two_proven_dangling_registry_imports_are_removed(self):
        self.assertEqual(restyle.DEAD_MENU_LOADS,sem.DEAD_MENU_LOADS)
        for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt'):
            source=restyle.read(ROOT/'pak_original_tce/ui'/name)
            result=restyle.prune_dead_registry(source)
            before=re.findall(r'loadMenu\s*\{\s*"([^"]+)"\s*\}',sem.strip_comments(source))
            after=re.findall(r'loadMenu\s*\{\s*"([^"]+)"\s*\}',sem.strip_comments(result))
            self.assertEqual(after,[path for path in before if path not in restyle.DEAD_MENU_LOADS])
            self.assertEqual(len(before)-len(after),2)
            for dead in restyle.DEAD_MENU_LOADS:
                self.assertNotIn(dead,result)
        # The exemption does not turn into a general permission to delete UI.
        fixture=['loadMenu','{','"ui/options_controls.menu"','}']
        self.assertEqual(sem._dead_menu_loads(fixture),fixture)

    def test_all_three_hubs_leave_map_visible_and_keep_original_actions(self):
        for filename in ('ingame_main.menu','ingame_vote.menu','options.menu'):
            sources=[ROOT/'pak_original_tce/ui'/filename]
            if (ROOT/'src/assets/ui'/filename).exists():sources.append(ROOT/'src/assets/ui'/filename)
            for path in sources:
                with self.subTest(path=path):
                    source=restyle.read(path)
                    result=restyle.menu(source,Path('ui')/filename)
                    self.assertRegex(result,r'#define WINDOW_HEIGHT 104\b')
                    background=next(result[s:e] for s,e in restyle.blocks(result)
                                    if re.search(r'\bname\s+"background"',result[s:e]))
                    self.assertRegex(background,r'\brect\s+0 0 852 104\b')
                    self.assertRegex(background,r'\bbackcolor\s+0 0 0 0\b')
                    before=list(restyle.macro_calls(source,{'BUTTON','BUTTONEXT'}))
                    after=list(restyle.macro_calls(result,{'BUTTON','BUTTONEXT'}))
                    self.assertEqual(len(before),len(after))
                    self.assertEqual([a[4:] for _,_,_,a in before],
                                     [a[4:5]+old[5:7]+a[7:] for (_,_,_,a),(_,_,_,old) in zip(after,before)])
                    rectangles={}
                    for _,_,_,args in after:
                        x,y,w,h=map(float,args[:4]);label=args[4].strip('"')
                        self.assertEqual(y,56);self.assertEqual(h,32)
                        self.assertGreaterEqual(x,24);self.assertLessEqual(x+w,828)
                        self.assertLessEqual(y+h,104-12)
                        self.assertLessEqual(font_width(label,float(args[5])),w-12)
                        rect=(x,y,w,h)
                        # Vote/Referee/RCON and FUI/ingame BACK alternatives
                        # are mutually exclusive states of the same native slot.
                        if rect in rectangles:
                            self.assertTrue(label in ('Vote','Referee','RCON','MISC','MAP','PLAYERS','BACK'))
                        rectangles[rect]=label
                    self.assertNotIn('modern_options_icon_',result)
                    self.assertNotIn('modern_pause_',result)

    def test_appearance_navigation_occupies_reserved_header_slot_only(self):
        result=restyle.menu(restyle.read(ROOT/'pak_original_tce/ui/options.menu'),Path('ui/options.menu'))
        items=[result[s:e] for s,e in restyle.blocks(result)
               if 'name "modern_appearance_link"' in result[s:e]]
        self.assertEqual(len(items),1)
        item=items[0]
        for literal in ('rect 568 56 124 32','text "APPEARANCE"','type ITEM_TYPE_BUTTON',
                        'action { close options ; open modern_appearance ; }'):
            self.assertIn(literal,item)
        self.assertNotRegex(item,r'\b(?:cvar|feeder|ownerdraw)\b')
        for event in ('mouseEnter','mouseExit','onFocus','leaveFocus'):
            self.assertIn(event+' { setitemcolor "modern_appearance_link"',item)
        self.assertEqual(layout_settings.apply(result,Path('ui/options.menu')),result)

    def test_advanced_long_labels_and_checkbox_marks_fit_their_actual_rows(self):
        path=Path('ui/hostgame_advanced.menu')
        result=layout_browse.apply(restyle.visual(restyle.read(ROOT/'pak_original_tce'/path)),path)
        controls={'NUMERICFIELD','YESNO','MULTI','EDITFIELD','CHECKBOXALIGNX'}
        count=0
        for _,_,name,args in restyle.macro_calls(result,controls):
            x,y,w,h=map(float,args[:4]);scale=float(args[5]);count+=1
            self.assertGreaterEqual(scale,.20)
            self.assertGreaterEqual(x,24);self.assertLessEqual(x+w,828)
            self.assertGreaterEqual(y,60);self.assertLessEqual(y+h,432)
            anchor=w/2+(float(args[6]) if name=='CHECKBOXALIGNX' else 0)
            self.assertLessEqual(font_width(args[4].strip('"'),scale),anchor)
            if name=='CHECKBOXALIGNX':self.assertLessEqual(anchor+8+h,w)
            if name=='MULTI':
                choices=re.findall(r'"([^"\n]*)"',args[8])
                if 'cvarStrList' in args[8]:choices=choices[::2]
                for choice in choices:self.assertLessEqual(font_width(choice,scale),w/2-6)
        self.assertGreater(count,40)

    def test_ten_entry_quickmessage_panel_contains_every_real_entry(self):
        for filename in ('wm_quickmessage.menu','wm_quickmessageAlt.menu','wm_ftquickmessage.menu'):
            result=restyle.menu(restyle.read(ROOT/'pak_original_tce/ui'/filename),Path('ui')/filename)
            self.assertIn('0 0 300 284',result)
            positions=[int(a[3]) for _,_,_,a in restyle.macro_calls(result,{'QM_MENU_ITEM','QM_MENU_ITEM_TEAM'})
                       if a[3].isdigit()]
            self.assertTrue(positions)
            for pos in positions:self.assertLessEqual(52+22*pos+20,284-12)

    def test_legacy_fade_boxes_no_longer_extend_beyond_the_menu_canvas(self):
        count=0
        for source in (ROOT/'pak_original_tce/ui').glob('*.menu'):
            if source.name in ('main.menu','global.menu'):continue
            result=restyle.menu(restyle.read(source),Path('ui')/source.name)
            for start,end in restyle.blocks(result):
                block=result[start:end]
                if not re.search(r'\bname\s+"background"',block):continue
                height=104 if source.name in ('ingame_main.menu','ingame_vote.menu','options.menu') else 480
                self.assertRegex(block,rf'\brect\s+0 0 852 {height}\b')
                count+=1
        self.assertGreater(count,40)

    def test_private_header_title_rules_are_solid_without_sparse_texture(self):
        original=restyle.read(ROOT/'pak_original_tce/ui/menumacros.h')
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
