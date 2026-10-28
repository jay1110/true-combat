"""Expand actual menu macros and audit their virtual-canvas geometry.

This is deliberately independent of the layout generators. A host C
preprocessor expands #include/#if/macros; ET's string-token paste is represented
by adjacent quoted tokens, and its bounded $evalfloat/$evalint arithmetic is
evaluated separately. This does not emulate engine visibility, ownerdraw output,
font rasterization or input and must be paired with actual engine screenshots.
"""
from __future__ import annotations

import argparse
import ast
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

import semantic

ROOT = Path(__file__).resolve().parents[2]
CANVAS = (0.0, 0.0, 852.0, 480.0)
SCENARIOS = ((1024,768), (1280,720), (1920,1080), (2560,1080), (3440,1440))
TOKEN = re.compile(r'"(?:[^"\\]|\\.)*"|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?|[A-Za-z_]\w*|[^\s]')
VALUES = {'name':1,'rect':4,'text':1,'textscale':1,'textfont':1,
          'textalign':1,'textalignx':1,'textaligny':1,'visible':1,'type':1,
          'ownerdraw':1,'border':1,'bordersize':1,'elementwidth':1,
          'elementheight':1,'maxpaintchars':1,'fullscreen':1}
FLAGS = {'autowrapped','wrapped','decoration'}
FONTS = {2:'BarlowCondensed-SemiBold.ttf',3:'BarlowCondensed-SemiBold.ttf',
         0:'Barlow-Regular.ttf',1:'Barlow-SemiBold.ttf'}


def number(expression):
    """Only finite arithmetic, never eval(), calls, names or indexing."""
    tree = ast.parse(expression.strip(), mode='eval')
    def value(node):
        if isinstance(node, ast.Constant) and type(node.value) in (int,float):
            return float(node.value)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op,(ast.UAdd,ast.USub)):
            return value(node.operand) * (-1 if isinstance(node.op,ast.USub) else 1)
        if isinstance(node,ast.BinOp) and isinstance(node.op,(ast.Add,ast.Sub,ast.Mult,ast.Div)):
            a,b = value(node.left),value(node.right)
            return {ast.Add:lambda:a+b,ast.Sub:lambda:a-b,ast.Mult:lambda:a*b,ast.Div:lambda:a/b}[type(node.op)]()
        raise ValueError('Unsupported geometry expression: '+expression)
    result = value(tree.body)
    if not math.isfinite(result) or abs(result)>1000000:
        raise ValueError('Unbounded geometry expression: '+expression)
    return result


def arithmetic(text):
    # Balanced scan rather than a regexp so nested macro arithmetic is covered.
    pattern = re.compile(r'\$\s*(evalfloat|evalint)\s*\(')
    at = 0; result = []
    while (match := pattern.search(text,at)):
        result.append(text[at:match.start()]);depth=1;end=match.end()
        while end<len(text) and depth:
            depth += (text[end]=='(') - (text[end]==')');end+=1
        if depth:raise ValueError('Unclosed ET geometry expression')
        value = number(text[match.end():end-1])
        result.append(str(int(value)) if match[1]=='evalint' else repr(value));at=end
    return ''.join(result)+text[at:]


def quoted(token):
    # Menu source is Latin-1; unicode_escape would corrupt non-ASCII labels.
    return token[1:-1].replace(r'\"','"').replace(r'\\','\\')


def parse(text):
    """Read menu/item properties at their own brace level, skipping scripts."""
    ts = TOKEN.findall(arithmetic(text));menus=[];i=0
    def block(index):
        if ts[index]!='{':raise ValueError('Missing menu/item brace')
        props={};children=[];index+=1
        while index<len(ts) and ts[index]!='}':
            key=ts[index].lower();index+=1
            if key=='itemdef':
                child,index=block(index);children.append(child)
            elif key in FLAGS:props[key]=True
            elif key in VALUES:
                count=VALUES[key];data=ts[index:index+count];index+=count
                if len(data)!=count:raise ValueError('Truncated '+key)
                if key in ('name','text') or (count==1 and data[0].startswith('"')):
                    text=quoted(data[0]) if data[0].startswith('"') else data[0]
                    while index<len(ts) and ts[index].startswith('"'):
                        text+=quoted(ts[index]);index+=1
                    props[key]=text
                else:
                    try:props[key]=[number(v) for v in data] if count>1 else number(data[0])
                    except (SyntaxError,ValueError) as error:raise ValueError('Unexpanded '+key+': '+repr(data)) from error
            elif key=='{':
                depth=1
                while index<len(ts) and depth:
                    depth+=(ts[index]=='{')-(ts[index]=='}');index+=1
        if index>=len(ts):raise ValueError('Unclosed menu/item')
        props['items']=children
        return props,index+1
    while i<len(ts):
        if ts[i].lower()=='menudef':
            menu,i=block(i+1);menus.append(menu)
        else:i+=1
    return menus


