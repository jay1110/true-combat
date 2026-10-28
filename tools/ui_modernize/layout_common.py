"""Native desktop menu presentation, isolated from the approved main menu."""
from pathlib import Path
import re

SETTINGS = {
    'game': ('GAME', 'options_customise_game'),
    'view': ('VIEW', 'options_customise_hud'),
    'controls': ('CONTROLS', 'options_controls'),
    'system': ('SYSTEM', 'options_system'),
    'appearance': ('APPEARANCE', 'modern_appearance'),
}

# Navigation hubs are a real overlay: their entire painted/input rectangle is
# limited to the top rail.  The renderer can then show the running map below
# it instead of an almost opaque 852x480 decoration covering the whole view.
HEADER_HEIGHT = 104
HEADER_BUTTON_Y = 56
HEADER_BUTTON_HEIGHT = 32


def set_window(text, x=0, y=0, width=852, height=480):
    for name, value in zip(('WINDOW_X','WINDOW_Y','WINDOW_WIDTH','WINDOW_HEIGHT'),
                           (x,y,width,height)):
        text = re.sub(r'(?m)^\s*#define\s+'+name+r'\s+[^\n]+',
                      '#define '+name+' '+str(value),text)
    return text


def add_items(text, items, menu_name=None):
    """Append only to the requested native menu, respecting quoted scripts."""
    token=re.compile(r'//[^\n]*|/\*.*?\*/|"(?:[^"\\]|\\.)*"|\bmenuDef\b|[{}]',re.S|re.I)
    ranges=[];start=None;depth=0
    for m in token.finditer(text):
        v=m.group()
        if v.lower()=='menudef' and start is None:start=m.start();depth=0
        elif start is not None:
            if v=='{':depth+=1
            elif v=='}':
                depth-=1
                if depth==0:ranges.append((start,m.start(),m.end()));start=None
    chosen=[]
    for start,close,end in ranges:
        match=re.search(r'\bname\s+"?([^"\s{}]+)',text[start:end],re.I)
        if match and (menu_name is None or match[1]==menu_name):chosen.append((start,close,end))
    if not chosen:raise ValueError('No native menu for decorations: '+str(menu_name))
    start,close,end=chosen[-1]
    return text[:close]+'\n'+items+'\n'+text[close:]


def panel(name,x,y,w,h,color='.055 .075 .10 .96'):
    return f'''itemDef {{ name "{name}" rect {x} {y} {w} {h}
        style WINDOW_STYLE_FILLED backcolor {color} visible 1 decoration }}\n'''


def icon(name,shader,x,y,size=26):
    return f'''itemDef {{ name "{name}" rect {x} {y} {size} {size}
        style WINDOW_STYLE_SHADER background "tce2_ui/icons/{shader}"
        forecolor .94 .75 .27 1 visible 1 decoration }}\n'''


def header_canvas(text):
    from restyle import blocks, item_prop
    text = set_window(text, height=HEADER_HEIGHT)
    for start, end in reversed(list(blocks(text))):
        block = text[start:end]
        if re.search(r'\bname\s+"background"', block):
            block = item_prop(block, 'rect', f'0 0 852 {HEADER_HEIGHT}')
            block = item_prop(block, 'backcolor', '0 0 0 0')
        elif re.search(r'\bname\s+"windowtitle"', block):
            # Vote/Referee/RCON have their own privilege-dependent title items.
            block = item_prop(block, 'rect', '24 8 804 36')
            block = item_prop(block, 'textscale', '.40')
            block = item_prop(block, 'textalignx', '0')
            block = item_prop(block, 'textaligny', '27')
            block = item_prop(block, 'textalign', 'ITEM_ALIGN_LEFT')
        text = text[:start] + block + text[end:]
    return text


def header_button(args, slot, width=124, stride=136, scale='.27'):
    result = list(args)
    result[:4] = list(map(str, (24 + slot * stride, HEADER_BUTTON_Y,
                              width, HEADER_BUTTON_HEIGHT)))
    result[5:7] = [scale, '22']
    return result


