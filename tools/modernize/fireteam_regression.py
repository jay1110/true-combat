"""Finite production fireteam transport, parsing, sorting and drawing contracts."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
from compat_validate import msvc_environment
from weapon_regression import function
ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--optimized',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source_paths=['src/cgame/cg_tce_fireteam.c','src/cgame/cg_fireteamoverlay.c','src/cgame/cg_draw.c','src/cgame/cg_drawtools.c','src/cgame/cg_popupmessages.c','src/cgame/tce_popup_draw.c','src/cgame/tce_popup_draw.h','src/game/g_fireteams.c','src/game/g_cmds.c','src/cgame/cg_local.h','src/game/g_local.h','src/game/q_shared.c','src/game/q_shared.h','src/game/bg_public.h','tools/modernize/fireteam_regression.py']
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report={'source_hashes':{p:digest(ROOT/p) for p in source_paths},'results':{},'limits':['Actual production functions; instrumented engine transport, clock and renderer, not live game or pixel acceptance.','Command fixture executes the actual ClientCommand prefix through fireteam dispatch; later unrelated commands are outside this fixture.']}
    env=None
    if os.name=='nt':env,compiler=msvc_environment(out,'x64' if args.sanitize else 'x86','14.44' if args.sanitize else '14.39')
    else:compiler='cc'
    for mode in ('client','server'):
        directory=out/mode;directory.mkdir(exist_ok=True)
        template=Path(__file__).with_name('fireteam_'+mode+'_fixture.c')
        text=template.read_text()
        if mode=='client':
            source=(ROOT/'src/cgame/cg_fireteamoverlay.c').read_text()
            names=('CG_IsOnSameFireteam','CG_IsFireTeamLeader','CG_SortFireTeam','CG_SortClientFireteam','CG_SortedFireTeamPlayerForPosition','CG_ParseFireteams')
            units=[]
            for name in names:
                definition=re.search(r'(?m)^(?:int QDECL|void|fireteamData_t \*|clientInfo_t \*)\s*'+name+r'\(',source)
                if not definition:raise ValueError('Definition missing: '+name)
                units.append(function(source[definition.start():],name))
            text=text.replace('/* ACTUAL_SORT_FUNCTIONS */','\n\n'.join(units))
            draw=(ROOT/'src/cgame/cg_draw.c').read_text()
            frame=function(draw,'CG_Draw2D')
            assert frame.index('CG_DrawLimboMessage();') < frame.index('CG_DrawFireteamHUD();')
            assert 'if (!cg_paused.integer)\n\t\t{\n\t\t\tCG_DrawFireteamHUD();' in frame
            for name in ('CG_DrawFollow','CG_DrawSpectatorMessage','CG_DrawLimboMessage'):
                assert 'if (!cg.showFireteamMenu) '+name+'();' in frame
            for name in ('CG_DrawFollow','CG_DrawLimboMessage'):
                assert 'a.paint = CG_FireteamMessagePaint; a.small = CG_FireteamMessageSmall;' in function(draw,name)
            hud=[function((ROOT/'src/cgame/cg_drawtools.c').read_text(),name) for name in ('CG_TCAdjustFrom640','CG_AdjustFrom640')]
            hud += [function(draw,name) for name in ('CG_TCEFireteamTextY','CG_TCEFireteamStatusOffset','CG_MessagePaint','CG_MessageSmall','CG_FireteamMessagePaint','CG_FireteamMessageSmall','CG_DrawFireteamHUD')]
            text=text.replace('/* ACTUAL_HUD_FUNCTION */','\n\n'.join(hud))
            popup=(ROOT/'src/cgame/cg_popupmessages.c').read_text()
            names=('CG_PopupPaint','CG_PopupMessageBottom','CG_PopupCompactPic','CG_PopupCompactPaint','CG_PopupDuration','CG_DrawPMItems')
            text=text.replace('/* ACTUAL_POPUP_FUNCTIONS */','\n\n'.join(function(popup,name) for name in names))
        else:
            source=(ROOT/'src/game/g_fireteams.c').read_text()
            text=text.replace('/* ACTUAL_SERVER_FUNCTION */',source.replace('#include "g_local.h"','',1))
            cmds=(ROOT/'src/game/g_cmds.c').read_text()
            full=function(cmds,'ClientCommand');prefix=full[:full.index('\n\telse if (Q_stricmp(cmd, "showstats")')]+ '\n}\n'
            names=('ConcatArgs','G_SayTo','G_Say','Cmd_Say_f','G_VoiceTo','G_Voice','Cmd_Voice_f')
            # The helpers precede ClientCommand; preserve their exact bodies.
            units=[function(cmds,name) for name in names]+[prefix]
            text=text.replace('/* ACTUAL_COMMAND_FUNCTIONS */','\n\n'.join(units))
        generated=directory/'fixture.c';generated.write_text(text,encoding='utf-8',newline='\n')
        exe=directory/('fixture.exe' if os.name=='nt' else 'fixture')
        if os.name=='nt':
            command=[compiler,'/nologo','/TC','/W3','/O2' if args.optimized else '/Od','/DWIN32','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT),'/I'+str(ROOT/'src/game'),str(generated),str(ROOT/'src/game/q_shared.c'),'/Fe:'+str(exe)]
            if args.sanitize:command+=['/fsanitize=address','/Zi','/link','/DEBUG']
        else:
            command=[compiler,'-O2' if args.optimized else '-O0','-I'+str(ROOT),'-I'+str(ROOT/'src/game'),str(generated),str(ROOT/'src/game/q_shared.c'),'-lm','-o',str(exe)]
            if args.sanitize:command+=['-fsanitize=address,undefined','-g']
        run=subprocess.run(command,cwd=directory,env=env,capture_output=True,text=True)
        (directory/'compile.log').write_text(run.stdout+run.stderr)
        row={'command':command,'compile_exit':run.returncode,'template_sha256':digest(template),'generated_sha256':digest(generated)}
        if run.returncode:print(run.stdout+run.stderr)
        else:
            result=subprocess.run([str(exe)],cwd=directory,env=env,capture_output=True,text=True,timeout=20)
            row.update(run_exit=result.returncode,stdout=result.stdout,stderr=result.stderr);print(result.stdout+result.stderr,end='')
        report['results'][mode]=row
    report['passed']=all(x.get('run_exit',1)==0 for x in report['results'].values())
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    return int(not report['passed'])
if __name__=='__main__':raise SystemExit(main())
