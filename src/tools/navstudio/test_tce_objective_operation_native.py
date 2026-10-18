"""Actual bridge entity capability and producer-bound bomb query, offline."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_traversal_reset_native import function

ROOT=Path(__file__).resolve().parents[2]


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows native compiler required')
class NativeTCEObjectiveOperationTests(TestCase):
    def test_actual_capability_plant_observer_and_exact_concurrent_site_queries(self):
        source=(ROOT/'game/etbot/g_etbot_interface.cpp').read_text()
        structs='\n'.join(re.search(r'struct '+name+r'\s*\{.*?\};',source,re.S)[0]
                          for name in('TCEObjectiveObservation','TCEBombBinding','BotEntity'))
        functions='\n'.join(function(source,name)for name in('TceObjectiveOperation','TceLiveObjectiveBomb',
            'TceRecordObjectiveBomb','TceExactObjectiveBomb'))
        functions+='\n'+function('static '+source[source.index('void Bot_Event_TCEObjectiveAction('):],'Bot_Event_TCEObjectiveAction')
        register=source[source.index('            if (TceObjectiveOperation(e)) {'):]
        register=register[:register.index('\n\t\t\tswitch(e->s.eType)')]
        program=r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdarg>
extern "C" {
#include "@HEADER@"
gentity_t g_entities[MAX_GENTITIES];level_locals_t level;vmCvar_t g_gametype;
}
#undef vsnprintf
using obint16=short;
''' .replace('@HEADER@',(ROOT/'game/g_local.h').as_posix())+structs+r'''
static TCEObjectiveObservation tceObjectiveObserved[MAX_CLIENTS];
static TCEBombBinding tceBombBindings[MAX_GENTITIES];
static BotEntity m_EntityHandles[MAX_GENTITIES];
'''+functions+r'''
static int registrations=0;
extern "C" void Com_sprintf(char *out,int size,const char *format,...) {
 va_list args;va_start(args,format);vsnprintf(out,size,format,args);va_end(args);
}
enum {ET_TEAM_AXIS=1,ET_TEAM_ALLIES=2};
static void Bot_Util_AddGoal(const char *kind,gentity_t *entity,int teams,const char *name) {
 assert(!strcmp(kind,"TCEOBJECTIVE")&&entity->s.eType==ET_OID_TRIGGER&&teams==6&&name&&*name);++registrations;
}
static void registerOne(int i){gentity_t *e=&g_entities[i];const char *pGoalName="";char tceGoalFallback[64];
'''+register+r'''
}
static gclient_t clients[2];static gentity_t before[MAX_GENTITIES];
static void site(int id,int target,int owner) {
 gentity_t *e=&g_entities[id],*t=&g_entities[target];
 e->inuse=t->inuse=qtrue;e->s.eType=ET_OID_TRIGGER;e->spawnflags=owner;e->target_ent=t;
 t->s.eType=ET_EXPLOSIVE;t->constructibleStats.weaponclass=2;
}
static void bomb(int id,int team) {
 gentity_t *b=&g_entities[id];memset(b,0,sizeof(*b));
 b->inuse=qtrue;b->s.eType=ET_MISSILE;b->s.weapon=15;b->s.teamNum=team;
 b->s.effect1Time=level.time;b->methodOfDeath=MOD_DYNAMITE;b->health=5;
}
int main() {
 level.num_entities=700;level.time=100;g_gametype.integer=2;
 for(int i=0;i<MAX_GENTITIES;++i)m_EntityHandles[i].m_HandleSerial=1;
 g_entities[0].client=&clients[0];g_entities[1].client=&clients[1];
 site(410,411,ALLIED_OBJECTIVE);site(420,421,AXIS_OBJECTIVE);
 assert(TceObjectiveOperation(&g_entities[410])==2);
 registerOne(410);registerOne(420);assert(registrations==2);
 g_entities[411].s.eType=(entityType_t)65;assert(TceObjectiveOperation(&g_entities[410])==1);
 g_gametype.integer=5;assert(!TceObjectiveOperation(&g_entities[410]));
 g_entities[411].s.eType=ET_EXPLOSIVE;assert(TceObjectiveOperation(&g_entities[410])==2);
 g_gametype.integer=7;assert(!TceObjectiveOperation(&g_entities[410]));registerOne(410);assert(registrations==2);
 g_gametype.integer=2;g_entities[411].constructibleStats.weaponclass=1;assert(!TceObjectiveOperation(&g_entities[410]));
 g_entities[411].constructibleStats.weaponclass=2;g_entities[411].inuse=qfalse;assert(!TceObjectiveOperation(&g_entities[410]));
 g_entities[411].inuse=qtrue;g_entities[410].spawnflags=0;assert(!TceObjectiveOperation(&g_entities[410]));
 g_entities[410].spawnflags=ALLIED_OBJECTIVE;
 assert(TceExactObjectiveBomb(&g_entities[410])==-1);
 bomb(600,TEAM_AXIS);memcpy(before,g_entities,sizeof(before));
 Bot_Event_TCEObjectiveAction(0,600,1,410);
 assert(!memcmp(before,g_entities,sizeof(before)));
 assert(TceExactObjectiveBomb(&g_entities[410])==600);
 assert(TceExactObjectiveBomb(&g_entities[420])==-1); // Foreign site's bomb ignored, despite identical positions.
 bomb(601,TEAM_ALLIES);Bot_Event_TCEObjectiveAction(1,601,1,420);
 assert(TceExactObjectiveBomb(&g_entities[410])==600&&TceExactObjectiveBomb(&g_entities[420])==601);
 assert(tceObjectiveObserved[0].plantCount==1&&tceObjectiveObserved[1].plantCount==1);
 bomb(602,TEAM_AXIS);Bot_Event_TCEObjectiveAction(0,602,1,999); // No invented owner from proximity.
 assert(TceExactObjectiveBomb(&g_entities[410])==-2);
 g_entities[602].inuse=qfalse;
 ++m_EntityHandles[600].m_HandleSerial;assert(TceExactObjectiveBomb(&g_entities[410])==-2);
 --m_EntityHandles[600].m_HandleSerial;
 ++m_EntityHandles[411].m_HandleSerial;assert(TceExactObjectiveBomb(&g_entities[410])==-2);
 --m_EntityHandles[411].m_HandleSerial;
 bomb(602,TEAM_AXIS);Bot_Event_TCEObjectiveAction(0,602,1,410);
 assert(TceExactObjectiveBomb(&g_entities[410])==-2); // Two actual plants on one site are ambiguous.
 assert(TceExactObjectiveBomb(&g_entities[420])==601);
 g_entities[602].inuse=qfalse;
 g_entities[600].s.teamNum=TEAM_ALLIES;assert(TceExactObjectiveBomb(&g_entities[410])==-2);
 g_entities[600].s.teamNum=TEAM_AXIS;
 g_entities[600].health=255;Bot_Event_TCEObjectiveAction(1,600,2,-1);
 assert(TceExactObjectiveBomb(&g_entities[410])==-1&&TceExactObjectiveBomb(&g_entities[420])==601);
 assert(tceObjectiveObserved[1].defuseCount==1&&tceObjectiveObserved[0].defuseCount==0);
 memset(tceBombBindings,0,sizeof(tceBombBindings));assert(TceExactObjectiveBomb(&g_entities[420])==-2);
 puts("PASS native capability, real producer, concurrent opposite sites and stale/ambiguous/foreign guards");
}
'''
        with tempfile.TemporaryDirectory(prefix='tce-objective-binding-',dir=ROOT.parent/'build')as folder:
            directory=Path(folder);(directory/'main.cpp').write_text(program)
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(TCEObjective LANGUAGES CXX)\nadd_executable(objective main.cpp)\ntarget_compile_definitions(objective PRIVATE GAMEDLL)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in(['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                           ['cmake','--build',str(directory/'build'),'--config','Release'],
                           [str(directory/'build/Release/objective.exe')]):
                run=subprocess.run(command,**options);self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('PASS native capability',run.stdout)


if __name__=='__main__':main()