def appearance_link(text):
    return add_items(text, '''itemDef {
        name "modern_appearance_link"
        rect 568 56 124 32
        text "APPEARANCE"
        textfont UI_FONT_ARIBLK_27
        textscale .27
        textalign ITEM_ALIGN_CENTER textalignx 62 textaligny 22
        type ITEM_TYPE_BUTTON
        style WINDOW_STYLE_FILLED backcolor .10 .14 .19 .96
        forecolor .84 .88 .92 1
        border WINDOW_BORDER_FULL bordercolor .23 .29 .35 .75
        visible 1
        '''+hover_feedback('modern_appearance_link')+'''
        action { close options ; open modern_appearance ; }
    }''', 'options')


def hover_feedback(name):
    """Native mouse and keyboard focus paint, targeting only this button."""
    result=''
    for event,focused in (('mouseEnter',True),('mouseExit',False),
                          ('onFocus',True),('leaveFocus',False)):
        result+=event+' { '
        result+=f'setitemcolor "{name}" backcolor '+('.19 .26 .33 1' if focused else '.08 .12 .16 1')+' ; '
        result+=f'setitemcolor "{name}" forecolor '+('1 1 1 1' if focused else '.84 .89 .94 1')+' ; '
        result+=f'setitemcolor "{name}" bordercolor '+('.94 .75 .27 1' if focused else '.23 .29 .35 .75')+' ; }\n'
    return result


def settings_navigation(text, relative):
    from restyle import macro_calls
    page=Path(relative).stem
    current=next((k for k,v in SETTINGS.items() if v[1]==page),None)
    if current is None:return text
    backs=[a for s,e,n,a in macro_calls(text,{'BUTTON'}) if a[4].strip('"').lower()=='back']
    if len(backs)!=1:raise ValueError('Settings Back contract is ambiguous: '+page)
    exit_action=backs[0][7]
    items=''
    for i,(key,(label,target)) in enumerate(SETTINGS.items()):
        active=key==current
        name='modern_nav_'+key
        behavior=('decoration' if active else 'type ITEM_TYPE_BUTTON\n'+hover_feedback(name)+
                  ' action { '+exit_action+' ; close options ; open '+target+' }')
        items+=f'''itemDef {{
            name "{name}" rect {252+i*116} 12 112 30
            text "{label}" textfont UI_FONT_ARIBLK_27 textscale .25
            textalign ITEM_ALIGN_CENTER textalignx 56 textaligny 21
            style WINDOW_STYLE_FILLED
            border WINDOW_BORDER_FULL bordercolor .23 .29 .35 .75
            backcolor {'.94 .75 .27 1' if active else '.08 .12 .16 1'}
            forecolor {'.025 .035 .05 1' if active else '.84 .89 .94 1'}
            visible 1 {behavior}
        }}\n'''
    return add_items(text,items,page)


VOTE_PAGES={
    'ingame_vote_misc':'misc','ingame_vote_misc_refrcon':'misc',
    'ingame_vote_map':'map','ingame_vote_players':'players',
}


def vote_navigation(text,relative):
    """Persist the original privilege-specific hub routes on each subpage."""
    from restyle import macro_calls, read, ROOT
    page=Path(relative).stem
    current=VOTE_PAGES.get(page)
    if current is None:return text
    backs=[a for _,_,_,a in macro_calls(text,{'BUTTON'}) if a[4].strip('"').lower()=='back']
    if len(backs)!=1:raise ValueError('Vote Back contract is ambiguous: '+page)
    exit_action=backs[0][7].rstrip().rstrip(';')
    original=read(ROOT/'pak/ui/ingame_vote.menu')
    templates=list(macro_calls(original,{'BUTTONEXT'}))
    items=''
    for index,key in enumerate(('misc','map','players')):
        cases=[a for _,_,_,a in templates if a[4].strip('"').lower()==key]
        if len(cases)!=2:raise ValueError('Vote privilege templates changed: '+key)
        if current==key:cases=[None]
        for case,args in enumerate(cases):
            name='modern_vote_nav_'+key+('' if args is None else '_'+str(case))
            active=args is None
            behavior=('decoration' if active else 'type ITEM_TYPE_BUTTON\n'+hover_feedback(name)+
                      ' action { '+exit_action+' ; '+args[7]+' }\n'+args[8])
            items+=f'''itemDef {{ name "{name}" rect {398+index*146} 12 138 30
                text "{key.upper()}" textfont UI_FONT_ARIBLK_27 textscale .25
                textalign ITEM_ALIGN_CENTER textalignx 69 textaligny 21
                style WINDOW_STYLE_FILLED border WINDOW_BORDER_FULL
                bordercolor .23 .29 .35 .75
                backcolor {'.94 .75 .27 1' if active else '.08 .12 .16 1'}
                forecolor {'.025 .035 .05 1' if active else '.84 .89 .94 1'}
                visible 1 {behavior}
            }}\n'''
    return add_items(text,items,page)


