"""New native layouts for pause, dialogs, votes, credits and quick messages."""
from pathlib import Path
import re
from layout_common import set_window, add_items, icon


def calls(text,names,change):
    from restyle import macro_calls
    for s,e,n,a in reversed(list(macro_calls(text,names))):
        b=change(n,list(a))
        if b is not None:text=text[:s]+n+'( '+', '.join(b)+' )'+text[e:]
    return text


def pause(text):
    text=set_window(text)
    slots={'Deployment':(0,0),'Vote':(1,0),'Referee':(1,0),'RCON':(1,0),
           'Server Info':(0,1),'Options':(1,1),'Disconnect':(0,2),
           'Exit TC:E':(1,2),'Omni-bot Editor':(2,0)}
    editor='"Omni-bot Editor"' in text
    def update(n,a):
        label=a[4].strip('"');col,row=slots[label]
        if editor:
            order={'Deployment':0,'Vote':1,'Referee':1,'RCON':1,'Server Info':2,
                   'Options':3,'Disconnect':4,'Exit TC:E':5,'Omni-bot Editor':6}[label]
            col,row=order%3,order//3;x=24+col*274;w=256
        else:x=84+col*350;w=328
        a[:4]=list(map(str,(x,84+row*108,w,88)))
        a[5:7]=['.37','57'];return a
    text=calls(text,{'BUTTON','BUTTONEXT'},update)
    art=''
    entries=[('play',0,0),('settings',1,0),('info',0,1),('controls',1,1),('server',0,2),('exit',1,2)]
    if not editor:
        for i,(glyph,col,row) in enumerate(entries):
            art+=icon('modern_pause_'+str(i),glyph,108+col*350,113+row*108,28)
        text=add_items(text,art,'ingame_main')
    return text


