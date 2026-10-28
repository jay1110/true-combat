"""Rebuild the optional UI theme from immutable original menu sources."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'pak'
DEST = ROOT / 'ui-modern'
DIRS = ('ui', 'ui_obj', 'ui_robj', 'ui_tdm')


def read(path):
    # Preserve all original byte-valued labels, including legacy German text.
    return path.read_bytes().decode('latin1').replace('\r', '')


def visual(text):
    """Only change presentation tokens, never commands, labels or cvar values."""
    colors = {
        '.45 .4 .3 1': '.94 .75 .27 1',
        '.19 .19 .19 1': '.23 .29 .35 .75',
        '.09 .09 .09 .7': '.10 .14 .19 .96',
        '.13 .16 .22 .7': '.19 .26 .33 1',
        '.22 .16 .13 .7': '.19 .26 .33 1',
        '.1 .1 .1 .95': '.055 .075 .10 .98',
        '.0 .20 .40 .6': '.94 .75 .27 .9',
        '.5 .5 .5 .2': '.12 .17 .22 .96',
        '.6 .6 .6 1': '.84 .88 .92 1',
        '.9 .9 .9 1': '.98 .98 .96 1',
    }
    for before, after in colors.items():
        text = re.sub(r'(?<![\w.])' + re.escape(before).replace(r'\ ', r'\s+') + r'(?![\w.])', after, text)
    text = text.replace('ITEM_TEXTSTYLE_SHADOWED', 'ITEM_TEXTSTYLE_NORMAL')
    return text


def blocks(text):
    """Find item blocks while ignoring quoted strings and both comment forms."""
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:[^"\\]|\\.)*"|\bitemDef\b|[{}]', re.S | re.I)
    start = None
    depth = 0
    for m in token.finditer(text):
        v = m.group()
        if v.lower() == 'itemdef' and start is None:
            start = m.start()
            depth = 0
        elif start is not None:
            if v == '{': depth += 1
            if v == '}':
                depth -= 1
                if depth == 0:
                    yield start, m.end()
                    start = None


def item_prop(block, key, value):
    pattern = r'(?im)^(\s*' + key + r'\s+)[^\n\\]*(\\[^\n]*)?$'
    if re.search(pattern, block):
        return re.sub(pattern, lambda m: m[1] + value + (' ' + m[2] if m[2] else ''), block)
    # Insert before the final brace, keeping original behavior fields intact.
    at = block.rfind('}')
    return block[:at] + '\t' + key + '\t' + value + '\n\t' + block[at:]


def macro_calls(text, names):
    # Return complete macro calls including nested conditional-script braces.
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:[^"\\]|\\.)*"|[A-Za-z_]\w*|[(),]', re.S)
    tokens = list(token.finditer(text))
    i = 0
    while i < len(tokens):
        m = tokens[i]
        if m.group() in names and i + 1 < len(tokens) and tokens[i+1].group() == '(':
            depth = 1; parts = []; begin = tokens[i+1].end(); j = i + 2
            while j < len(tokens):
                v = tokens[j].group()
                if v == '(': depth += 1
                elif v == ')':
                    depth -= 1
                    if depth == 0:
                        parts.append(text[begin:tokens[j].start()].strip())
                        yield m.start(), tokens[j].end(), m.group(), parts
                        i = j; break
                elif v == ',' and depth == 1:
                    parts.append(text[begin:tokens[j].start()].strip()); begin = tokens[j].end()
                j += 1
        i += 1


def main_layout(text, path):
    # Large left navigation, wide scene, original eight actions and mode label.
    compact = path.parent.name == 'ui'
    # Real BarlowCondensed .30 captions max70.41units. Native text offset40
    # plus16right padding fits128px buttons inside a160px rail with16px sides.
    dimensions = ('48','92','160','320') if compact else ('0','0','852','480')
    for key, value in zip(('WINDOW_X','WINDOW_Y','WINDOW_WIDTH','WINDOW_HEIGHT'), dimensions):
        text = re.sub(r'(?m)^#define\s+'+key+r'\s+[^\n]+', '#define '+key+' '+value, text)
    calls = list(macro_calls(text, {'BUTTONTEXTONLY', 'BUTTON', 'BIGLABEL', 'LABELWHITE'}))
    for start, end, name, args in reversed(calls):
        if name in ('BUTTONTEXTONLY', 'BUTTON'):
            # Calculate index in source order, not reversed replacement order.
            index = sum(1 for a,b,n,c in calls if a < start and n in ('BUTTONTEXTONLY','BUTTON'))
            args[:4] = ['16' if compact else '64', str((38 if compact else 130)+index*32), '128', '27']
            args[5:7] = ['.30', '19']
        elif name == 'BIGLABEL':
            # BIGLABEL applies X both to the item and to its text offset.
            args[:4] = ['32', '80', '600', '32']; args[5:7] = ['.60', '25']
        elif name == 'LABELWHITE':
            args[:4] = ['16' if compact else '64', '280' if compact else '404', '330', '16']; args[5:] = ['.20','ITEM_ALIGN_LEFT','0','12']
        text = text[:start] + name+'( '+', '.join(args)+' )' + text[end:]
    # Preserve each studio/publisher logo; align them in a quiet footer strip.
    for start, end in reversed(list(blocks(text))):
        block = text[start:end]
        match = re.search(r'(?im)^\s*name\s+"?(groovesix|teamterminator|atvi_logo|id_logo|sd_logo)\b',block)
        if match:
            names = ['groovesix','teamterminator','atvi_logo','id_logo','sd_logo']
            x = 64 + names.index(match[1].lower())*100
            # Original logo aspect ratios, fitted into an 80 x 28 footer cell.
            ratio = {'groovesix':2, 'teamterminator':164/48,
                     'atvi_logo':4, 'id_logo':1, 'sd_logo':1}[match[1].lower()]
            w, h = min(80, 28*ratio), min(28, 80/ratio)
            block = item_prop(block, 'rect', f'{x+(80-w)/2:g} {440+(28-h)/2:g} {w:g} {h:g}')
        elif re.search(r'(?im)^\s*name\s+ELITELOGO\b', block):
            block = item_prop(block, 'rect', '612 34 176 88')
        text = text[:start] + block + text[end:]
    # Insert decoration before original content so it cannot cover controls.
    at = text.index('// Company Logos //') if '// Company Logos //' in text else text.index('// Background Logo //')
    if not compact:
        decoration = '''itemDef {
        name "modern_brand"
        rect 64 20 192 48
        style WINDOW_STYLE_SHADER
        background "ui/assets/tce_logo2.tga"
        forecolor 1 1 1 1
        visible 1
        decoration
    }
    itemDef {
        name "modern_navigation_panel"
        rect 48 116 160 280
        style WINDOW_STYLE_SHADER
        background "tce2_ui/panel"
        forecolor 1 1 1 1
        visible 1
        decoration
    }
'''
        # These images decorate the existing buttons and do not intercept input.
        for index, icon in enumerate(('multiplayer','host','folder','replays','settings','profile','credits','exit')):
            decoration += f'''itemDef {{
        name "modern_navigation_icon_{index}"
        rect 76 {136 + index*32} 15 15
        style WINDOW_STYLE_SHADER
        background "tce2_ui/icons/{'info' if icon == 'credits' else icon}"
        forecolor .94 .75 .27 .9
        visible 1
        decoration
    }}
'''
        text = text[:at] + decoration + text[at:]
    return text


def macros(text):
    text = visual(text)
    # Ingame dialogs use solid readable cards rather than black glass.
    for start, end in reversed(list(blocks(text))):
        block=text[start:end]
        if re.search(r'\bname\s+"window"',block):
            block=item_prop(block,'backcolor','.035 .055 .075 .97')
            block=item_prop(block,'bordercolor','.23 .29 .35 .75')
        elif re.search(r'\bname\s+"windowtitle"',block):
            block=item_prop(block,'textaligny','21')
        text=text[:start]+block+text[end:]
    # Consistent large title without the low-resolution gunmen ornament.
    text = text.replace('10 -30 60 60', '0 4 3 24')
    text = text.replace('"ui/assets/tce_gunmen256"', '"tce2_ui/focus"')
    text = text.replace('64 4 $evalint(WINDOW_WIDTH-4) 24', '16 4 $evalint(WINDOW_WIDTH-20) 24')
    text = text.replace('textscale\t.55', 'textscale\t.40')
    # A modern title and panel with the exact old item names/actions.
    # Button background textures are white-modulated; focus still uses colors.
    text = re.sub(r'(backcolor\s+)\.055 \.075 \.10 \.98',r'\g<1>.055 .075 .10 .97',text)
    # Legacy button macros ignore their declared scale argument. Use it now.
    for name in ('BUTTON','BUTTONEXT','NAMEDBUTTON','NAMEDBUTTONEXT','BUTTONTEXTONLY'):
        pat = r'(#define '+name+r'\([^\n]+\n)(.*?)(?=\n#define |\Z)'
        def patch(m):
            body=m[2]
            parameter = 'BUTTON' if name == 'BUTTONTEXTONLY' else name
            body=re.sub(r'(textscale\s+)\.25',r'\g<1>'+parameter+'_TEXT_SCALE',body)
            if name == 'BUTTONTEXTONLY':
                body=body.replace('textalign ITEM_ALIGN_CENTER','textalign ITEM_ALIGN_LEFT')
                body=re.sub(r'textalign\s+ITEM_ALIGN_CENTER','textalign ITEM_ALIGN_LEFT',body)
                body=re.sub(r'textalignx\s+\$evalfloat\(0\.5\*\(BUTTON_W\)\)', 'textalignx 40',body)
            return m[1]+body
        text=re.sub(pat,patch,text,flags=re.S)
    return text


def menu(text, relative):
    original = text
    text = visual(text)
    # Modernize all hand-written list/edit/dialog styles as well as macro users.
    for start, end in reversed(list(blocks(text))):
        block = text[start:end]
        name = re.search(r'(?im)^\s*name\s+"?([^"\s]+)',block)
        if name and name[1].lower() == 'background':
            block = re.sub(r'(backcolor\s+)0 0 0 (?:\.55|0\.55)',r'\g<1>.015 .025 .04 .72',block)
        if re.search(r'\bITEM_TYPE_LISTBOX\b',block):
            block = item_prop(block,'backcolor','.055 .075 .10 .94')
            block = item_prop(block,'bordercolor','.23 .29 .35 .75')
            block = item_prop(block,'forecolor','.88 .91 .94 1')
            # Native list selection is painted AFTER text: keep it translucent.
            block = re.sub(r'(outlinecolor\s+)\.12 \.17 \.22 \.96',
                           r'\g<1>.94 .75 .27 .18',block)
        text=text[:start]+block+text[end:]
    if relative.name == 'main.menu': text=main_layout(text,relative)
    if relative.name == 'global.menu':
        text=text.replace('"ui/assets/3_cursor3"','"tce2_ui/cursor"')
        text=text.replace('"ariblk" 16','"tce2_ui/BarlowCondensed-SemiBold" 16')
        text=text.replace('"ariblk" 27','"tce2_ui/BarlowCondensed-SemiBold" 27')
        text=text.replace('"courbd" 21','"tce2_ui/Barlow-Regular" 21')
        text=text.replace('"courbd" 30','"tce2_ui/Barlow-SemiBold" 30')
        for start,end in reversed(list(blocks(text))):
            b=text[start:end]
            if re.search(r'(?im)^\s*name\s+ELITELOGO\b',b):
                b=item_prop(b,'rect','0 0 852 480');b=item_prop(b,'background','"tce2_ui/background"')
                b=item_prop(b,'forecolor','1 1 1 1')
            text=text[:start]+b+text[end:]
        # Main background container must cover the same 852x480 design space.
        text=re.sub(r'(?m)^(\s*rect\s+)0 0 640 480',r'\g<1>0 0 852 480',text)
    # A few pure visibility/voice helper menus have no explicit palette at all.
    # Add only an unobtrusive noninteractive accent decoration to those files.
    if text==original:
        at=text.rfind('}')
        if at>=0:
            text=text[:at]+'''    itemDef { name "modern_accent" rect 0 0 2 16 style WINDOW_STYLE_FILLED backcolor .94 .75 .27 .8 visible 1 decoration }
'''+text[at:]
    # A separate widget sheet keeps the approved main menu and its dependencies
    # byte-identical while the rest of the interface gets desktop layouts.
    if relative.name not in ('main.menu','global.menu'):
        import layout_settings, layout_browse, layout_misc, layout_secondary
        from layout_common import settings_navigation, vote_navigation
        text=layout_settings.apply(text,relative)
        text=layout_browse.apply(text,relative)
        text=layout_misc.apply(text,relative)
        text=layout_secondary.apply(text,relative)
        text=settings_navigation(text,relative)
        text=vote_navigation(text,relative)
        # Legacy copies used negative 862x650 fade boxes to bleed across a
        # 640-wide parent.  All modern pages use one852x480 canvas; retain the
        # same decoration/script identity without an off-screen painted rect.
        from layout_common import HEADER_HEIGHT
        header=relative.name in ('ingame_main.menu','ingame_vote.menu','options.menu')
        for start,end in reversed(list(blocks(text))):
            b=text[start:end]
            if re.search(r'\bname\s+"background"',b):
                b=item_prop(b,'rect',f'0 0 852 {HEADER_HEIGHT if header else 480}')
                text=text[:start]+b+text[end:]
        text=text.replace('"ui/menumacros.h"','"ui/modern_widgets.h"')
    return text


def main():
    rows=[]
    for folder in DIRS:
        for p in sorted((SOURCE/folder).glob('*')):
            if p.suffix.lower() not in ('.menu','.h','.txt') or not p.is_file():continue
            relative=p.relative_to(SOURCE);s=read(p)
            if p.name=='menumacros.h': s=macros(s)
            elif p.name=='menudef.h':
                # Independent native visual font slots; no item/action constants.
                s=re.sub(r'(?m)^(#define UI_FONT_ARIBLK_16\s+)0',r'\g<1>2',s)
                s=re.sub(r'(?m)^(#define UI_FONT_ARIBLK_27\s+)1',r'\g<1>3',s)
            elif p.suffix.lower()=='.menu':s=menu(s,relative)
            elif p.name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt') and folder=='ui':
                source_line='loadMenu { "ui/options.menu" }'
                if s.count(source_line)!=1:raise ValueError('Missing/duplicate Options registry entry: '+p.name)
                s=s.replace(source_line,source_line+'\n\tloadMenu { "ui/modern_appearance.menu" }',1)
            q=DEST/relative;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(s.encode('latin1'))
            rows.append({'path':relative.as_posix(),'changed':p.read_bytes()!=q.read_bytes()})
    from layout_common import widgets
    (DEST/'ui/modern_widgets.h').write_bytes(widgets(read(DEST/'ui/menumacros.h')).encode('latin1'))
    (DEST/'restyle-coverage.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'files':len(rows),'menus':sum(r['path'].endswith('.menu') for r in rows),'all_menu_sources_edited':all(r['changed'] for r in rows if r['path'].endswith('.menu'))}))


if __name__=='__main__':main()
