"""Actual original + current Game settings: semantics, reachability and fit."""
from pathlib import Path
import re
import struct
import unittest

import layout_settings as layout
import restyle
import semantic as sem

ROOT=Path(__file__).resolve().parents[2]


def scenarios():
    for source in ('pak_original_tce','src/assets'):
        for name in sorted(layout.PAGES):
            path=ROOT/source/'ui'/name
            if path.exists():yield path,Path('ui')/name


def semantic_context():
    files=sem.source_files(ROOT/'pak_original_tce')
    units=[sem.parse(path,data) for path,data in files.items() if Path(path).suffix in sem.SEMANTIC_SUFFIXES]
    _,contracts=sem.signatures(units)
    names=set()
    for unit in units:
        for ts in [unit.body]+[d.body for d in unit.defines]:
            names.update(ts[i+1] for i,t in enumerate(ts[:-1]) if t.lower()=='name')
    return contracts,names


def font_width(text,scale,face='Barlow-Regular.ttf'):
    # Real local TTF advance widths in the native 48*scale glyph-scale space.
    # This is a metrics-based geometry check, not a rasterizer/screenshots test.
    data=(ROOT/'ui-modern/assets/fonts/tce2_ui'/face).read_bytes()
    u16=lambda at:struct.unpack_from('>H',data,at)[0]
    u32=lambda at:struct.unpack_from('>I',data,at)[0]
    tables={data[12+i*16:16+i*16]:u32(12+i*16+8) for i in range(u16(4))}
    base=tables[b'cmap'];upm=u16(tables[b'head']+18);metrics=u16(tables[b'hhea']+34)
    def glyph(code):
        for index in range(u16(base+2)):
            record=base+4+index*8;platform=u16(record);table=base+u32(record+4)
            if platform not in (0,3) or u16(table)!=4:continue
            count=u16(table+6)//2;ends=table+14;starts=ends+count*2+2;deltas=starts+count*2;ranges=deltas+count*2
            for segment in range(count):
                end,start=u16(ends+segment*2),u16(starts+segment*2)
                if start<=code<=end:
                    delta,offset=u16(deltas+segment*2),u16(ranges+segment*2)
                    if not offset:return (code+delta)&65535
                    value=u16(ranges+segment*2+offset+2*(code-start))
                    return ((value+delta)&65535) if value else 0
        raise AssertionError('Missing actual font glyph '+repr(chr(code)))
    return sum(u16(tables[b'hmtx']+min(glyph(ord(ch)),metrics-1)*4) for ch in text)*48*scale/upm