def dialog(text):
    from restyle import macro_calls,blocks,item_prop
    # Subwindow positions below use the full desktop coordinate space. Keeping
    # a legacy 640-wide parent would add its 106-unit centering offset twice.
    text=set_window(text)
    labels=list(macro_calls(text,{'LABEL','LABELWHITE'}))
    variable_labels=list(macro_calls(text,{'CVARLABEL','CVARLABELWHITE'}))
    action_calls=list(macro_calls(text,{'BUTTON','BUTTONEXT','NAMEDBUTTON','NAMEDBUTTONEXT'}))
    # The respawn explanation is a complete long paragraph. Give long static
    # messages a wide card instead of squeezing their original text into a
    # confirmation-sized box; native font hinting can otherwise overrun it.
    long_message=any(len(a[4].strip('"'))>120 for _,_,_,a in labels)
    width=804 if long_message else 444
    # Preserve native modal/timeout/input lifecycle; only widen the message card.
    height=max(180,min(332,98+len(labels)*19))
    if variable_labels:height=308 if len(action_calls)>2 else 280
    for name,value in [('SUBWINDOW_WIDTH',str(width)),('SUBWINDOW_HEIGHT',str(height)),
                       ('SUBWINDOW_X',str((852-width)//2)),('SUBWINDOW_Y',str((480-height)//2))]:
        text=re.sub(r'(?m)^#define\s+'+name+r'\s+[^\n]+','#define '+name+' '+value,text)
    count=0
    source=list(macro_calls(text,{'LABEL','LABELWHITE','CVARLABEL','CVARLABELWHITE','BUTTON','BUTTONEXT','NAMEDBUTTON','NAMEDBUTTONEXT'}))
    button_count=len(action_calls)
    for s,e,n,a in reversed(source):
        prefix=a[:1] if n.startswith('NAMED') else []
        if prefix:a=a[1:]
        if n.startswith('CVARLABEL'):
            a[:4]=['SUBWINDOW_X+24','SUBWINDOW_Y+60','SUBWINDOW_WIDTH-48','132']
            a[5:]=['.25' if long_message else '.26','ITEM_ALIGN_LEFT','0','15']
        elif n.startswith('LABEL'):
            idx=sum(1 for z in labels if z[0]<s)
            a[:4]=['SUBWINDOW_X+24','SUBWINDOW_Y+'+str(52+idx*19),'SUBWINDOW_WIDTH-48','18']
            a[5:]=['.25' if long_message else '.26','ITEM_ALIGN_LEFT','0','15']
        else:
            right=('.5' in a[0] or '.25' in a[0])
            offset=str((width-190)//2) if button_count==1 else str(width-214) if right else '24'
            a[:4]=['SUBWINDOW_X+'+offset,'SUBWINDOW_Y+SUBWINDOW_HEIGHT-42','190','30']
            if prefix and len(action_calls)>2:
                a[:4]=['SUBWINDOW_X+24','SUBWINDOW_Y+SUBWINDOW_HEIGHT-80','SUBWINDOW_WIDTH-48','30']
            a[5:7]=['.30','21']
        text=text[:s]+n+'( '+', '.join(prefix+a)+' )'+text[e:]
    # Error text and timeout counters are native fields, retained and enlarged.
    for s,e in reversed(list(blocks(text))):
        b=text[s:e]
        if re.search(r'\bITEM_TYPE_TIMEOUT_COUNTER\b',b):
            b=item_prop(b,'rect','$evalfloat(SUBWINDOW_X+24) $evalfloat(SUBWINDOW_Y+92) $evalfloat(SUBWINDOW_WIDTH-48) 20')
            b=item_prop(b,'textalign','ITEM_ALIGN_LEFT');b=item_prop(b,'textalignx','0')
            b=item_prop(b,'textscale','.26');b=item_prop(b,'textaligny','15')
        text=text[:s]+b+text[e:]
    return text


def input_dialog(text,error=False):
    """Native password/IP and dynamic error dialogs with usable input spacing."""
    from restyle import blocks,item_prop
    height=280 if error else 180
    top=(480-height)//2
    text=set_window(text)
    for name,value in [('SUBWINDOW_WIDTH','444'),('SUBWINDOW_HEIGHT',str(height)),
                       ('SUBWINDOW_X','204'),('SUBWINDOW_Y',str(top))]:
        text=re.sub(r'(?m)^#define\s+'+name+r'\s+[^\n]+','#define '+name+' '+value,text)
    def change(name,args):
        if name=='EDITFIELDLEFT':
            args[:4]=['228',str(top+66),'396','28']
            args[5:7]=['.26','20']
        elif name=='CVARLABEL':
            args[:4]=['228',str(top+54),'396','160']
            args[5:]=['.26','ITEM_ALIGN_LEFT','0','18']
        elif name=='BUTTON':
            left=args[4].strip('"').upper()=='BACK'
            args[:4]=['228' if left else '434',str(top+height-42),'190','30']
            args[5:7]=['.30','21']
        else:return None
        return args
    text=calls(text,{'EDITFIELDLEFT','CVARLABEL','BUTTON'},change)
    for start,end in reversed(list(blocks(text))):
        block=text[start:end]
        if re.search(r'(?im)^\s*name\s+"(?:background|loadPanel)"',block):
            block=item_prop(block,'rect','0 0 852 480')
        if re.search(r'(?im)^\s*name\s+"efleftback(?:Password|Server IP):"',block):
            # The native value starts at label width +8. These two short labels
            # use the actual Barlow .26 advances (~57/53px), with 5px padding.
            x=288 if 'efleftbackPassword:' in block else 284
            block=item_prop(block,'rect',f'{x} {top+66} {624-x} 28')
            block=item_prop(block,'backcolor','.075 .105 .14 .98')
            block=item_prop(block,'border','WINDOW_BORDER_FULL')
            block=item_prop(block,'bordercolor','.35 .32 .22 .85')
        text=text[:start]+block+text[end:]
    return text


def mod_browser(text,relative):
    from restyle import blocks,item_prop
    text=lists(text,relative)
    for start,end in reversed(list(blocks(text))):
        block=text[start:end]
        if re.search(r'\bFEEDER_MODS\b',block):
            block=item_prop(block,'rect','24 66 804 350')
            block=item_prop(block,'elementwidth','780')
            block=item_prop(block,'elementheight','26')
            block=item_prop(block,'textscale','.28')
            block=item_prop(block,'textaligny','-2')
            block=re.sub(r'\bcolumns\s+1\s+0\s+200\s+33\b','columns 1 0 780 33',block)
        text=text[:start]+block+text[end:]
    def change(name,args):
        back=args[4].strip('"').upper()=='BACK'
        args[:4]=['24' if back else '638','442','184' if back else '190','28']
        args[5:7]=['.28','20']
        return args
    return calls(text,{'BUTTON'},change)


def tce_modes(text):
    # This original page is a game-type selector, not a mod-list feeder.
    from layout_common import panel
    from restyle import macro_calls
    text=set_window(text)
    def change(name,args):
        if name=='MULTIACTION':
            args[:4]=['168','204','516','32'];args[5:7]=['.30','22']
        elif name=='BUTTON':
            back=args[4].strip('"').upper()=='BACK'
            args[:4]=['24' if back else '638','442','184' if back else '190','28']
            args[5:7]=['.28','20']
        else:return None
        return args
    text=calls(text,{'MULTIACTION','BUTTON'},change)
    window=list(macro_calls(text,{'WINDOW'}))
    if len(window)!=1:raise ValueError('Unexpected TC:E mode selector window')
    at=window[0][1]
    text=text[:at]+'\n'+panel('modern_tce_mode_card',144,152,564,136)+text[at:]
    return text


def lists(text,relative):
    from restyle import blocks,item_prop
    text=set_window(text)
    for key,val in [('SUBWINDOW_WIDTH','804'),('SUBWINDOW_HEIGHT','420'),('SUBWINDOW_X','24'),('SUBWINDOW_Y','12')]:
        text=re.sub(r'(?m)^#define\s+'+key+r'\s+[^\n]+','#define '+key+' '+val,text)
    for s,e in reversed(list(blocks(text))):
        b=text[s:e]
        if re.search(r'\bITEM_TYPE_LISTBOX\b',b):
            b=item_prop(b,'rect','24 66 804 350')
            b=item_prop(b,'elementwidth','780');b=item_prop(b,'elementheight','20')
            b=item_prop(b,'textscale','.25');b=item_prop(b,'textaligny','-4')
            if re.search(r'\bFEEDER_SERVERSTATUS\b',b):
                b=re.sub(r'\bcolumns\s+4\s+0\s+40\s+20\s+40\s+40\s+10\s+90\s+40\s+10\s+135\s+40\s+20',
                         'columns 4 0 250 20 250 120 10 370 120 10 490 280 20',b)
        text=text[:s]+b+text[e:]
    buttons=list(__import__('restyle').macro_calls(text,{'BUTTON'}))
    for s,e,n,a in reversed(buttons):
        idx=next(i for i,v in enumerate(buttons) if v[0]==s)
        a[:4]=[str(24+idx*270),'442','256','28'];a[5:7]=['.29','21']
        text=text[:s]+n+'( '+', '.join(a)+' )'+text[e:]
    return text


def quick(text):
    # Existing key-bound command hierarchy remains intact, now in a readable rail.
    text=text.replace('rect\t\t0 100 640 380','rect\t\t0 0 852 480')
    text=re.sub(r'(#define ORIGIN_QUICKMESSAGE\s+)10 10',r'\g<1>24 56',text)
    text=text.replace('0 19 204 136','0 0 300 258')
    text=text.replace('$evalfloat((0)+2) $evalfloat((19)+2) $evalfloat((204)-4) 12',
                      '12 12 276 26')
    text=text.replace('6 $evalfloat( 35 + ( 12 * POS )) 128 10',
                      '16 $evalfloat( 52 + ( 24 * POS )) 268 22')
    text=re.sub(r'(textscale\s+)\.2\b',r'\g<1>.28',text)
    text=re.sub(r'(textscale\s+)\.19\b',r'\g<1>.32',text)
    text=re.sub(r'(textaligny\s+)8\b',r'\g<1>17',text)
    text=re.sub(r'(textaligny\s+)10\b',r'\g<1>20',text)
    return text


def apply(text,relative):
    path=Path(relative)
    if path.parent.name!='ui':return text
    name=path.name
    if name=='ingame_main.menu':return pause(text)
    if name in ('popup_password.menu','playonline_connecttoip.menu'):
        return input_dialog(text)
    if name=='popup_hostgameerrormessage.menu':return input_dialog(text,error=True)
    if name=='mods.menu':return mod_browser(text,relative)
    if name=='tcemods.menu':return tce_modes(text)
    if name in ('ingame_serverinfo.menu','playonline_serverinfo.menu'):
        return lists(text,relative)
    if name.startswith('wm_') and 'QM_MENU_START' in text:return quick(text)
    if '#define SUBWINDOW_WIDTH' in text and 'SUBWINDOWBLACK(' in text and not re.search(r'\b(?:ITEM_TYPE_LISTBOX|EDITFIELD[A-Z]*|NUMERICFIELD[A-Z]*|SLIDER[A-Z]*)\b',text):
        return dialog(text)
    return text