class Font:
    """Read the shipped TTF's advance widths and ascender/descender metrics."""
    def __init__(self,path):
        self.data=path.read_bytes();self.u16=lambda p:struct.unpack_from('>H',self.data,p)[0]
        self.i16=lambda p:struct.unpack_from('>h',self.data,p)[0]
        self.u32=lambda p:struct.unpack_from('>I',self.data,p)[0]
        self.tables={self.data[12+i*16:16+i*16]:self.u32(20+i*16) for i in range(self.u16(4))}
        self.upm=self.u16(self.tables[b'head']+18);self.metrics=self.u16(self.tables[b'hhea']+34)
    def glyph(self,code):
        base=self.tables[b'cmap']
        for index in range(self.u16(base+2)):
            record=base+4+index*8;table=base+self.u32(record+4)
            if self.u16(record) not in (0,3) or self.u16(table)!=4:continue
            count=self.u16(table+6)//2;ends=table+14;starts=ends+count*2+2;deltas=starts+count*2;ranges=deltas+count*2
            for segment in range(count):
                if self.u16(starts+segment*2)<=code<=self.u16(ends+segment*2):
                    delta,offset=self.u16(deltas+segment*2),self.u16(ranges+segment*2)
                    if not offset:return (code+delta)&65535
                    glyph=self.u16(ranges+segment*2+offset+2*(code-self.u16(starts+segment*2)))
                    return ((glyph+delta)&65535) if glyph else 0
        return 0
    def width(self,text,scale):
        clean=re.sub(r'\^[0-9A-Za-z]','',text)
        return sum(self.u16(self.tables[b'hmtx']+min(self.glyph(ord(c)),self.metrics-1)*4) for c in clean)*48*scale/self.upm


def outside(rect,canvas=CANVAS,tolerance=.75):
    x,y,w,h=rect;a,b,c,d=canvas
    return w<0 or h<0 or x<a-tolerance or y<b-tolerance or x+w>a+c+tolerance or y+h>b+d+tolerance


def inspect(menus,path,fonts=None):
    findings=[];items=0
    # Main/global artwork is explicitly frozen by the user's prior approval.
    frozen=Path(path).name in ('main.menu','global.menu')
    def emit(menu,item,kind,rect,**extra):
        findings.append({'menu':menu.get('name','?'),'item':item.get('name','?'),
                         'kind':kind,'rect':rect,'initial_visible':item.get('visible',0),
                         'frozen_approved_artwork':frozen,**extra})
    for menu in menus:
        origin=menu.get('rect',[0,0,640,480])
        if menu.get('fullscreen',0):origin=[0,0,640,480] # Actual Menu_PostParse contract.
        if outside(origin):emit(menu,menu,'menu_outside_canvas',origin)
        for item in menu['items']:
            items+=1;local=item.get('rect',[0,0,0,0]);x=origin[0]+local[0];y=origin[1]+local[1]
            rect=[x,y,local[2],local[3]]
            if outside(rect):emit(menu,item,'item_outside_canvas',rect)
            text=item.get('text','')
            if not text or fonts is None or item.get('ownerdraw') or item.get('autowrapped') or item.get('wrapped') or '\n' in text or '\\n' in text:continue
            font=fonts.get(int(item.get('textfont',0)),fonts[0]);width=font.width(text,item.get('textscale',.55))
            anchor=x+item.get('textalignx',0);align=int(item.get('textalign',0))
            left=anchor-width*(.5 if align in (1,3) else 1 if align==2 else 0)
            textrect=[left,y+item.get('textaligny',0)-48*item.get('textscale',.55)*.75,width,48*item.get('textscale',.55)]
            if outside(textrect,tolerance=2):emit(menu,item,'text_outside_canvas',textrect,text=text)
            if item.get('type')==1 and local[2]>0 and (left<x-.75 or left+width>x+local[2]+.75):
                emit(menu,item,'button_label_outside_tile',textrect,text=text,tile=rect)
    return findings,items


