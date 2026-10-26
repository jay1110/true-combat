"""Focused natural door traversal in the already running single test game.

Never starts a game, opens a door itself, edits a graph or teleports a bot.
Use only after Studio has installed the matching generated WAY/proof.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))
from tce_nav_livecheck import (INSTANCE, Rcon, LogWindow, digest, fresh_screenshot,
                             positions, process_identity, provenance, serial_lock, save, verify_runtime)
import quiet


def door_evidence(text, recipes):
    """A destination arrival alone cannot certify the requested door action."""
    evidence=[]
    for entity,mode in sorted(recipes.items()):
        counts={event:len(re.findall(r'TCE2DOOR '+event+r'\b[^\r\n]*\bdoor='+str(entity)+r'\b',text))
                for event in ('USE','WAIT','TO_SWITCH','RETURN','PASSAGE','EGRESS')}
        evidence.append(dict(entity=entity,mode=mode,events=counts,
                             passed=counts['PASSAGE']>0 and counts['EGRESS']>0 and counts['USE']>0 and
                             (mode==1 or counts['TO_SWITCH']>0 and counts['RETURN']>0)))
    return evidence


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map',default='obj_navstudio_doors')
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--way',type=Path,required=True)
    parser.add_argument('--proof',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--route',type=Path,required=True,help='JSON array of actual native waypoint positions')
    parser.add_argument('--seconds',type=int,default=150)
    parser.add_argument('--port',type=int,default=27986)
    parser.add_argument('--password',default='navgen_local')
    args=parser.parse_args()
    args.report_sha256=args.way_sha256=None
    route=json.loads(args.route.read_text())
    native=json.loads(args.report.read_text())
    if not 1<=len(route)<=32 or any(p not in [n['position'] for n in native['nodes'] if n.get('accepted')] for p in route):
        parser.error('Every destination must be an accepted position in this exact native graph.')
    args.out.mkdir(parents=True,exist_ok=False)
    result=dict(map=args.map,complete=False,claim='Focused door traversal only; no map-series or whole-map claim.',
                route=route,samples=[],door_samples=[],screenshots=[],bot_teleports=False,doors_opened_by_test_tool=False)
    rcon=None;token=None;held=False;logs=[];start=None
    try:
        with serial_lock():
            result['provenance_before']=provenance(args)
            identity=process_identity(args.port);result['identity']=identity
            rcon=Rcon(args.port,args.password,args.out)
            result['runtime']=verify_runtime(rcon,args.map)
            if rcon.cvar('g_gametype')!='7':raise RuntimeError('Focused fixture expects Bodycount GT7.')
            deadline=time.monotonic()+60
            while rcon.cvar('gamestate')!='0':
                if time.monotonic()>deadline:raise RuntimeError('Warmup did not settle.')
                time.sleep(.25)
            held='NAVSTUDIO_AUTHORING_ON' in rcon.command('tce_navgen_authoring on')
            if not held:raise RuntimeError('Round setup hold was not confirmed.')
            token=uuid.uuid4().hex[:16]
            directory=INSTANCE/'navgen';directory.mkdir(exist_ok=True)
            begin=directory/('door-live-'+token+'-begin.gm')
            begin.write_text(quiet.file_source(quiet.begin_script(token)),encoding='utf8')
            if 'NAVSTUDIO_QUIET_OK' not in rcon.command('bot script_runfile homepath/navgen/'+begin.name):
                raise RuntimeError('Native population pause failed.')
            rcon.command('bot kickall');rcon.command('bot script_run Server.MaxBots = -1;')
            door_ids=sorted({n['door_recipe']['entity'] for n in native['nodes'] if n.get('accepted') and n.get('door_recipe')})
            sample=directory/('door-live-'+token+'-status.gm')
            sample.write_text(''.join('print("NAVDOORSTAT",%d,GetEntityStat(GetGameEntityFromId(%d),"tce_door_state"));'%(d,d) for d in door_ids),encoding='ascii')
            # The authoring hold pauses bot input/round changes, not mover thinks.
            # Let finite doors close naturally so this bot proves real USE.
            deadline=time.monotonic()+15
            while time.monotonic()<deadline:
                reply=rcon.command('bot script_runfile homepath/navgen/'+sample.name)
                states={int(a):int(b) for a,b in re.findall(r'NAVDOORSTAT\s+(\d+)\s+(-?\d+)',reply)}
                if all(states.get(d)==0 for d in door_ids):break
                time.sleep(.2)
            else:raise RuntimeError('Fixture doors did not naturally close before the probe.')
            result['doors_initially_closed']=True
            rcon.command('bot addbot 1 1 NavProbe')
            deadline=time.monotonic()+35
            while time.monotonic()<deadline:
                rows=positions(rcon.command('navprobe'))
                if any(r['name']=='NavProbe' and r['health']>0 for r in rows):break
                time.sleep(.3)
            else:raise RuntimeError('Natural bot spawn failed.')
            rcon.command('bot debugbot NavProbe fpinfo')
            rcon.command('tce_navgen_authoring off');held=False
            rcon.command('bot waypoint_view on');rcon.command('cg_omnibotdrawing 1')
            log=LogWindow(INSTANCE/'tce2/etconsole.log',args.out/'console-window.log');logs.append(log)
            program='global NavProbeRoute = {'+', '.join('Vector3(%g,%g,%g)'%tuple(p) for p in route)+'};'
            dispatch=directory/('door-live-'+token+'-route.gm');dispatch.write_text(program,encoding='ascii')
            rcon.command('bot script_runfile homepath/navgen/'+dispatch.name)
            start=time.monotonic();next_shot=0
            while time.monotonic()-start<args.seconds:
                elapsed=time.monotonic()-start
                if rcon.cvar('mapname')!=args.map:raise RuntimeError('Map changed.')
                rows=positions(rcon.command('navprobe'));bot=next((r for r in rows if r['name']=='NavProbe'),None)
                if bot is None or any(r['name'] not in ('NavProbe','NavStudioObserver') for r in rows):
                    raise RuntimeError('Unexpected native population.')
                result['samples'].append(dict(time=round(elapsed,3),bot=bot))
                result['door_samples'].append(dict(time=round(elapsed,3),reply=rcon.command('bot script_runfile homepath/navgen/'+sample.name)))
                log.poll()
                if elapsed>=next_shot and bot['health']>0:
                    result['screenshots'].append(fresh_screenshot(rcon,args.out,len(result['screenshots'])+1,bot))
                    next_shot=elapsed+18
                text=(args.out/'console-window.log').read_text(encoding='latin1')
                if 'NAVPATH DONE' in text or re.search(r'NAVPATH (?:FAILED|TIMEOUT)\b|TCE2DOOR BLOCKED\b',text):break
                time.sleep(.2)
            log.poll();text=(args.out/'console-window.log').read_text(encoding='latin1')
            result['successes']=re.findall(r'NAVPATH SUCCESS[^\r\n]*',text)
            result['failures']=re.findall(r'NAVPATH (?:FAILED|TIMEOUT)[^\r\n]*|TCE2DOOR BLOCKED[^\r\n]*',text)
            result['door_events']=[line for line in text.splitlines() if 'TCE2DOOR ' in line]
            recipes={n['door_recipe']['entity']:n['door_recipe']['mode'] for n in native['nodes']
                     if n.get('accepted') and n.get('door_recipe') and n['door_recipe']['role']==2}
            result['door_evidence']=door_evidence(text,recipes)
            result['complete']=len(result['successes'])==len(route) and not result['failures'] and 'NAVPATH DONE' in text
            result['complete'] &= bool(result['door_evidence']) and all(e['passed'] for e in result['door_evidence'])
            result['provenance_after']=provenance(args)
            result['files_unchanged']=result['provenance_before']['files']==result['provenance_after']['files']
            result['complete'] &= result['files_unchanged']
    except Exception as exc:
        result['error']=str(exc)
    finally:
        result['elapsed_seconds']=round(time.monotonic()-start,3) if start else 0
        for log in logs:log.poll();log.close()
        if token and rcon:
            try:
                current=process_identity(args.port)
                if any(current[k]!=identity[k] for k in ('pid','path','start_ticks')):raise RuntimeError('Owned game changed; no cleanup sent.')
                rcon.command('tce_navgen_authoring on');held=True
                rcon.command('bot kickall')
                restore=INSTANCE/'navgen'/('door-live-'+token+'-restore.gm')
                restore.write_text(quiet.file_source(quiet.restore_script(token)),encoding='utf8')
                reply=rcon.command('bot script_runfile homepath/navgen/'+restore.name)
                result['population_restored']='NAVSTUDIO_QUIET_RESTORED' in reply
                rcon.command('tce_navgen_authoring off');held=False
                result['complete'] &= result['population_restored']
            except Exception as exc:result['cleanup_error']=str(exc);result['complete']=False
        if held and rcon:rcon.command('tce_navgen_authoring off')
        if rcon:rcon.stream.close()
        result['visually_reviewed']=False
        save(args.out/'result.json',result)
    summary={k:result.get(k) for k in ('map','complete','elapsed_seconds','error','failures','door_evidence',
                                     'files_unchanged','population_restored','cleanup_error')}
    summary.update(successes=len(result.get('successes',[])),destinations=len(route),
                   screenshots=len(result['screenshots']),result_file=str(args.out/'result.json'))
    print(json.dumps(summary,indent=2))
    return 0 if result['complete'] else 1


if __name__=='__main__':raise SystemExit(main())
