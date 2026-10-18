"""Execute native USE-gated flag pickup, state observation and capture bodies.

Actual game structs and production functions are compiled. External engine,
score/script/respawn services are controlled stubs; this proves the flag and
observer contract, not engine movement, item proximity or a whole-map mission.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_traversal_reset_native import ROOT

def function(source,name):
    match=re.search(r'^(?:void|int)\s+'+re.escape(name)+r'\s*\([^;]*?\)\s*\{',source,re.M)
    if not match:raise AssertionError('Actual native function missing: '+name)
    at=source.index('{',match.start());end=at+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[match.start():end]


def program():
    bridge=(ROOT/'game/etbot/g_etbot_interface.cpp').read_text()
    kinds=(ROOT/'vendor/omni-bot/source/Omnibot/Common/Omni-Bot_Types.h').read_text()
    state=re.search(r'typedef enum eFlagState\s*\{[^}]+\}\s*FlagState;',kinds).group()
    observed=re.search(r'struct TCEObjectiveObservation\s*\{[^}]+\};',bridge).group()
    bodies=[function((ROOT/'game/g_team.c').read_text(),name) for name in ('Team_TouchOurFlag','Team_TouchEnemyFlag','Pickup_Team')]
    bodies+=[function((ROOT/'game/g_items.c').read_text(),'Touch_Item'),
             function((ROOT/'game/g_trigger.c').read_text(),'Touch_flagonly'),
             function((ROOT/'game/g_trigger.c').read_text(),'Touch_flagonly_multiple')]
    return r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#define FEATURE_OMNIBOT 1
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdarg>
extern "C" {
#include "g_local.h"
#include "g_etbot_interface.h"
}
''' + state + r'''
level_locals_t level;gentity_t g_entities[MAX_GENTITIES];
vmCvar_t g_maxclients,g_gametype,g_gamestate;
#undef _vsnprintf
#undef vsnprintf
int Q_vsnprintf(char *d,int n,const char *f,va_list a){return _vsnprintf(d,n,f,a);}
void QDECL Com_Printf(const char *format,...){}
void QDECL Com_Error(int code,const char *format,...){exit(97);}
#include "etbot/tce_flag_query.h"
''' + observed + r'''
static TCEObjectiveObservation tceObjectiveObserved[MAX_CLIENTS];
''' + function(bridge,'Bot_Event_TCEFlagCapture') + r'''
static int respawns,capturescripts,stockscripts,scores;
void AddScore(gentity_t *e,int value){scores+=value;}
void AddKillScore(gentity_t *e,int value){}
void G_Script_ScriptEvent(gentity_t *e,const char *event,const char *params){
 if(!strcmp(event,"trigger")&&!strcmp(params,"captured"))capturescripts++;
}
void Bot_TeamScriptEvent(int team,const char *event,const char *params){}
void QDECL G_Printf(const char *format,...){}
void QDECL PrintMsg(gentity_t *ent,const char *format,...){}
void Team_ResetFlag(gentity_t *ent){}
void RespawnItem(gentity_t *ent){respawns++;ent->flags&=~FL_NODRAW;ent->r.svFlags&=~SVF_NOCLIENT;ent->r.contents=CONTENTS_TRIGGER;}
int Pickup_Ammo(gentity_t *ent,gentity_t *other){return -1;}
int Pickup_Weapon(gentity_t *ent,gentity_t *other){return 0;}
int Pickup_Health(gentity_t *ent,gentity_t *other){return 0;}
qboolean BG_CanItemBeGrabbed(const entityState_t *ent,const playerState_t *ps,int *skill,int team){return qtrue;}
void G_LogPrintf(const char *fmt,...){}
void G_AddEvent(gentity_t *ent,int event,int parm){}
int G_SoundIndex(const char *name){return 0;}
gentity_t *G_TempEntity(vec3_t origin,int event){return &g_entities[99];}
void G_UseTargets(gentity_t *ent,gentity_t *activator){}
void trap_LinkEntity(gentity_t *ent){ent->r.linked=qtrue;}
void G_FreeEntity(gentity_t *ent){ent->inuse=qfalse;}
#define RED_FLAG 1
#define BLUE_FLAG 2
''' + '\n'.join(bodies) + r'''
int main(){
 static gclient_t client;static gitem_t item;
 for(int red=0;red<2;red++){
  memset(g_entities,0,sizeof(g_entities));memset(&client,0,sizeof(client));memset(&item,0,sizeof(item));
  memset(tceObjectiveObserved,0,sizeof(tceObjectiveObserved));memset(&level,0,sizeof(level));
  level.num_entities=100;level.time=1234;g_maxclients.integer=2;g_gametype.integer=2;g_gamestate.integer=GS_PLAYING;
  gentity_t *flag=&g_entities[70],*actor=&g_entities[1],*cap=&g_entities[80];int owner=-1,powerup=red?PW_REDFLAG:PW_BLUEFLAG;
  item.giType=IT_TEAM;item.giTag=powerup;flag->item=&item;flag->inuse=qtrue;flag->s.eType=ET_ITEM;
  flag->classname=red?"team_CTF_redflag":"team_CTF_blueflag";flag->s.number=70;flag->s.density=1;
  flag->active=qtrue;flag->r.contents=CONTENTS_TRIGGER;flag->entstate=STATE_DEFAULT;
  actor->inuse=qtrue;actor->health=100;actor->client=&client;actor->s.number=1;
  client.sess.sessionTeam=red?TEAM_ALLIES:TEAM_AXIS;client.ps.persistant[PERS_TEAM]=client.sess.sessionTeam;
  assert(TceReadFlagState(flag,&owner)==S_FLAG_AT_BASE&&owner==-1);
  Touch_Item(flag,actor,nullptr); // Same native touching event without F.
  assert(client.ps.powerups[powerup]==0&&TceFlagCarrier(flag)==-1&&flag->s.density==1);
  flag->active=qtrue;client.pers.cmd.buttons=BUTTON_ACTIVATE;
  Touch_Item(flag,actor,nullptr); // Actual native pickup bodies write powerup/flagParent.
  assert(client.ps.powerups[powerup]==INT_MAX&&client.flagParent==70&&flag->s.density==0);
  assert(TceReadFlagState(flag,&owner)==S_FLAG_CARRIED&&owner==1);
  cap->inuse=qtrue;cap->s.number=80;cap->entstate=STATE_DEFAULT;cap->accuracy=10;
  cap->spawnflags=(red?BLUE_FLAG:RED_FLAG)|4;
  Touch_flagonly_multiple(cap,actor,nullptr);
  assert(client.ps.powerups[powerup]==INT_MAX&&tceObjectiveObserved[1].flagCaptureCount==0);
  cap->spawnflags=(red?RED_FLAG:BLUE_FLAG)|4;
  Touch_flagonly_multiple(cap,actor,nullptr);
  assert(client.ps.powerups[powerup]==0&&client.flagParent==70);
  assert(tceObjectiveObserved[1].flagCaptureCount==1&&tceObjectiveObserved[1].flagCaptureTrigger==80&&
         tceObjectiveObserved[1].flagCaptureParent==70&&tceObjectiveObserved[1].flagCaptureTime==1234);
  assert(TceFlagCarrier(flag)==-1&&TceReadFlagState(flag,&owner)==S_FLAG_AT_BASE&&owner==-1);
  Touch_flagonly_multiple(cap,actor,nullptr);assert(tceObjectiveObserved[1].flagCaptureCount==1);
  // A one-shot trigger without KILL_FLAG legitimately keeps the powerup.
  gentity_t *keep=&g_entities[81];*keep=*cap;keep->s.number=81;
  client.ps.powerups[powerup]=INT_MAX;keep->touch=Touch_flagonly;
  keep->spawnflags=red?BLUE_FLAG:RED_FLAG;Touch_flagonly(keep,actor,nullptr);
  assert(tceObjectiveObserved[1].flagCaptureCount==1&&keep->touch==Touch_flagonly);
  keep->spawnflags=red?RED_FLAG:BLUE_FLAG;Touch_flagonly(keep,actor,nullptr);
  assert(client.ps.powerups[powerup]==INT_MAX&&tceObjectiveObserved[1].flagCaptureCount==2&&
         tceObjectiveObserved[1].flagCaptureTrigger==81&&keep->touch==nullptr&&
         keep->nextthink==level.time+FRAMETIME&&keep->think==G_FreeEntity);
  client.ps.powerups[powerup]=0;
  // A stale parent and a different colored powerup cannot invent ownership.
  client.ps.powerups[red?PW_BLUEFLAG:PW_REDFLAG]=INT_MAX;
  assert(TceFlagCarrier(flag)==-1);client.ps.powerups[red?PW_BLUEFLAG:PW_REDFLAG]=0;
  flag->flags|=FL_NODRAW;flag->r.contents=0;
  gentity_t *drop=&g_entities[71];*drop=*flag;drop->s.number=71;drop->s.otherEntityNum=70;
  drop->s.density=1;drop->flags=FL_DROPPED_ITEM;drop->r.svFlags=0;drop->r.contents=CONTENTS_TRIGGER;
  assert(TceReadFlagState(flag,&owner)==S_FLAG_DROPPED&&TceReadFlagState(drop,&owner)==S_FLAG_DROPPED);
  drop->active=qtrue;Touch_Item(drop,actor,nullptr);
  assert(client.flagParent==70&&client.ps.powerups[powerup]==INT_MAX&&TceFlagCarrier(flag)==1);
  actor->health=0;assert(TceFlagCarrier(flag)==-1);actor->health=100;
  client.ps.pm_flags|=PMF_LIMBO;assert(TceFlagCarrier(flag)==-1);client.ps.pm_flags=0;
  static gentity_t before[MAX_GENTITIES];memcpy(before,g_entities,sizeof(before));
  Bot_Event_TCEFlagCapture(-1,80,70);Bot_Event_TCEFlagCapture(1,MAX_GENTITIES,70);Bot_Event_TCEFlagCapture(1,80,MAX_GENTITIES);
  assert(tceObjectiveObserved[1].flagCaptureCount==2&&memcmp(before,g_entities,sizeof(before))==0);
 }
 assert(capturescripts==4&&respawns==2);
 puts("actual USE-gated pickup, real carrier, colored capture and dropped-state lifecycle passed");
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class FlagPickupNativeTests(TestCase):
    def test_native_pickup_state_and_capture_lifecycle_for_both_teams(self):
        with tempfile.TemporaryDirectory(prefix='tce-flag-native-') as folder:
            d=Path(folder);(d/'main.cpp').write_text(program(),encoding='utf8')
            (d/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(FlagPickup LANGUAGES C CXX)\n'
                'add_executable(flag-pickup main.cpp "'+(ROOT/'game/q_shared.c').as_posix()+'")\ntarget_include_directories(flag-pickup PRIVATE "'+(ROOT/'game').as_posix()+'")\n'
                'target_compile_options(flag-pickup PRIVATE /Gy)\ntarget_link_options(flag-pickup PRIVATE /OPT:REF)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(d/'build'),'-A','Win32'],
                            ['cmake','--build',str(d/'build'),'--config','Release'],
                            [str(d/'build/Release/flag-pickup.exe')]):
                result=subprocess.run(command,**options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':main()