def preprocess(compiler,source,include,fui):
    cmd=[compiler,'-E','-P','-x','c','-undef','-nostdinc','-I',str(include)]
    if fui:cmd+=['-DFUI=1']
    env=os.environ.copy();env['PATH']=str(Path(compiler).resolve().parent)+os.pathsep+env.get('PATH','')
    result=subprocess.run(cmd+[str(source)],env=env,capture_output=True)
    if result.returncode:raise ValueError(str(source)+': '+str(result.returncode)+' '+result.stderr.decode(errors='replace'))
    return result.stdout.decode('latin1')


def source_files(root,variant):
    files={p.relative_to(root).as_posix():p.read_bytes() for p in root.rglob('*') if p.suffix in ('.menu','.h')}
    if variant=='game':
        import restyle
        files.pop('ui/tce_omnibot.menu',None)
        for name in ('ingame_main.menu','options_controls.menu','options_customise_hud.menu'):
            key='ui/'+name;files[key]=restyle.menu(restyle.read(ROOT/'src/assets'/key),Path(key)).encode('latin1')
    return files


def audit(root,compiler,variant):
    files=source_files(root,variant);fonts={i:Font(ROOT/'ui-modern/assets/fonts/tce2_ui'/name) for i,name in FONTS.items()}
    with tempfile.TemporaryDirectory(prefix='tce-ui-geometry-') as tmp:
        include=Path(tmp)
        for path,data in files.items():
            destination=include/path;destination.parent.mkdir(parents=True,exist_ok=True)
            # ET's ## pastes string token values; C only accepts identifier paste.
            destination.write_bytes(data.replace(b'##',b' '))
        tasks=[(path,fui) for path in sorted(files) if path.endswith('.menu') for fui in (False,True)]
        def unit(task):
            path,fui=task;text=preprocess(compiler,include/path,include,fui)
            menus=parse(text);findings,count=inspect(menus,path,fonts)
            return {'path':path,'fui':fui,'source_sha256':hashlib.sha256(files[path]).hexdigest(),
                    'menus':len(menus),'items':count,'findings':findings}
        with ThreadPoolExecutor(max_workers=4) as pool:rows=list(pool.map(unit,tasks))
    counts={}
    for row in rows:
        for finding in row['findings']:counts[finding['kind']]=counts.get(finding['kind'],0)+1
    actionable=[{'path':row['path'],'fui':row['fui'],**finding} for row in rows for finding in row['findings'] if not finding['frozen_approved_artwork']]
    stable=files==source_files(root,variant)
    return {'schema':1,'passed':not actionable and stable,'input_stable':stable,
            'variant':variant,'canvas':CANVAS,'native_transform_scenarios_to_verify':SCENARIOS,
            'inputs':{path:hashlib.sha256(data).hexdigest() for path,data in sorted(files.items())},
            'menu_source_files':len(tasks)//2,'units':rows,'counts':counts,'actionable_findings':actionable,
            'limits':['Host C preprocessing with documented ET string-paste normalization; actual engine parser still needs runtime verification.',
                      'Both FUI branches and all conditional item rects audited; initial visibility reported, no visibility oracle.',
                      'TTF advances check static labels; dynamic strings, wrapped text, ownerdraw content and raster bounds require runtime views.',
                      'Virtual canvas fit is resolution independent only if the native transform correctly fits that canvas; native scaling tests required.']}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=ROOT/'ui-modern')
    p.add_argument('--compiler',default=shutil.which('gcc') or shutil.which('clang') or r'C:\msys64\mingw64\bin\gcc.exe')
    p.add_argument('--variant',choices=('editor','game'),default='game');p.add_argument('--out',type=Path,required=True)
    p.add_argument('--report-only',action='store_true',help='Write diagnostics without failing on actionable geometry findings')
    args=p.parse_args();report=audit(args.root,args.compiler,args.variant)
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'passed':report['passed'],'menu_source_files':report['menu_source_files'],'units':len(report['units']),'counts':report['counts'],'actionable_findings':len(report['actionable_findings'])}))
    return int(not report['passed'] and not args.report_only)


if __name__=='__main__':raise SystemExit(main())
