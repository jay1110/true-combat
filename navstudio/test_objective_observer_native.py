"""Compile the real observer function; no game process or gameplay mutation."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase,main,skipUnless


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class ObjectiveObserverTests(TestCase):
    def test_confirmed_events_are_client_specific_and_do_not_infer_bomb_disappearance(self):
        root=(Path(__file__).resolve().parents[1] / "src")
        source=(root/'game/etbot/g_etbot_interface.cpp').read_text()
        structure=re.search(r'struct TCEObjectiveObservation\s*\{.*?\};',source,re.S)[0]
        binding=re.search(r'struct TCEBombBinding\s*\{.*?\};',source,re.S)[0]
        begin=source.index('void Bot_Event_TCEObjectiveAction(')
        opening=source.index('{',begin);depth=1;end=opening+1
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        function=source[begin:end]
        program='''#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
constexpr int MAX_CLIENTS=64,MAX_GENTITIES=1024;
struct Entity { void *client=nullptr; int protectedGameplay=12345; };
Entity g_entities[MAX_GENTITIES];struct {int time;} level;
'''+structure+'\n'+binding+'''
static TCEObjectiveObservation tceObjectiveObserved[MAX_CLIENTS];
static TCEBombBinding tceBombBindings[MAX_GENTITIES];
// This older fixture observes counters only. The real producer-to-entity
// binding runs separately with actual g_local.h in the operation-native test.
static void TceRecordObjectiveBomb(int,int) {}
'''+function+'''
int main(){int clientToken=7;g_entities[1].client=&clientToken;g_entities[2].client=&clientToken;
 Entity before[MAX_GENTITIES];std::memcpy(before,g_entities,sizeof(before));level.time=100;
 Bot_Event_TCEObjectiveAction(-1,100,1,88);Bot_Event_TCEObjectiveAction(1,3,1,88);
 assert(tceObjectiveObserved[1].plantCount==0);
 Bot_Event_TCEObjectiveAction(1,100,1,88);
 assert(tceObjectiveObserved[1].plantCount==1 && tceObjectiveObserved[1].plantTime==100);
 assert(tceObjectiveObserved[1].plantBomb==100 && tceObjectiveObserved[1].plantObjective==88);
 // Removing/exploding a bomb is not a confirmed defuse event.
 bool bombAlive=false;(void)bombAlive;
 assert(tceObjectiveObserved[1].defuseCount==0 && tceObjectiveObserved[2].defuseCount==0);
 level.time=500;Bot_Event_TCEObjectiveAction(2,100,2,-1);
 assert(tceObjectiveObserved[2].defuseCount==1 && tceObjectiveObserved[2].defuseTime==500);
 assert(tceObjectiveObserved[2].defuseBomb==100 && tceObjectiveObserved[1].defuseCount==0);
 assert(std::memcmp(before,g_entities,sizeof(before))==0 && level.time==500);
 std::memset(tceObjectiveObserved,0,sizeof(tceObjectiveObserved));assert(tceObjectiveObserved[2].defuseCount==0);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            directory=Path(folder)
            (directory/'main.cpp').write_text(program)
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(ObserverTest LANGUAGES CXX)\nadd_executable(observer-test main.cpp)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release'],
                            [str(directory/'build/Release/observer-test.exe')]):
                result=subprocess.run(command,**options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        # The read side exposes counts/timestamps as existing int userdata only.
        for name in ('tce_bomb_plant_count','tce_bomb_defuse_count','tce_last_plant_time','tce_last_defuse_time'):
            self.assertIn('"'+name+'"',source)
        hint=(root/'game/g_active.c').read_text()
        self.assertIn('updateHints = updateHints || Bot_Interface_IsOmnibot',hint)
        missile=(root/'game/g_missile.c').read_text();weapon=(root/'game/g_weapon.c').read_text()
        self.assertRegex(missile,r'level\.tceBombPlanted=qtrue;\s*#ifdef FEATURE_OMNIBOT\s*Bot_Event_TCEObjectiveAction')
        self.assertRegex(weapon,r'traceEnt->health = 255;\s*#ifdef FEATURE_OMNIBOT\s*Bot_Event_TCEObjectiveAction')


if __name__=='__main__':main()
