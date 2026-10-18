"""Execute the actual objective script in the existing GameMonkey VM.

In this VM, numeric zero compares equal to null. Native zero state/counters
must be accepted by type, while missing observers must remain rejected.
"""
from pathlib import Path
import unittest
import test_flag_goal_vm as flag_vm
ROOT=flag_vm.ROOT

SCRIPT=ROOT/'src/vendor/omni-bot/tce2-scripts/goals/goal_tceobjective.gm'
BOOT='''
global Clock=0;global Plants=0;global Defuses=0;global BombPlanted=0;
global ObjState=0;global Missing=false;global Holds=0;global RouteCalls=0;
global Mode=5;global BotTeam=1;global BombTeam=2;global BombId=90;
global NativeOperation=2;global Owner=2;global Carrier=1;
global EVENT={PATH_SUCCESS=1};global Priority={Low=1,High=2};global BTN={USE=1,CROUCH=2};
global ENTFLAG={DEAD=1,LIMBO=2};
global GetTime=function(){return Clock;};global GetCvar=function(name){return 1;};
global sleep=function(t){global Clock;Clock+=t;};
global EntityIsValid=function(ent){return ent==80||ent==90;};
global GetGameIdFromEntity=function(ent){return ent;};
global GetEntityStat=function(ent,name){if(name=="tce_objective_state"){return ObjState;}
 if(name=="tce_objective_operation"){return NativeOperation;}
 if(name=="tce_objective_owner"){return Owner;}if(name=="tce_objective_bomb"){return BombId;}
 if(name=="tce_bomb_team"){return BombTeam;}return null;};
global GetEntPosition=function(ent){return {IsZero=function(){return false;}};};
global Pos={IsZero=function(){return false;}};
global NativeGoal={UsePosition=Pos,AimPosition=Pos,ActivationRadius=2,
 GetEntity=function(){return 80;},GetName=function(){return "TCEOBJECTIVE_test";},
 GetPosition=function(){return Pos;},ClearUsePoints=function(){},AddUsePoint=function(pos){}};
global Bot={GetTeam=function(){return BotTeam;},GetGameId=function(){return 11;},
 GetStat=function(name){if(name=="tce_gametype"){return Mode;}
 if(name=="tce_bomb_planted"){return BombPlanted;}
 if(name=="tce_bomb_carrier"){return Carrier;}
 if(name=="tce_bomb_plant_count"){if(Missing){return null;}return Plants;}
 if(name=="tce_bomb_defuse_count"){return Defuses;}
 if(name=="tce_last_plant_objective"){return 80;}
 if(name=="tce_last_defuse_bomb"){return 90;}
 if(name=="tce_cursor_hint"){if(NativeOperation==1){return 3;}if((Mode==2&&BombId>=0)||(Mode==5&&BombPlanted==1)){return 39;}return 10;}return null;},
 HasAnyEntityFlag=function(a,b){return false;},HasTarget=function(){return false;},
 HoldButton=function(button,time){if(button==BTN.USE){global Holds;Holds+=1;
  if((Mode==2&&BombId>=0)||(Mode==5&&BombPlanted==1)){global Defuses;Defuses+=1;}else{global Plants;Plants+=1;}}},ReleaseButton=function(button){}};
global Util={PathFailed=function(goal){},GetPriority=function(){}};
global Goal={};
'''
CONFIG='''
Goal.Bot=Bot;Goal.MapGoal=NativeGoal;
Goal.Finished=function(){this.Done=true;};Goal.BlackboardDelay=function(t){};
Goal.AddAimRequest=function(){};Goal.Stop=function(){};
Goal.MarkInUse=function(){return true;};
Goal.RouteTo=function(goal,radius){global RouteCalls;RouteCalls+=1;return 1;};
Goal.Goto=function(pos,radius){return 1;};
'''


