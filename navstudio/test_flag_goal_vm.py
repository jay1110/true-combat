"""Run the real FLAG script in the existing offline Omni GameMonkey VM."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
DRIVER=ROOT/'build/navstudio-gmvm/gm-quiet-vm.exe'
SCRIPT=ROOT/'vendor/omni-bot/tce2-scripts/goals/goal_flag.gm'
DEFINITION=ROOT/'vendor/omni-bot/tce2-global-scripts/mapgoals/mapgoal_flag.gm'
BOOTSTRAP='''
global Clock=0;global RouteCalls=0;global GotoCalls=0;global RawMoves=0;global Failures=0;
global RouteResult=1;global GotoResult=1;global Pickup=false;global GMMode=2;
global UseCalls=0;global UseReleases=0;global UsePicks=true;global NativeCaptures=0;
global CaptureOnGoto=false;global DropOnGoto=false;global KeepFlagOnCapture=false;
global BTN={USE=1};
global EVENT={PATH_SUCCESS=1};global Priority={Low=1,High=2};
global GetCvar=function(name){return 0;};global GetTime=function(){return Clock;};
global sleep=function(time){global Clock;Clock+=time;};
global Vec3=function(x,y,z){return {x=x,y=y,z=z};};global Vector3=Vec3;
global Util={RouteTo=function(goal){global RouteCalls;RouteCalls+=1;},
 PathFailed=function(goal){global Failures;Failures+=1;},ScriptDebugPrint=function(a,b){}};
global NativeGoal={TCECheckedUse=1,UsePosition=Vec3(1520,1312,0.25),
 FlagBasePosition=Vec3(1512,1312,-24.875),LivePosition=Vec3(1512,1312,-24.875),
 ActivationRadius=2,State=1,Points=0,Relative=null,
 GetGoalState=function(){return this.State;},GetPosition=function(){return this.LivePosition;},
 GetEntity=function(){return 291;},GetName=function(){return "FLAG_snow";},
 GetRadius=function(){return 2;},
 ClearUsePoints=function(){this.Points=0;},
 AddUsePoint=function(pos,relative){this.Points+=1;this.Point=pos;this.Relative=relative;}};
global Bot={GetStat=function(name){if(name=="tce_gametype"){return GMMode;}if(name=="tce_flag_capture_count"){return NativeCaptures;}return null;},CanGrabItem=function(ent){return !Pickup;},
 IsCarryingFlag=function(){return Pickup;},GetGameId=function(){return 11;},
 HoldButton=function(button,time){global UseCalls;UseCalls+=1;if(UsePicks){global Pickup;Pickup=true;}},
 ReleaseButton=function(button){global UseReleases;UseReleases+=1;},
 MoveTowards=function(pos){global RawMoves;RawMoves+=1;global Pickup;Pickup=true;}};
global Goal={};
'''
CONFIGURE='''
Goal.MapGoal=NativeGoal;Goal.GoalState=Goal.GETTING_FLAG;Goal.Bot=Bot;
Goal.Finished=function(){this.FinishedCount=1;};Goal.BlackboardDelay=function(t){this.Delay=t;};
Goal.AddAimRequest=function(priority,kind,pos){this.Aim=kind;};
Goal.MarkInUse=function(){this.Marked=true;return true;};
Goal.Stop=function(){this.Stopped=true;};
Goal.RouteTo=function(goal,radius){global RouteCalls;RouteCalls+=1;return RouteResult;};
Goal.Goto=function(pos,radius){global GotoCalls;GotoCalls+=1;this.FinalPos=pos;this.FinalRadius=radius;
 if(this.GoalState==this.CARRYING_TO_CAP && GotoResult==1 && (CaptureOnGoto||DropOnGoto)){
 global Pickup;if(!CaptureOnGoto || !KeepFlagOnCapture){Pickup=false;}
 if(CaptureOnGoto){global NativeCaptures;NativeCaptures+=1;}}
 return GotoResult;};
'''

class FlagActualVMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name!='nt' or not DRIVER.is_file():raise unittest.SkipTest('Existing offline Omni-bot VM driver required.')
        data=json.loads(DRIVER.with_name('build.json').read_text())
        if data['executable_sha256']!=hashlib.sha256(DRIVER.read_bytes()).hexdigest():
            raise AssertionError('Existing GM driver does not match its build receipt.')

    def run_vm(self,*scripts):
        with tempfile.TemporaryDirectory(prefix='tce-flag-vm-') as directory:
            files=[]
            for i,source in enumerate(scripts):
                file=Path(directory)/f'{i}.gm';file.write_text(source,encoding='ascii');files.append(str(file))
            result=subprocess.run([str(DRIVER),*files],capture_output=True,text=True,timeout=10,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            return result.stdout

    def goal(self,setup='',probe=''):
        return self.run_vm(BOOTSTRAP,'Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();',
            CONFIGURE+setup+'Goal.Update();',probe or 'local no_probe=0;')

    def test_checked_floor_routes_absolute_point_then_stops_without_raw_move(self):
        output=self.goal(probe='''print("CHECK:"+RouteCalls+":"+GotoCalls+":"+RawMoves+":"+NativeGoal.Points+":"+NativeGoal.Relative+":"+Goal.FinalRadius+":"+Goal.Stopped+":"+Goal.Delay);
print("TARGET:"+Goal.FinalPos.x+":"+Goal.FinalPos.z);''')
        self.assertIn('CHECK:1:1:0:1:0:2:1:null',output)
        self.assertIn('TARGET:1520:0.25',output)

    def test_real_use_input_is_required_and_released_after_pickup_or_timeout(self):
        output=self.goal(probe='print("USE:"+UseCalls+":"+UseReleases+":"+Pickup+":"+Goal.TCEUseHeld);')
        self.assertIn('USE:1:1:1:0',output)
        output=self.goal('global UsePicks=false;',
            'print("NO_USE_PICKUP:"+(UseCalls>0)+":"+UseReleases+":"+Pickup+":"+Goal.Delay);')
        self.assertIn('NO_USE_PICKUP:1:1:0:10',output)

    def test_carrier_follows_cap_route_and_success_requires_native_capture_counter(self):
        trace='global GetCvar=function(name){return 1;};global Pickup=true;Goal.GoalState=Goal.CARRYING_TO_CAP;'
        output=self.goal(trace+'global CaptureOnGoto=true;',
            'print("CAP:"+RouteCalls+":"+GotoCalls+":"+NativeCaptures+":"+Pickup+":"+UseCalls+":"+Goal.FinalRadius);')
        self.assertIn('CAP:1:1:1:0:0:2',output)
        self.assertIn('CAPTURED',output)
        output=self.goal(trace+'global DropOnGoto=true;')
        self.assertIn('CARRY_LOST_WITHOUT_CAPTURE',output)
        self.assertNotIn('TCEFLAG CAPTURED',output)
        output=self.goal(trace,'print("STILL_CARRYING:"+Pickup+":"+Goal.Delay+":"+NativeCaptures);')
        self.assertIn('STILL_CARRYING:1:10:0',output)
        self.assertIn('NO_CAPTURE',output)

    def test_cap_route_failure_does_not_invent_capture_or_clear_carrier(self):
        output=self.goal('global Pickup=true;Goal.GoalState=Goal.CARRYING_TO_CAP;global RouteResult=0;',
            'print("CAP_FAIL:"+RouteCalls+":"+GotoCalls+":"+Pickup+":"+NativeCaptures+":"+Failures);')
        self.assertIn('CAP_FAIL:1:0:1:0:1',output)

    def test_real_capture_succeeds_when_native_trigger_intentionally_keeps_flag(self):
        output=self.goal('global GetCvar=function(name){return 1;};global Pickup=true;'
            'Goal.GoalState=Goal.CARRYING_TO_CAP;global CaptureOnGoto=true;global KeepFlagOnCapture=true;',
            'print("KEEP_FLAG:"+Pickup+":"+NativeCaptures+":"+Goal.Delay+":"+Clock);')
        self.assertIn('KEEP_FLAG:1:1:null:0',output)
        self.assertIn('TCEFLAG CAPTURED',output)
        self.assertNotIn('NO_CAPTURE',output)
        self.assertNotIn('CARRY_LOST_WITHOUT_CAPTURE',output)

    def test_route_failure_never_continues_into_final_movement(self):
        output=self.goal('global RouteResult=0;', 'print("FAIL:"+RouteCalls+":"+GotoCalls+":"+RawMoves+":"+Failures+":"+Goal.FinishedCount);')
        self.assertIn('FAIL:1:0:0:1:1',output)
        output=self.goal('global GotoResult=0;', 'print("FAIL:"+RouteCalls+":"+GotoCalls+":"+RawMoves+":"+Failures+":"+Goal.FinishedCount);')
        self.assertIn('FAIL:1:1:0:1:1',output)

    def test_moved_base_is_stale_not_a_license_to_chase_solid_center(self):
        output=self.goal('NativeGoal.LivePosition=Vec3(1560,1312,-24.875);',
            'print("STALE:"+RouteCalls+":"+GotoCalls+":"+RawMoves+":"+Goal.Delay);')
        self.assertIn('STALE:0:0:0:10',output)

    def test_ordinary_et_and_unchecked_base_keep_original_dynamic_route(self):
        for setup in ('NativeGoal.TCECheckedUse=0;', 'global GMMode=null;', 'global GMMode=null;NativeGoal.State=2;'):
            output=self.goal(setup,'print("ORIGINAL:"+RouteCalls+":"+GotoCalls+":"+RawMoves);')
            self.assertIn('ORIGINAL:1:0:1',output)
        output=self.goal('NativeGoal.State=2;NativeGoal.Points=1;','print("POINTS:"+NativeGoal.Points);')
        self.assertIn('POINTS:0',output)
        # A missing shared TC contact helper is a closed failure, never the
        # unsafe raw dynamic item centre path from the historical behavior.
        output=self.goal('NativeGoal.State=2;','print("NO_HELPER:"+RouteCalls+":"+GotoCalls+":"+RawMoves+":"+Goal.Delay);')
        self.assertIn('NO_HELPER:0:0:0:10',output)

    def test_definition_loads_and_serializes_only_opted_in_properties(self):
        schema='''global InternalGoalState={FlagState=1};
global Validate={NumRange=function(a,b){return {Default=function(v){return v;}};},Type=function(t){return {Default=function(v){return v;}};}};
global Schema=function(){return {Check=function(target){foreach(key and value in this){if(key!="Check" && target[key]==null){target[key]=value;}}return true;}};};
global Definition={};'''
        output=self.run_vm(BOOTSTRAP,schema,'Definition.Define=function(){\n'+DEFINITION.read_text()+'\n};Definition.Define();',
            '''local plain={Version=0};Definition.UpgradeVersion(plain);local saved={};Definition.SaveToTable(saved);
print("DEFAULT:"+Definition.TCECheckedUse+":"+tableCount(saved)+":"+plain.Version);
local props={Version=1,TCECheckedUse=1,UsePosition=Vec3(1520,1312,0.25),FlagBasePosition=Vec3(1512,1312,-24.875),ActivationRadius=2};
Definition.UpgradeVersion(props);Definition.SaveToTable(saved);print("LOADED:"+saved.TCECheckedUse+":"+saved.UsePosition.x+":"+saved.FlagBasePosition.z+":"+saved.ActivationRadius);''')
        self.assertIn('DEFAULT:0:0:1',output)
        self.assertIn('LOADED:1:1520:-24.875:2',output)

if __name__=='__main__':unittest.main()
