"""Wide desktop settings geometry; original controls and scripts stay intact.

Called once after the existing visual pass. No generated pages, cvars, commands,
shared macro definitions or main-menu resources are introduced here.
"""
from pathlib import Path
import re
from layout_common import header_canvas, header_button, appearance_link

PAGES = frozenset(('options.menu', 'options_system.menu', 'options_controls.menu',
                   'options_customise_game.menu', 'options_customise_hud.menu'))
ROWS = frozenset(('BIND', 'SLIDER', 'MULTI', 'MULTIACTION', 'YESNO', 'YESNOACTION',
                  'CHECKBOXALIGNX', 'CHECKBOXALIGNXACTION', 'EDITFIELD'))
CALLS = ROWS | {'SUBWINDOW', 'CVARFLOATLABEL', 'BUTTON', 'WINDOW'}


def _label(args):
    return args[4].strip('"').lower()


def _fmt(value):
    return f'{value:g}' if isinstance(value,(float,int)) else str(value)


def _geometry(args,x,y,width,height):
    result=list(args)
    result[:4]=list(map(_fmt,(x,y,width,height)))
    return result


def _row(args,name,x,y,width,height):
    result=_geometry(args,x,y,width,height)
    result[5]='.22'
    # These native macros place label and editable value on either side of
    # the row midpoint. Keep their original types, names and value actions.
    if name.startswith('CHECKBOXALIGNX'):
        result[6:8]=['8',str(min(height-2,14))]
    else:
        result[6]=str(min(height-2,14))
    return result


def _sections(calls):
    groups={};current=None
    for call in calls:
        start,end,name,args=call
        if name=='SUBWINDOW':
            current=_label(args)
            if current in groups:raise ValueError('Duplicate settings section: '+current)
            groups[current]={'header':call,'rows':[],'labels':[]}
        elif name in ROWS:
            if current is None:raise ValueError('Setting outside a named section')
            groups[current]['rows'].append(call)
        elif name=='CVARFLOATLABEL':
            if current is None:raise ValueError('Value label outside a named section')
            groups[current]['labels'].append(call)
    return groups


def _stack(groups,plan,positions,rects):
    """Plan entries: (section,x,y,w,row_h); no source/control reordering."""
    for section,x,y,w,row_h in plan:
        group=groups[section]
        h=18+len(group['rows'])*row_h
        positions[group['header'][0]]=_geometry(group['header'][3],x,y,w,h)
        rects.append((x,y,w,h))
        row_positions={}
        for index,(start,end,name,args) in enumerate(group['rows']):
            ry=y+18+index*row_h
            positions[start]=_row(args,name,x+8,ry,w-16,row_h)
            row_positions[args[7] if not name.startswith('CHECKBOXALIGNX') else args[8]]=(ry,row_h)
        for start,end,name,args in group['labels']:
            # Existing numeric sensitivity readout shares its slider's row.
            key=args[4]
            candidates=[value for cvar,value in row_positions.items() if cvar.split()[0]==key]
            if len(candidates)!=1:raise ValueError('Unbound settings readout: '+key)
            ry,rh=candidates[0]
            result=_geometry(args,x+w-50,ry,40,rh)
            result[5:]=['.20','ITEM_ALIGN_RIGHT','40',str(min(rh-2,14))]
            positions[start]=result


def _plan_controls(groups):
    columns=[(('movement','mouse input','miscellaneous'),24,15),
             (('weapon use','weapon selection','chat'),298,15),
             (('pop-ups','fire-teams'),572,17)]
    plan=[]
    for sections,x,rh in columns:
        y=60
        for section in sections:
            plan.append((section,x,y,256,rh))
            y+=18+len(groups[section]['rows'])*rh+6
        if y-6>432:raise ValueError('Controls column exceeds safe footer boundary')
    return plan


def _decoration(rects):
    items=[]
    for index,(x,y,w,h) in enumerate(rects):
        items.append(f'''    itemDef {{
        name "modern_settings_card_{index}"
        rect {x} {y} {w} {h}
        style WINDOW_STYLE_FILLED
        backcolor .045 .065 .085 .98
        visible 1
        decoration
    }}
    itemDef {{
        name "modern_settings_accent_{index}"
        rect {x} {y} 3 16
        style WINDOW_STYLE_FILLED
        backcolor .94 .75 .27 .95
        visible 1
        decoration
    }}
''')
    return ''.join(items)


def _landing_icons():
    # These are painted after the existing opaque button macros. They decorate
    # the full clickable cards and cannot receive input or change navigation.
    items=[]
    for key,glyph,x,y in (('game','play',24,84),('view','graphics',444,84),
                          ('controls','controls',24,252),('system','settings',444,252)):
        items.append(f'''    itemDef {{
        name "modern_options_icon_{key}"
        rect {x+24} {y+32} 64 64
        style WINDOW_STYLE_SHADER
        background "tce2_ui/icons/{glyph}"
        forecolor .94 .75 .27 1
        visible 1
        decoration
    }}
    itemDef {{
        name "modern_options_divider_{key}"
        rect {x+112} {y+24} 1 80
        style WINDOW_STYLE_FILLED
        backcolor .94 .75 .27 .25
        visible 1
        decoration
    }}
    itemDef {{
        name "modern_options_arrow_{key}"
        rect {x+344} {y+54} 20 20
        style WINDOW_STYLE_SHADER
        background "tce2_ui/icons/chevron_right"
        forecolor .94 .75 .27 .65
        visible 1
        decoration
    }}
''')
    return ''.join(items)