class ObjectiveZeroVMTests(unittest.TestCase):
    run_vm=flag_vm.FlagActualVMTests.run_vm
    @classmethod
    def setUpClass(cls):
        flag_vm.FlagActualVMTests.setUpClass()
    def test_zero_counters_state_and_missing_observer(self):
        for mode,event in ((0,'PLANTED'),(1,'DEFUSED')):
            output=self.run_vm(BOOT,'Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();',
                CONFIG+f'global BombPlanted={mode};Goal.Update();',
                'print("ZERO:"+Holds+":"+RouteCalls+":"+Plants+":"+Defuses);')
            self.assertIn(event,output)
            self.assertIn('ZERO:1:1:'+('1:0' if mode==0 else '0:1'),output)
            self.assertNotIn('NATIVE_OBSERVER_MISSING',output)
        output=self.run_vm(BOOT,'Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();',
            CONFIG+'global Missing=true;Goal.Update();','print("MISSING:"+Holds+":"+RouteCalls);')
        self.assertIn('NATIVE_OBSERVER_MISSING',output)
        self.assertIn('MISSING:0:0',output)

    def test_inactive_planted_target_allows_only_real_opposing_team_defuse(self):
        script='Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();'
        for bomb_team,defender in ((1,2),(2,1)):
            setup=(f'global ObjState=3;global BombPlanted=1;global BombTeam={bomb_team};'
                   f'global BotTeam={defender};')
            output=self.run_vm(BOOT,script,CONFIG+setup+'Goal.Update();',
                'print("STATE3_DEFUSE:"+Holds+":"+RouteCalls+":"+Plants+":"+Defuses);')
            self.assertIn('STATE3_DEFUSE:1:1:0:1',output)
            self.assertIn('TCEOBJECTIVE DEFUSED',output)
        cases=(
            'global ObjState=4;global BombPlanted=1;',
            'global ObjState=3;global BombPlanted=0;',
            'global ObjState=3;global BombPlanted=1;global BotTeam=2;',
            'global ObjState=3;global BombPlanted=1;global BombId=-1;',
            'global ObjState=3;global BombPlanted=1;global BombTeam=0;',
            'global ObjState=3;global BombPlanted=1;global BombTeam=null;',
            'global ObjState=3;global BombPlanted=1;global Mode=2;global BotTeam=2;',
        )
        for setup in cases:
            with self.subTest(setup=setup):
                output=self.run_vm(BOOT,script,CONFIG+setup+'Goal.Update();',
                    'print("BLOCKED:"+Holds+":"+RouteCalls+":"+Plants+":"+Defuses);')
                self.assertIn('BLOCKED:0:0:0:0',output)

    def test_gt2_sabotage_plants_without_carrier_and_defuses_only_own_site_enemy_bomb(self):
        script='Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();'
        for team,owner in((1,2),(2,1)):
            setup=f'global Mode=2;global Carrier=0;global BotTeam={team};global Owner={owner};global BombId=-1;'
            output=self.run_vm(BOOT,script,CONFIG+setup+'NativeGoal.Operation="plant_defuse";Goal.Update();',
                               'print("GT2_PLANT:"+Holds+":"+RouteCalls+":"+Plants+":"+Defuses);')
            self.assertIn('GT2_PLANT:1:1:1:0',output);self.assertIn('TCEOBJECTIVE PLANTED',output)
        # A global bomb at a different site does not prevent native local -1 planting.
        output=self.run_vm(BOOT,script,CONFIG+'global Mode=2;global Carrier=0;global BombId=-1;global BombPlanted=1;Goal.Update();',
                           'print("OTHER_SITE:"+Holds+":"+Plants+":"+Defuses);')
        self.assertIn('OTHER_SITE:1:1:0',output)
        output=self.run_vm(BOOT,script,CONFIG+'global Mode=2;global Carrier=0;global Owner=1;global BombId=90;global BombTeam=2;Goal.Update();',
                           'print("GT2_DEFUSE:"+Holds+":"+Plants+":"+Defuses);')
        self.assertIn('GT2_DEFUSE:1:0:1',output);self.assertIn('TCEOBJECTIVE DEFUSED',output)

    def test_gt2_unknown_mismatched_native_kind_ambiguous_bomb_and_gt5_no_carrier_fail_closed(self):
        script='Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();'
        cases=(
            'global Mode=2;global BombId=-2;',
            'global Mode=2;global BombId=null;',
            'global Mode=2;global BombId=90;global BombTeam=1;',
            'global Mode=2;global BombId=-1;global Owner=1;',
            'global Mode=2;global BombId=-1;global Owner=0;',
            'global Mode=2;global BombId=-1;global NativeOperation=0;',
            'global Mode=2;global BombId=-1;global NativeOperation=1;NativeGoal.Operation="plant_defuse";',
            'global Mode=2;global BombId=-1;NativeGoal.Operation="activate_capture";',
            'global Mode=5;global Carrier=0;',
            'global Mode=2;global BombId=-1;global Missing=true;',
        )
        for setup in cases:
            with self.subTest(setup=setup):
                output=self.run_vm(BOOT,script,CONFIG+setup+'Goal.Update();','print("CLOSED:"+Holds+":"+RouteCalls+":"+Plants+":"+Defuses);')
                self.assertIn('CLOSED:0:0:0:0',output)

    def test_gt2_capture_activation_owner_and_hint_remain_distinct(self):
        script='Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();'
        output=self.run_vm(BOOT,script,CONFIG+'global Mode=2;global Owner=1;global NativeOperation=1;NativeGoal.Operation="activate_capture";'+
            'global sleep=function(t){global Clock;global Owner;Clock+=t;Owner=2;};Goal.Update();',
            'print("CAPTURE:"+Holds+":"+RouteCalls);')
        self.assertIn('CAPTURE:1:1',output);self.assertIn('TCEOBJECTIVE OWNER_CHANGED',output)


if __name__=='__main__':unittest.main()
