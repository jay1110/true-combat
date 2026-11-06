"""Exercise actual shared BIND/list paint with the shipped Barlow font metrics."""
import argparse
import hashlib
import json
import os
import math
import struct
from pathlib import Path
import shutil
import subprocess
import sys
from compat_validate import msvc_environment
from weapon_regression import function

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/ui_modernize'))
from geometry_audit import Font,FONTS


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true')
    p.add_argument('--classic-dat',action='store_true');a=p.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=ROOT/'src/ui/ui_shared.c';fixture=Path(__file__).with_name('field_text_fixture.c');text=source.read_text(encoding='utf8')
    names=('TCE_FitTextScale','TCE_ListBoxOptionalImages','Item_Bind_Paint','Item_ListBox_Paint',
           'TCE_ParagraphInk','TCE_ModernWrappedPass','TCE_ModernAutoWrapped','Item_Text_AutoWrapped_Paint',
           'TCE_ModernItemTextScale')
    bodies={name:function(text,name) for name in names}
    modern=ROOT/'src/ui/tce_modern_ui.c'
    bodies={n:function(modern.read_text(),n) for n in ('TCE_ModernReadableScale','TCE_ModernInkWidth')}|bodies
    paint=function(text,'Item_Paint');start=paint.index('readableScale = TCE_ModernItemTextScale(')
    assert start < paint.index('Window_Paint(') and 'return' not in paint[start:]
    assert 'item->textscale = savedTextScale;' in paint and 'item->textaligny = savedTextY;' in paint
    if bodies['Item_Bind_Paint'].count('TCE_FitTextScale(')!=1 or bodies['Item_ListBox_Paint'].count('TCE_FitTextScale(')!=1:
        raise ValueError('Both actual paint paths must call the tested font-fit helper')
    fonts=[ROOT/'ui-modern/assets/fonts/tce2_ui'/FONTS[i] for i in range(4)]
    advances=[];glyphsets=[]
    for path in fonts:
        font=Font(path);advances.append('{'+','.join(format(font.width(chr(i),1),'.9g')+'f' if '.' in format(font.width(chr(i),1),'.9g') else format(font.width(chr(i),1),'.9g')+'.0f' for i in range(256))+'}')
        glyphs=[];long=font.i16(font.tables[b'head']+50)
        for i in range(256):
            g=font.glyph(i);loc=font.tables[b'loca']
            start=font.u32(loc+g*4) if long else font.u16(loc+g*2)*2
            end=font.u32(loc+(g+1)*4) if long else font.u16(loc+(g+1)*2)*2
            if start==end:top=pitch=height=width=0
            else:
                at=font.tables[b'glyf']+start
                x0,y0,x1,y1=[font.i16(at+k) for k in (2,4,6,8)]
                top=math.ceil(y1*48/font.upm);pitch=math.floor(x0*48/font.upm)
                width=math.ceil(x1*48/font.upm)-pitch;height=top-math.floor(y0*48/font.upm)
            glyphs.append('{'+','.join(str(v) for v in (top,height,pitch,width,round(font.width(chr(i),1))))+'}')
        glyphsets.append('{{'+','.join(glyphs)+'},1.f}')
    if a.classic_dat:
        fonts=[ROOT/'ui-modern/assets/fonts/tce2_ui'/name for name in
               ('Barlow-Regular_21.dat','Barlow-SemiBold_30.dat',
                'BarlowCondensed-SemiBold_16.dat','BarlowCondensed-SemiBold_27.dat')]
        advances=[];glyphsets=[]
        for path in fonts:
            raw=path.read_bytes();assert len(raw)==20548
            rows=[struct.unpack_from('<7i4fi32s',raw,n*80) for n in range(256)]
            scale=struct.unpack_from('<f',raw,20480)[0]
            advances.append('{'+','.join(repr(row[4]*scale)+'f' for row in rows)+'}')
            glyphsets.append('{{'+','.join('{'+','.join(str(row[i]) for i in (1,6,3,5,4))+'}' for row in rows)+'},'+repr(scale)+'f}')
    metrics='static const float fontAdvances[4][256]={'+','.join(advances)+'};'
    message='Unpure client detected. Invalid .PK3 files referenced!\n\nThe following packs are missing:\n'
    message+=''.join(name+' (local file exists with wrong checksum)\n' for name in
                     ['tce2/pak'+str(i)+'.pk3' for i in range(5)]+['tce2/tce2-1.0.2.pk3','etmain/obj_Depot-.49-Beta9.1_compassfix.pk3'])
    code=fixture.read_text().replace('/* ACTUAL_FIELD_PAINT */','\n'.join(bodies.values())).replace('/* FONT_ADVANCES */',metrics)
    code=code.replace('/* FONT_GLYPHS */','static const fontInfo_t fontGlyphs[4]={'+','.join(glyphsets)+'};').replace('/* MISSING_PACK_CASE */',json.dumps(message))
    unit=out/'field_text.c';unit.write_text(code,encoding='utf8',newline='\n')
    if os.name=='nt':
        env,cc=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44' if a.sanitize else '14.39');exe=out/'field_text.exe'
        cmd=[cc,'/nologo','/TC','/W3','/D_CRT_SECURE_NO_WARNINGS','/O2' if a.optimized else '/Od','/I'+str(ROOT),str(unit),str(ROOT/'src/ui/tce_viewport.c'),'/Fe:'+str(exe),'/Fo:'+str(out)+os.sep]
        if a.sanitize:cmd+=['/fsanitize=address','/Zi','/link','/DEBUG']
    else:
        env=os.environ.copy();cc=shutil.which('clang') or shutil.which('gcc');exe=out/'field_text'
        cmd=[cc,'-std=c99','-O2' if a.optimized else '-O0','-I'+str(ROOT),str(unit),str(ROOT/'src/ui/tce_viewport.c'),'-lm','-o',str(exe)]
        if a.sanitize:cmd+=['-fsanitize=address,undefined']
    built=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True);(out/'compile.log').write_text(built.stdout+built.stderr)
    inputs=[source,modern,ROOT/'src/ui/tce_modern_ui.h',ROOT/'src/ui/tce_viewport.c',ROOT/'src/ui/tce_viewport.h',fixture,Path(__file__),ROOT/'tools/ui_modernize/geometry_audit.py',*fonts]
    report={'build_exit':built.returncode,'command':cmd,'font_profile':'classic-dat' if a.classic_dat else 'ttf-metrics','function_sha256':{name:hashlib.sha256(body.encode()).hexdigest() for name,body in bodies.items()},'inputs':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs},'limits':['Actual full BIND/list/paragraph paint and helper compiled with controlled engine/draw services and shipped font metrics; no native rasterizer or physical input proof.']}
    if not built.returncode:
        run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True);report.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report));return report.get('run_exit',built.returncode)


if __name__=='__main__':raise SystemExit(main())