def widgets(text):
    """The original macro identities and scripts, with new paint only."""
    from restyle import blocks,item_prop
    for start,end in reversed(list(blocks(text))):
        b=text[start:end]
        match=re.search(r'\bname\s+"([^"\n]+)"',b,re.I)
        name=match[1] if match else ''
        if name=='window':
            b=item_prop(b,'backcolor','.025 .035 .05 .985')
            b=item_prop(b,'bordercolor','0 0 0 0')
        elif name=='windowtitle':
            b=item_prop(b,'rect','24 8 $evalint(WINDOW_WIDTH-48) 36')
            b=item_prop(b,'textscale','.52')
            b=item_prop(b,'textalignx','0')
            b=item_prop(b,'textaligny','28')
            b=item_prop(b,'forecolor','.96 .97 .98 1')
        elif name=='titlebargradient':
            b=item_prop(b,'rect','24 46 $evalint(WINDOW_WIDTH-48) 2')
            b=item_prop(b,'style','WINDOW_STYLE_FILLED')
            b=item_prop(b,'backcolor','.94 .75 .27 .75')
            # FILLED still draws any nonzero background shader in Window_Paint.
            # Remove the sparse focus texture so this really is a solid rule.
            b=re.sub(r'(?m)^[ \t]*background[ \t]+[^\n]*\n','',b)
        elif name.startswith('subwindowblacktitle'):
            b=item_prop(b,'rect','$evalfloat(SUBWINDOWBLACK_X+16) $evalfloat(SUBWINDOWBLACK_Y+12) $evalfloat(SUBWINDOWBLACK_W-32) 28')
            b=item_prop(b,'textscale','.32')
            b=item_prop(b,'textalignx','0')
            b=item_prop(b,'textaligny','22')
            b=item_prop(b,'bordercolor','0 0 0 0')
        elif name.startswith('subwindowtitle'):
            b=item_prop(b,'textscale','.23')
            b=item_prop(b,'textalignx','8')
            b=item_prop(b,'textaligny','14')
        elif name.startswith('subwindow'):
            b=item_prop(b,'bordercolor','0 0 0 0')
            b=item_prop(b,'backcolor','.045 .065 .085 .96')
        if re.search(r'\bITEM_TYPE_(?:BIND|SLIDER|MULTI|YESNO|EDITFIELD)\b',b):
            # Setting rows are filled native controls, never overlay decorations.
            if not re.search(r'(?im)^\s*style\s+',b):
                b=re.sub(r'(?m)^(\s*visible\s+)',
                         '\t\tstyle WINDOW_STYLE_FILLED \\\n\t\tbackcolor .075 .105 .14 .95 \\\n'+r'\1',b,count=1)
        # Every block in the widget sheet is part of a multi-line #define.
        # item_prop can insert a previously absent property just before `}`;
        # that new physical line must also continue the macro. Without it the
        # native preprocessor sees a top-level `}` and stops loading the menu.
        lines=b.splitlines(keepends=True)
        for i in range(len(lines)-1):
            line=lines[i].rstrip('\r\n').rstrip()
            if not line.endswith('\\'):
                lines[i]=line+' \\\n'
        b=''.join(lines)
        text=text[:start]+b+text[end:]
    return text
