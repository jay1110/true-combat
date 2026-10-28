"""Compile exact shared viewport math, UI wrappers and logical mouse dispatch."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from weapon_regression import function
from compat_validate import msvc_environment
ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--optimized',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    paths=['src/ui/tce_viewport.c','src/ui/tce_viewport.h','src/ui/tce_ui_coordinates.c','src/ui/tce_ui_coordinates.h','src/ui/tce_modern_ui.c','src/ui/tce_modern_ui.h','src/ui/ui_atoms.c','src/ui/ui_shared.c','src/ui/ui_main.c','src/cgame/cg_hud_viewport.c','src/cgame/cg_hud_viewport.h','src/cgame/cg_drawtools.c','src/cgame/cg_draw.c','src/cgame/cg_newDraw.c']
    sources={p:(ROOT/p).read_text(encoding='utf-8-sig') for p in paths}
    shared=sources['src/ui/ui_shared.c'];units=[function(shared,'TCE_MenuViewportBackground'),function(shared,'TCE_MenuViewportAnchorY'),function(shared,'TCE_UISharedViewportAdjust')]
    matches=list(re.finditer(r'^void AdjustFrom640\(',shared,re.M))
    assert len(matches)==3
    for match,name in zip(matches,['SharedWindowsAdjust','SharedLinuxAdjust','SharedGenericAdjust']):
        units.append(function(shared[match.start():],'AdjustFrom640').replace('void AdjustFrom640(',f'void {name}(',1))
    units += [function(sources['src/ui/ui_atoms.c'],'UI_AdjustFrom640')]
    units += [function(sources['src/ui/tce_modern_ui.c'],n) for n in
              ['TCE_ModernWidth','TCE_ModernText','TCE_ModernTextFit','TCE_ModernCursor','TCE_ModernBackdrop']]
    units += [function(sources['src/ui/ui_main.c'],n) for n in ['_UI_MouseEvent','_UI_DrawSides','_UI_DrawTopBottom']]
    units += [function(sources['src/cgame/cg_newDraw.c'],'CG_MouseEvent')]
    # The width NaN gate has two preprocessor-selected opening braces; the
    # raw balanced-C extractor cannot parse that unchanged legacy function.
    drawtools=sources['src/cgame/cg_drawtools.c'];start=drawtools.index('void CG_DrawPic(')
    units += [drawtools[start:drawtools.index('\n// NERVE - SMF',start)]]
    units += [function(sources['src/cgame/cg_draw.c'],n) for n in ['CG_DrawDisconnect','CG_DrawLagometer']]
    assert 'UI_AdjustFrom640(' in function(sources['src/ui/ui_atoms.c'],'UI_DrawHandlePic')
    assert 'TCE_ModernCursor(uiInfo.uiDC.cursorx,uiInfo.uiDC.cursory,' in sources['src/ui/ui_main.c']
    assert 'else UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory,' in sources['src/ui/ui_main.c']
    for name,value in [('cg_wideScreen','1'),('ui_menuScale','1'),('ui_menuMaxHeight','1080'),('cg_hudScale','.85'),('cg_hudMaxHeight','1080')]:
        assert re.search(r'\{\s*NULL,\s*"'+name+r'",\s*"'+re.escape(value)+r'",\s*CVAR_ARCHIVE\s*\}',sources['src/ui/ui_main.c'])
    assert 'if (CG_HudViewportAdjust(x, y, w, h, tce_uiCoordinates)) return;' in function(sources['src/cgame/cg_drawtools.c'],'CG_AdjustFrom640')
    frame=function(sources['src/cgame/cg_draw.c'],'CG_Draw2D')
    assert 'CG_HudViewportWorld();\n\t\tCG_DrawCrosshair();\n\t\tCG_HudViewportBegin(.5f, .5f);' in frame
    assert 'CG_HudViewportAnchor(.5f, .5f);\n\t\tCG_DrawWarmup();\n\t\tCG_HudViewportAnchor(0, 0);' in frame
    assert 'CG_HudViewportAnchor(0, 0);\n\t\t\tCG_DrawSpectator();\n\t\t\tCG_HudViewportAnchor(.5f, .5f);\n\t\t\tCG_DrawCrosshairNames();' in frame
    assert 'CG_MenuViewportBegin();\n\t\tCG_DrawOverlays();\n\t\tCG_MenuViewportEnd();' in frame
    assert re.search(r'else\s*\{\s*CG_HudViewportFull\(\);\s*CG_DrawOverlays\(\);\s*\}',frame)
    paint=function(shared,'Menu_Paint')
    assert 'previousViewportMenu = tceMenuViewportPaintMenu;' in paint
    assert 'tceMenuViewportPaintMenu = menu;' in paint and 'tceMenuViewportPaintMenu = previousViewportMenu;' in paint
    assert 'CG_HudViewportMaskMargins(type == 2);' in function(sources['src/cgame/cg_draw.c'],'CG_DrawWeapReticle')
    assert 'CG_HudViewportMaskMargins(0);' in function(sources['src/cgame/cg_draw.c'],'CG_DrawBinocReticle')
    context=re.sub(r'^#include[^\n]*\n','',sources['src/cgame/cg_hud_viewport.c'],flags=re.M)
    template=Path(__file__).with_name('viewport_fixture.c');fixture=template.read_text().replace('/* ACTUAL_FUNCTIONS */','\n\n'.join(units)).replace('/* ACTUAL_HUD_CONTEXT */',context)
    fixture=fixture.replace('/* ACTUAL_DRAW_ADAPTERS */','\n\n'.join(function(sources['src/cgame/cg_drawtools.c'],n) for n in ['CG_TCAdjustFrom640','CG_AdjustFrom640','CG_DrawSides','CG_DrawTopBottom']))
    generated=out/'fixture.c';generated.write_text(fixture,encoding='utf-8',newline='\n')
    env=os.environ.copy()
    if os.name=='nt':env,compiler=msvc_environment(out,'x64' if args.sanitize else 'x86','14.44' if args.sanitize else '14.39')
    else:compiler=shutil.which('clang') or shutil.which('gcc')
    report={'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths+[str(template.relative_to(ROOT)),str(Path(__file__).relative_to(ROOT))]},'results':{},'limits':['Instrumented cvar/display services and actual logical mouse dispatch; real OS input/rendering is not simulated.','Exact production CGame private context and shared-wrapper precedence are compiled; real frame scopes/pixel acceptance require a live game.','Actual cursor and backdrop scopes are compiled in both UI/CGame. Text fitting uses controlled font advances and paint services, not the actual TTF rasterizer.']}
    for mode in ['ui','cgame-shared']:
        directory=out/mode;directory.mkdir(exist_ok=True);exe=directory/('fixture.exe' if os.name=='nt' else 'fixture')
        if os.name=='nt':
            command=[compiler,'/nologo','/TC','/W3','/O2' if args.optimized else '/Od','/I'+str(ROOT),str(generated),str(ROOT/'src/ui/tce_viewport.c'),str(ROOT/'src/ui/tce_ui_coordinates.c'),'/Fe:'+str(exe)]
            if mode=='cgame-shared':command+=['/DCGAMEDLL']
            if args.sanitize:command+=['/fsanitize=address','/Zi','/link','/DEBUG']
        else:
            command=[compiler,'-std=c99','-O2' if args.optimized else '-O0','-I'+str(ROOT),str(generated),str(ROOT/'src/ui/tce_viewport.c'),str(ROOT/'src/ui/tce_ui_coordinates.c'),'-lm','-o',str(exe)]
            if mode=='cgame-shared':command+=['-DCGAMEDLL']
            if args.sanitize:command+=['-fsanitize=address,undefined','-g']
        built=subprocess.run(command,cwd=directory,env=env,capture_output=True,text=True);(directory/'compile.log').write_text(built.stdout+built.stderr)
        row={'command':command,'compile_exit':built.returncode}
        if built.returncode:print(built.stdout+built.stderr)
        else:
            result=subprocess.run([str(exe)],cwd=directory,env=env,capture_output=True,text=True,timeout=20);row.update(run_exit=result.returncode,stdout=result.stdout,stderr=result.stderr);print(mode+': '+result.stdout+result.stderr,end='')
        report['results'][mode]=row
    report['passed']=all(r.get('run_exit',1)==0 for r in report['results'].values());(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    return int(not report['passed'])
if __name__=='__main__':raise SystemExit(main())