class SettingsLayout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.contracts,cls.names=semantic_context()

    def test_each_real_variant_retains_full_semantic_contract(self):
        for path,relative in scenarios():
            with self.subTest(path=path):
                source=restyle.read(path)
                result=layout.apply(restyle.visual(source),relative)
                # The appearance route is the only intentional new function.
                # Compare the complete original menu after removing exactly
                # that separately verified navigation item, never old controls.
                if relative.name=='options.menu':
                    for start,end in reversed(list(restyle.blocks(result))):
                        if 'name "modern_appearance_link"' in result[start:end]:
                            result=result[:start]+result[end:]
                before,_=sem.signatures([sem.parse(relative.as_posix(),source.encode('latin1'))],self.contracts)
                after,_=sem.signatures([sem.parse(relative.as_posix(),result.encode('latin1'))],self.contracts,self.names)
                self.assertEqual(before,after)
                self.assertNotEqual(source,result)
                if relative.name!='options.menu':
                    self.assertEqual(layout.apply(result,relative),result)

    def test_main_other_menus_and_unowned_directories_are_byte_identical(self):
        for relative in ('ui/main.menu','ui/playonline.menu','ui_obj/main.menu','elsewhere/options.menu'):
            source=b'keep every byte\r\n'.decode('latin1')
            self.assertEqual(layout.apply(source,Path(relative)),source)

    def test_every_actual_control_and_footer_has_nonoverlapping_click_rect(self):
        for path,relative in scenarios():
            result=layout.apply(restyle.visual(restyle.read(path)),relative)
            calls=list(restyle.macro_calls(result,layout.ROWS|{'BUTTON'}))
            original=list(restyle.macro_calls(restyle.read(path),layout.ROWS|{'BUTTON'}))
            self.assertEqual(len(calls),len(original))
            rectangles=[]
            for _,_,name,args in calls:
                x,y,w,h=map(float,args[:4]);label=args[4]
                self.assertGreaterEqual(x,24);self.assertLessEqual(x+w,828)
                self.assertGreaterEqual(y,60 if name!='BUTTON' else 0)
                self.assertLessEqual(y+h,470)
                if name in layout.ROWS:self.assertLessEqual(y+h,432)
                # #ifdef FUI chooses exactly one of the two original BACKs.
                rect=(x,y,w,h)
                if name=='BUTTON' and label.lower()=='"back"' and rect in [r for _,r in rectangles]:continue
                for other,prior in rectangles:
                    px,py,pw,ph=prior
                    overlap=x<px+pw and px<x+w and y<py+ph and py<y+h
                    self.assertFalse(overlap,(str(path),label,other,rect,prior))
                rectangles.append((label,rect))

    def test_true_body_font_labels_and_all_visible_multi_choices_fit(self):
        for path,relative in scenarios():
            result=layout.apply(restyle.visual(restyle.read(path)),relative)
            for _,_,name,args in restyle.macro_calls(result,layout.ROWS):
                width=float(args[2]);scale=float(args[5])
                self.assertGreaterEqual(scale,.22)
                self.assertLessEqual(font_width(args[4].strip('"'),scale),width/2-5,(path,args[4]))
                if name in ('MULTI','MULTIACTION'):
                    choices=re.findall(r'"([^"\n]*)"',args[8])
                    if 'cvarStrList' in args[8]:choices=choices[::2] # Values are not displayed labels.
                    for choice in choices:
                        self.assertLessEqual(font_width(choice,scale),width/2-5,(path,choice))

    def test_controls_game_extensions_remain_visible_and_bindable(self):
        path=ROOT/'src/assets/ui/options_controls.menu'
        result=layout.apply(restyle.visual(restyle.read(path)),Path('ui/options_controls.menu'))
        bindings={args[7]:args for _,_,name,args in restyle.macro_calls(result,{'BIND'})}
        for name in ('+wstats','dropobj','commandmap','+mapexpand','mapzoomin','mapzoomout','+freelook'):
            self.assertIn('"'+name+'"',bindings)
        self.assertEqual(len([1 for _,_,n,a in restyle.macro_calls(result,layout.ROWS)]),58)

    def test_name_options_bind_real_cgame_cvars_in_both_variants(self):
        game=restyle.menu(restyle.read(ROOT/'src/assets/ui/options_customise_hud.menu'),Path('ui/options_customise_hud.menu'))
        editor=restyle.read(ROOT/'ui-modern/ui/options_customise_hud.menu')
        cgame=(ROOT/'src/cgame/cg_main.c').read_text()
        ui=(ROOT/'src/ui/ui_main.c').read_text()
        for cvar,default,variable in (('cg_drawTeamNames','1','ui_drawOverheadNames'),
                                      ('cg_drawCrosshairNames','0','ui_drawTeamNames')):
            self.assertRegex(cgame,r'\{\s*&'+cvar+r'\s*,\s*"'+cvar+r'"\s*,\s*"'+default+r'"\s*,\s*CVAR_ARCHIVE\s*\}')
            self.assertRegex(ui,r'\{\s*&'+variable+r'\s*,\s*"'+cvar+r'"\s*,\s*"'+default+r'"\s*,\s*CVAR_ARCHIVE\s*\}')
        for variant,text in (('game',game),('editor',editor)):
            with self.subTest(variant=variant):
                controls={}
                for _,_,name,args in restyle.macro_calls(text,layout.ROWS):
                    cvar=args[7].split()[0].strip('"')
                    self.assertNotIn(cvar,controls)
                    controls[cvar]=(name,args)
                kind,team=controls['cg_drawTeamNames']
                self.assertEqual(kind,'MULTI')
                self.assertEqual(team[4],'"Team Names:"')
                self.assertEqual(sem.tokens(team[8]),sem.tokens('cvarFloatList { "Off" 0 "Team" 1 "Fireteam" 2 }'))
                kind,crosshair=controls['cg_drawCrosshairNames']
                self.assertEqual(kind,'YESNO')
                self.assertEqual(crosshair[4],'"Crosshair Names:"')
                for args in (team,crosshair):
                    self.assertLessEqual(font_width(args[4].strip('"'),float(args[5])),float(args[2])/2-5)

    def test_extended_view_cards_and_rows_fit_before_footer(self):
        game=restyle.menu(restyle.read(ROOT/'src/assets/ui/options_customise_hud.menu'),Path('ui/options_customise_hud.menu'))
        editor=restyle.read(ROOT/'ui-modern/ui/options_customise_hud.menu')
        for variant,text,count,bottom in (('game',game,11,412),('editor',editor,8,322)):
            with self.subTest(variant=variant):
                groups=layout._sections(list(restyle.macro_calls(text,layout.CALLS)))
                hud=groups['hud display'];self.assertEqual(len(hud['rows']),count)
                x,y,w,h=map(float,hud['header'][3][:4])
                self.assertEqual(y+h,bottom)
                self.assertLessEqual(y+h,432)
                for index,(_,_,_,args) in enumerate(hud['rows']):
                    rx,ry,rw,rh=map(float,args[:4])
                    self.assertEqual(ry,78+30*index)
                    self.assertGreaterEqual(rx,x);self.assertLessEqual(rx+rw,x+w)
                    self.assertLessEqual(ry+rh,y+h)

    def test_editor_name_additions_have_an_exact_independent_semantic_contract(self):
        key='ui/options_customise_hud.menu'
        original=(ROOT/'pak_original_tce'/key).read_bytes()
        data=(ROOT/'ui-modern'/key).read_bytes()
        def matches(candidate):
            widgets=(ROOT/'pak_original_tce'/sem.ORIGINAL_WIDGETS).read_bytes()
            reference=sem.layout_reference({key:original,sem.ORIGINAL_WIDGETS:widgets},
                                           {key:candidate,sem.PRIVATE_WIDGETS:widgets})
            before_units=[sem.parse(path,content) for path,content in reference.items()]
            before,_=sem.signatures(before_units,self.contracts)
            actual=sem.normalize_layout([sem.parse(key,candidate),sem.parse(sem.PRIVATE_WIDGETS,widgets)],before_units,self.contracts)
            after,_=sem.signatures(actual,self.contracts,self.names)
            return before[key]==after[key]
        self.assertTrue(matches(data))
        for old,new in ((b'"Fireteam" 2',b'"Fireteam" 1'),
                        (b'"Crosshair Names:"',b'"Other Names:"'),
                        (b'"cg_drawCrosshairNames"',b'"cg_drawCrosshairPickups"'),
                        (b'"Show the name of the player under your crosshair"',b'"Different behavior"'),
                        (b'"cg_drawFPS"',b'"cg_drawSnapshot"')):
            with self.subTest(old=old):
                self.assertIn(old,data);self.assertFalse(matches(data.replace(old,new,1)))

    def test_unexpected_new_setting_cannot_silently_overflow_or_disappear(self):
        source=restyle.read(ROOT/'src/assets/ui/options_controls.menu')
        duplicate='BIND( 8, 180, 200, 10, "Unexpected:", .2, 8, "+unexpected", "" )\n'
        source=source.replace('// Input //',duplicate*10+'// Input //')
        with self.assertRaisesRegex(ValueError,'exceeds safe footer'):
            layout.apply(source,Path('ui/options_controls.menu'))
        system=restyle.read(ROOT/'pak_original_tce/ui/options_system.menu').replace('"Resolution:"','"Unexpected Resolution:"')
        with self.assertRaisesRegex(ValueError,'resolution row'):
            layout.apply(system,Path('ui/options_system.menu'))
        hud=restyle.read(ROOT/'src/assets/ui/options_customise_hud.menu')
        hud=hud.replace('// In-Game Display //',
                        'YESNO( 8, 180, 384, 10, "Extra:", .2, 8, "cg_drawFPS", "" )\n'*2+'// In-Game Display //')
        with self.assertRaisesRegex(ValueError,'HUD settings section exceeds safe footer'):
            layout.apply(hud,Path('ui/options_customise_hud.menu'))


if __name__=='__main__':unittest.main()
