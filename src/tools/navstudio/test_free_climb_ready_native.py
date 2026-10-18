"""Compile the actual readiness helper against production player/entity types."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_traversal_reset_native import function

ROOT = Path(__file__).resolve().parents[2]


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FreeClimbReadyTests(TestCase):
    def test_actual_player_state_readiness_and_independent_blockers(self):
        with tempfile.TemporaryDirectory(prefix='tce-free-ready-') as folder:
            directory = Path(folder)
            source = r'''
#define GAMEDLL 1
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "g_local.h"
#include "tce_nav_freeclimb.h"
#include "etbot/tce_nav_freeclimb_query.h"
static pmove_t *pm;
ACTUAL_PERMISSION_HELPER
int main(void){
 gentity_t entity;gclient_t client;playerState_t *ps;int before;
 memset(&entity,0,sizeof(entity));memset(&client,0,sizeof(client));entity.client=&client;ps=&client.ps;
 ps->stats[STAT_HEALTH]=100;ps->groundEntityNum=ENTITYNUM_WORLD;client.pmext.sprintTime=20000;
 assert(TceNavFreeClimbBlockers(&entity,0)==0);
 ps->pm_flags=PMF_DUCKED;assert(TceNavFreeClimbBlockers(&entity,0)==4);ps->pm_flags=0;
 ps->pm_flags=PMF_LADDER;assert(TceNavFreeClimbBlockers(&entity,0)==4);ps->pm_flags=0;
 ps->pm_flags=PMF_RESPAWNED;assert(TceNavFreeClimbBlockers(&entity,0)==4);ps->pm_flags=0;
 ps->pm_flags=PMF_JUMP_HELD;assert(TceNavFreeClimbBlockers(&entity,0)==4);ps->pm_flags=0;
 ps->eFlags=EF_PRONE;assert(TceNavFreeClimbBlockers(&entity,0)==8);ps->eFlags=0;
 ps->stats[STAT_TCE_WEAPON_FLAGS]=0x1000;assert(TceNavFreeClimbBlockers(&entity,0)==16);ps->stats[STAT_TCE_WEAPON_FLAGS]=0;
 ps->stats[STAT_TCE_FLAGS]=0x800;assert(TceNavFreeClimbBlockers(&entity,0)==32);ps->stats[STAT_TCE_FLAGS]=0;
 ps->weaponstate=9;assert(TceNavFreeClimbBlockers(&entity,0)==64);ps->weaponstate=0;
 ps->pm_time=501;assert(TceNavFreeClimbBlockers(&entity,0)==128);ps->stats[STAT_TCE_FLAGS]=0x40;
 assert(TceNavFreeClimbBlockers(&entity,0)==0);ps->stats[STAT_TCE_FLAGS]=0;ps->pm_time=0;
 client.pmext.sprintTime=2499;assert(TceNavFreeClimbBlockers(&entity,0)==256);client.pmext.sprintTime=2500;
 ps->groundEntityNum=ENTITYNUM_NONE;assert(TceNavFreeClimbBlockers(&entity,0)==2);ps->groundEntityNum=ENTITYNUM_WORLD;
 ps->stats[STAT_HEALTH]=0;assert(TceNavFreeClimbBlockers(&entity,0)==1);ps->stats[STAT_HEALTH]=100;
 assert(TceNavFreeClimbBlockers(NULL,0)==1);
 ps->stats[STAT_TCE_FLAGS]=0x800;entity.r.svFlags=SVF_BOT;
 assert(TceNavFreeClimbBlockers(&entity,0)==32);assert(TceNavFreeClimbBlockers(&entity,1)==0);
 {pmove_t move;playerState_t saved=*ps;memset(&move,0,sizeof(move));move.ps=ps;pm=&move;
  assert(!PM_FreeClimbActorAllowed());move.tceBotFreeClimb=qtrue;assert(PM_FreeClimbActorAllowed());
  assert(memcmp(&saved,ps,sizeof(saved))==0);move.tceBotFreeClimb=qfalse;
  ps->stats[STAT_TCE_FLAGS]=0;assert(PM_FreeClimbActorAllowed());}
 ps->stats[STAT_TCE_FLAGS]=0;entity.r.svFlags=0;
 before=client.pmext.sprintTime;assert(TceNavFreeClimbBlockers(&entity,0)==0);assert(client.pmext.sprintTime==before);
 return 0;
}
'''
            source = source.replace('ACTUAL_PERMISSION_HELPER', function((ROOT/'game/bg_pmove.c').read_text(), 'PM_FreeClimbActorAllowed'))
            (directory/'main.c').write_text(source)
            (directory/'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FreeClimbReady LANGUAGES C)\n'
                f'add_executable(free-ready main.c)\ntarget_include_directories(free-ready PRIVATE "{(ROOT/"game").as_posix()}")\n')
            options = dict(capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result = subprocess.run([str(directory/'build/Release/free-ready.exe')], **options)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__ == '__main__':
    main()
