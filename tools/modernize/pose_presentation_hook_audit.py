#!/usr/bin/env python3
"""Audit scoped presentation hooks/configstring lease without changing runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile
from weapon_regression import function
ROOT=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--installation',type=Path,default=ROOT/'build/navgen-overkill')
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    checks=[]
    def check(name,value):
        checks.append(dict(name=name,passed=bool(value)))
        if not value:raise AssertionError(name)
    text={n:(ROOT/n).read_text() for n in (
        'src/game/bg_public.h','src/game/g_client.c','src/game/g_active.c','src/game/g_save.c',
        'src/cgame/cg_players.c','src/cgame/cg_character.c','src/cgame/cg_main.c',
        'src/cgame/cg_servercmds.c','src/CMakeLists.txt')}
    bg=text['src/game/bg_public.h']
    check('existing SDK boundary39 to64 unchanged',
          bool(re.search(r'#define CS_FILTERCAMS\s+39',bg)) and bool(re.search(r'#define CS_MODELS\s+64',bg)))
    check('no existing SDK configstring macro occupies40..63',not re.search(
        r'^\s*#define\s+CS_\w+\s+(?:4\d|5\d|6[0-3])\b',bg,re.M))
    c=text['src/game/g_client.c']
    connect=function(c,'ClientConnect')
    check('fresh successful connection challenge before userinfo publication',
          connect.index('G_TCEPresentationConnect(clientNum)')<connect.index('ClientUserinfoChanged(clientNum)'))
    user=function(c,'ClientUserinfoChanged')
    check('cached ACK after Info_Validate',user.index('Info_Validate')<user.index('G_TCEPresentationUserinfo'))
    check('nonce comes from server private state',bool(re.search(
        r'Info_SetValueForKey\(countryInfo,\s*"tcep",\s*va\("%08x",\s*\(unsigned\)G_TCEPresentationChallenge',user)))
    check('spawn clears carriers/history','G_TCEPresentationResetClient(ent)' in function(c,'ClientSpawn'))
    disconnect=function(c,'ClientDisconnect')
    check('disconnect invalidates cached ACK','G_TCEPresentationUserinfo(clientNum,"")' in disconnect)
    check('corpse clears phase words','body->s.time = body->s.time2 = 0' in function(c,'CopyToBodyQue'))
    check('save restore clears explicit restored PS carriers','TCE_POSE_COMPACT_LEGS_SLOT' in function(text['src/game/g_save.c'],'ReadClient'))
    end=function(text['src/game/g_active.c'],'ClientEndFrame')
    check('endframe clears before early returns',end.index('G_TCEPresentationClear')<end.index('return;'))
    check('publication follows final SDK converter',end.index('G_TCEPresentationPublish')>end.rindex('BG_PlayerStateToEntityState'))
    init=function(text['src/cgame/cg_main.c'],'CG_Init')
    check('CG private init precedes character registration',init.index('CG_TCEPresentationInit')<init.index('CG_RegisterClients'))
    register=function(text['src/cgame/cg_character.c'],'CG_ParseAnimationFiles')
    check('negative speed markers cached before legacy reducer',
          register.index('CG_TCEPresentationRememberModel')<register.index('CG_CalcMoveSpeeds'))
    modified=function(text['src/cgame/cg_servercmds.c'],'CG_ConfigStringModified')
    check('manifest and own player config update ACK',
          'num==TCE_POSE_PRESENTATION_CS||num==CS_PLAYERS+cg.clientNum' in modified)
    player=function(text['src/cgame/cg_players.c'],'CG_Player')
    check('shared helper precedes guarded old angles',player.index('CG_TCEPresentationPrepare')<player.index('CG_PlayerAngles'))
    check('legacy animation is conditional',bool(re.search(r'if\s*\(!tcePresentation\)\s*\{\s*CG_PlayerAnimation',player)))
    check('canonical scale not repeated',bool(re.search(r'if\s*\(!tcePresentation\)\s*\{\s*for\s*\(i',player)))
    check('submitted body copies playerOrigin', 'VectorCopy(playerOrigin, body.origin)' in player)
    check('ordinary branch takes interpolated origin','VectorCopy(cent->lerpOrigin, playerOrigin)' in player)
    check('canonical head uses frame0',bool(re.search(
        r'if\s*\(tcePresentation\)\s*\{[^}]*head.frame\s*=\s*head.oldframe\s*=\s*0',player)))
    # Source-level body-origin review: #if0/debug comments are not active offsets.
    uncommented=re.sub(r'/\*.*?\*/|//[^\n]*','',player,flags=re.S)
    active=[];disabled=0
    for line in uncommented.splitlines():
        if re.match(r'\s*#\s*if\s+0\b',line):disabled+=1;continue
        if disabled:
            if re.match(r'\s*#\s*if',line):disabled+=1
            elif re.match(r'\s*#\s*endif',line):disabled-=1
            continue
        active.append(line)
    active='\n'.join(active)
    check('no active body component translation offset',
          not re.search(r'body\.origin\s*\[\s*[012]\s*\]\s*[+\-*/]?=',active))
    check('no active proneLegsOffset adjustment','proneLegsOffset' not in active)
    check('public and editor use same explicit shared sources',
          'target_sources(qagame PRIVATE game/bg_tce_pose_presentation.c game/bg_tce_pose_origin.c game/g_tce_pose_presentation.c)' in text['src/CMakeLists.txt']
          and 'cgame/cg_tce_pose_presentation.c game/bg_tce_pose_presentation.c game/bg_tce_pose_origin.c game/bg_tce_pose_clock.c' in text['src/CMakeLists.txt'])
    check('affected shared origin becomes playerOrigin before lighting',bool(re.search(
        r'if\s*\(tcePresentation\)\s*\{\s*VectorCopy\(body.origin, playerOrigin\)',player)))
    begin=player.index('CG_TCEPresentationBeginOrigin')
    check('affected origin scope precedes sprite splash shadow helpers',all(begin<player.index(name) for name in
          ('CG_PlayerSprites(cent)','CG_PlayerSplash(cent)','CG_PlayerShadow(cent')))
    after=re.sub(r'/\*.*?\*/|//[^\n]*','',player[begin:],flags=re.S)
    returns=len(re.findall(r'\breturn\s*;',after))
    restored=len(re.findall(r'CG_TCEPresentationEndOrigin\(cent,\s*&presentationOrigin\);\s*return\s*;',after))
    check('every post-scope return restores original interpolation/cache',returns==4 and restored==returns)
    check('normal render completion restores original interpolation/cache',bool(re.search(
        r'CG_TCEPresentationEndOrigin\(cent,\s*&presentationOrigin\);\s*\}$',player)))
    scripts=[];suspects=[]
    for folder in ('tce2','etmain'):
        for archive in sorted((a.installation/folder).glob('*.pk3')):
            with zipfile.ZipFile(archive) as z:
                for name in z.namelist():
                    if Path(name).suffix.lower() not in ('.lua','.gm','.script','.cfg'):continue
                    data=z.read(name)
                    scripts.append(dict(archive=archive.name,name=name,sha256=hashlib.sha256(data).hexdigest()))
                    source=data.decode('latin1')
                    if re.search(r'(?:SetConfigstring|GetConfigstring|ConfigString)\s*\(\s*(?:40|0x28)\b',source,re.I):
                        suspects.append(dict(archive=archive.name,name=name))
    check('supplied installation scripts have no literal40 configstring use',not suspects)
    report=dict(checks=checks,scanned_script_entries=len(scripts),scripts=scripts,
        source_hashes={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in text},
        dynamic_index_review=[
            'G_FindConfigstringIndex callers start at SDK CS_MODELS64 or later indexed ranges.',
            'G_SetConfigStringValue callers use CS_OID_DATA plus bounded objective index.',
            'G_SetEntState ent->count config index belongs to ET_WOLF_OBJECTIVE populated by objective spawner.',
            'Lua ConfigGet/ConfigSet intentionally permits arbitrary valid indices. New private40 lease is scoped to supplied source/scripts; no claim of immunity to arbitrary extensions.',
            'CG_Player MG42/AAGUN/mounted origin overrides are explicitly unsupported in affected publisher/consumer.',
            'Remote affected translation uses a shared evaluator with immutable captured ground-mover/ancestor trajectories, preserving player interpolation before no-next mover adjustment; synthetic/source controls remain separate from actual moving-platform play.',
            'Own body translation remains predicted by approved policy; shared authoritative phases and axes do not replace own prediction.'])
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(dict(checks=len(checks),script_entries=len(scripts),failures=0)))
if __name__=='__main__':main()
