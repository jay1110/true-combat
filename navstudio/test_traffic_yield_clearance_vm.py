"""Actual TrafficYield GM over independently swept, moving two-player hulls.

The offline VM executes the unchanged production choice/Stop/MoveTowards loop.
The fixture supplies a flat supported corridor and explicit body collisions;
this is not a substitute for ET PMove or a live-map qualification.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / 'build/navstudio-gmvm/gm-quiet-vm.exe'
SCRIPT = ROOT / 'vendor/omni-bot/tce2-scripts/goals/goal_checkstuck.gm'
NATIVE_DRIVER = ROOT / 'build/navstudio-aabb-gmvm/gm-aabb-vm.exe'

BOOTSTRAP = r'''
global Clock=0;global Delta=.05;global Goal={};global Actors={};global Steps=0;
global ENTFLAG={DEAD=1,LIMBO=2,ON_LADDER=3,ON_GROUND=4};
global BTN={JUMP=1,STRAFE_L=2,STRAFE_R=3};
global TRACE={SOLID=1,PLAYERCLIP=2,PLAYER=4,NAVCAPSULE=8};
global CONTENT={WATER=1,SLIME=2,LAVA=4,MOVER=8,TRIGGER=16};
global GetTime=function(){return Clock;};global GetMapName=function(){return "obj_pair_fixture";};
global GetCvar=function(n){return 1;};global GetPointContents=function(p){return 0;};
global GetEntFlags=function(e,f){return f==ENTFLAG.ON_GROUND;};
global GetEntPosition=function(e){return Actors[e].Pos;};
global GetEntTeam=function(e){return Actors[e].Team;};global GetGameIdFromEntity=function(e){return e;};
global GetEntFacing=function(e){return Vec3(0,-1,0);};
global AABB=function(a,b){return {Mins=a,Maxs=b};};
global GetEntWorldAABB=function(e){return {Entity=e,
 CenterPoint=function(){return Actors[this.Entity].Pos+Vec3(0,0,15.5);},
 GetAxisLength=function(a){if(a=="z"){return 89;}return Actors[this.Entity].Half*2+2;}};};
// A body moving past an equal-height player must clear both lateral extents.
// Slab sweeps include contact epsilon and do not release a blocker by a timer.
global TraceLine=function(a,b,box,mask,ignore,pvs){
 result={startsolid=0,fraction=1,end=b,normal=Vec3(0,0,1),entity=null};
 if(b.z<a.z-1){result.fraction=.5;result.end=Vec3(b.x,b.y,0);return result;}
 if(!(mask&TRACE.PLAYER)){return result;}
 foreach(id and other in Actors){if(id!=ignore){
  extent=Actors[ignore].Half+other.Half+.125;
  low=0;high=1;possible=1;
  for(axis=0;axis<2;axis+=1){
   av=a.x;bv=b.x;ov=other.Pos.x;if(axis==1){av=a.y;bv=b.y;ov=other.Pos.y;}
   dv=bv-av;
   if(dv<.000001&&dv>-.000001){if(av<=ov-extent||av>=ov+extent){possible=0;}}
   else{
    t1=(ov-extent-av)/dv;t2=(ov+extent-av)/dv;
    if(t1>t2){temp=t1;t1=t2;t2=temp;}
    if(t1>low){low=t1;}if(t2<high){high=t2;}
   }
  }
  if(possible&&low<high&&high>0&&low<1){result.fraction=low;result.entity=id;return result;}
 }}return result;
};
global MakeBot=function(id,p){return {Id=id,Pos=p,Half=16,Team=1,Target=p,Tolerance=4,
 GetGameEntity=function(){return this.Id;},GetGameId=function(){return this.Id;},
 GetPosition=function(){return this.Pos;},GetTeam=function(){return this.Team;},
 HasAnyEntityFlag=function(a,b,c){return 0;},HasEntityFlag=function(f){return f==ENTFLAG.ON_GROUND;},
 ReleaseButton=function(a,b,c){},ResetStuckTime=function(){},
 MoveTowards=function(p,r){this.Target=p;this.Tolerance=r;}};};
global sleep=function(t){
 global Clock;global Steps;
 for(elapsed=0;elapsed<t-.00001;elapsed+=Delta){
  foreach(id and bot in Actors){
   d=bot.Target-bot.Pos;distance=d.Length();
   if(distance>bot.Tolerance){
    step=204*Delta;if(step>distance-bot.Tolerance){step=distance-bot.Tolerance;}
    destination=bot.Pos+d.Normalize()*step;
    sweep=TraceLine(bot.Pos,destination,AABB(Vec3(-bot.Half,-bot.Half,-30),Vec3(bot.Half,bot.Half,57)),15,id,0);
    if(sweep.fraction>=1){bot.Pos=destination;Steps+=1;}
   }
  }Clock+=Delta;
 }
};
'''


def run_pair(source=None,native=False):
    driver=NATIVE_DRIVER if native else DRIVER
    receipt = json.loads(driver.with_name('build.json').read_text())
    assert hashlib.sha256(driver.read_bytes()).hexdigest() == receipt['executable_sha256']
    bootstrap=BOOTSTRAP
    if native:
        begin=bootstrap.index('global AABB=function(')
        end=bootstrap.index('// A body moving past',begin)
        bootstrap=bootstrap[:begin]+bootstrap[end:]
        bootstrap=bootstrap.replace('Clock+=Delta;','Clock+=Delta;NativeGC();')
    definition = 'Goal.Define=function(){\n' + (source or SCRIPT.read_text()) + '\n};Goal.Define();'
    replay = r'''
Actors[3]=MakeBot(3,Vec3(0,0,30));Actors[5]=MakeBot(5,Vec3(0,32.125,30));
Goal.Bot=Actors[5];Goal.Stop=function(){};
Goal.TrafficYield(3);
print("YIELDED:"+Actors[5].Pos.x+":"+Actors[5].Pos.y+":"+Steps+":"+Clock);
Actors[3].Target=Vec3(0,96,30);Actors[3].Tolerance=2;
sleep(1);
print("PASSED:"+Actors[3].Pos.y+":"+Actors[5].Pos.x);
'''
    with tempfile.TemporaryDirectory(prefix='tce-traffic-pair-vm-') as directory:
        files=[]
        for index,text in enumerate((bootstrap,definition,replay)):
            path=Path(directory)/(str(index)+'.gm');path.write_text(text,encoding='ascii');files.append(str(path))
        result=subprocess.run([str(driver),*files],capture_output=True,text=True,timeout=10,
                              creationflags=subprocess.CREATE_NO_WINDOW)
    return result


@unittest.skipUnless(os.name=='nt' and DRIVER.is_file(),'Existing offline Omni GM VM required.')
class TrafficYieldClearanceVMTests(unittest.TestCase):
    def test_actual_two_moving_body_yield_clears_the_other_path(self):
        result=run_pair();self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        yielded=re.search(r'YIELDED:([-\d.]+):([-\d.]+):(\d+):([-\d.]+)',result.stdout)
        passed=re.search(r'PASSED:([-\d.]+):([-\d.]+)',result.stdout)
        self.assertIsNotNone(yielded,result.stdout);self.assertIsNotNone(passed,result.stdout)
        self.assertGreater(int(yielded[3]),0,'The yielding player must really move.')
        self.assertGreaterEqual(abs(float(yielded[1])),32.125,result.stdout)
        self.assertGreater(float(passed[1]),64,result.stdout)
        self.assertLessEqual(float(yielded[4]),.4,result.stdout)

    def test_historical_32_unit_target_reproduces_real_tolerance_deadlock(self):
        # Only the old target distance is substituted; actual production
        # choice, hull sweeps, MoveTowards(4) and moving actor loop still run.
        text=SCRIPT.read_text()
        text=re.sub(r'offset = side \* [^;]+;', 'offset = side * 32;',text)
        text=re.sub(r'offset = -offset;', 'offset = -offset;',text)
        result=run_pair(text);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        passed=re.search(r'PASSED:([-\d.]+):([-\d.]+)',result.stdout)
        self.assertIsNotNone(passed,result.stdout)
        self.assertLess(float(passed[1]),32.125,result.stdout)


if __name__=='__main__':unittest.main()