def apply(text,relative):
    relative=Path(relative)
    if relative.parent.as_posix()!='ui' or relative.name not in PAGES:return text
    if '// modern settings desktop v1' in text:return text
    # Lazy import: restyle.menu invokes this module after its own definitions.
    from restyle import blocks,item_prop,macro_calls
    calls=list(macro_calls(text,CALLS))
    positions={};rects=[]
    page=relative.name
    if page=='options.menu':
        category={'game':0,'view':1,'controls':2,'system':3,'back':5}
        for start,end,name,args in reversed(calls):
            if name!='BUTTON':continue
            label=_label(args)
            if label not in category:raise ValueError('Unaccounted settings action: '+label)
            new_args=header_button(args,category[label])
            text=text[:start]+name+'( '+', '.join(new_args)+' )'+text[end:]
        text=header_canvas(text)
        return appearance_link(text).replace('// Options Menu //',
                       '// Options Menu //\n// modern settings desktop v1')
    else:
        groups=_sections(calls)
        if page=='options_controls.menu':
            plan=_plan_controls(groups)
        elif page=='options_customise_hud.menu':
            plan=[('hud display',24,60,384,30),('in-game display',444,60,384,34)]
        elif page=='options_customise_game.menu':
            plan=[('player',24,60,256,34),('physics',24,144,256,34),('chat',24,262,256,34),
                  ('downloads',312,60,516,34),('miscellaneous',312,218,516,34)]
        else:
            plan=[('audio',24,60,232,30),('networking',24,222,232,34)]
        if page!='options_system.menu' and set(groups)!={row[0] for row in plan}:
            raise ValueError('Unaccounted settings category in '+page)
        _stack(groups,plan,positions,rects)
        if page=='options_system.menu':
            if set(groups)!={'audio','video','networking'}:raise ValueError('Unaccounted System section')
            video=groups['video']
            if len(video['rows'])!=16 or video['labels']:raise ValueError('Unexpected System video controls')
            positions[video['header'][0]]=_geometry(video['header'][3],282,60,546,312)
            rects.append((282,60,546,312))
            # Keep the real long resolution labels on one full-width row.
            # All list labels/values remain exact; shrinking or truncating
            # their content is not a substitute for sufficient geometry.
            other=[]
            for call in video['rows']:
                start,end,name,args=call
                if _label(args)=='resolution:':
                    positions[start]=_row(args,name,282,86,546,30)
                else:other.append(call)
            if len(other)!=15:raise ValueError('Missing/duplicate System resolution row')
            for index,(start,end,name,args) in enumerate(other):
                x=282+284*(index//8);y=128+(index%8)*30
                positions[start]=_row(args,name,x,y,262,28)

    for start,end,name,args in calls:
        if name!='BUTTON' or start in positions:continue
        label=_label(args)
        if label=='calibrate brightness':
            positions[start]=_geometry(args,282,382,262,30)
        elif label=='set recommended':
            positions[start]=_geometry(args,566,382,262,30)
        elif label in ('back','apply','default settings'):
            x,width=(24,180) if label=='back' else (624,204)
            positions[start]=_geometry(args,x,442,width,26)
        else:raise ValueError('Unaccounted settings action: '+label)
        positions[start][5:7]=['.27','18']

    for start,end,name,args in reversed(calls):
        if start in positions:
            text=text[:start]+name+'( '+', '.join(positions[start])+' )'+text[end:]
    # Keep the source menuDef, WINDOW title and original background identities.
    for key,value in (('WINDOW_X','0'),('WINDOW_Y','0'),('WINDOW_WIDTH','852'),('WINDOW_HEIGHT','480')):
        text,count=re.subn(r'(?m)^#define\s+'+key+r'\s+[^\n]+','#define '+key+' '+value,text)
        if count!=1:raise ValueError('Missing/duplicate settings geometry: '+key)
    for start,end in reversed(list(blocks(text))):
        block=text[start:end]
        if re.search(r'(?im)^\s*name\s+"background"\s*$',block):
            block=item_prop(block,'rect','0 0 852 480')
            block=item_prop(block,'backcolor','.025 .035 .05 .97')
            text=text[:start]+block+text[end:]
    window=list(macro_calls(text,{'WINDOW'}))
    if len(window)!=1:raise ValueError('Settings WINDOW count changed')
    insert=window[0][1]
    text=text[:insert]+'\n// modern settings desktop v1\n'+_decoration(rects)+text[insert:]
    return text
