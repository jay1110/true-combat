"""Actual shared item contact and full FLAG goal in the existing real GM VM.

Independent analytic native queries prove the recorded floating-centre class.
This is script control/ownership evidence, not a native PMove/live pickup claim.
"""
from pathlib import Path
import unittest
import test_flag_goal_vm as flag_vm
from test_item_contact_goal_vm import BOOT, CONFIG, PICKUP

FLAG=flag_vm.SCRIPT
# World-AABB subtraction at the recorded large fractional coordinates has
# normal float roundoff. The fixture checks the unchanged full16/30/57 hull
# to float precision, rather than requiring bit-exact subtraction of world Z.
BODY_BOOT=BOOT.replace('body.mins.x!=-16 || body.mins.z!=-30 || body.maxs.z!=57',
    'Abs(body.mins.x+16)>0.0005 || Abs(body.mins.z+30)>0.0005 || Abs(body.maxs.z-57)>0.0005')

CONFIGURE=CONFIG+r'''
global Carrying=false;global OtherCarrier=false;global CanUse=true;
global MoveAfterUse=false;global WaterAfterUse=false;global PickupAfterUse=true;
global CarrierOnGoto=false;global CarrierAfterUse=false;global DeleteAfterUse=false;global MarkAllowed=true;
global RouteCount=0;global GotoCount=0;global LastRadius=null;global PriorCenterRoutes=0;
global Util={PathFailed=function(g){},ScriptDebugPrint=function(a,b){},
 RouteTo=function(g){global PriorCenterRoutes;PriorCenterRoutes+=1;}};
global Priority={Low=1,High=3};
Bot.IsCarryingFlag=function(){return Carrying;};
Bot.CanGrabItem=function(ent){return !Deleted && !OtherCarrier && CanUse;};
Bot.HoldButton=function(button,time){global UseCalls;UseCalls+=1;
 if(PickupAfterUse){global Carrying;Carrying=true;NativeGoal.State=3;}
 if(MoveAfterUse){global Item;Item=Vec3(Item.x+50,Item.y,Item.z);}
 if(WaterAfterUse){global Water;Water=1;}
 if(CarrierAfterUse){global OtherCarrier;OtherCarrier=true;NativeGoal.State=3;}
 if(DeleteAfterUse){global Deleted;Deleted=true;NativeGoal.State=0;}};
NativeGoal.Points=1;NativeGoal.ClearUsePoints=function(){this.Points=0;};
NativeGoal.GetName=function(){return "FLAG_flagspecops_dropped";};
NativeGoal.GetRadius=function(){return 2;};
NativeGoal.TCECheckedUse=0;
Goal.Bot=Bot;Goal.MapGoal=NativeGoal;Goal.GoalState=Goal.GETTING_FLAG;
Goal.Finished=function(){this.Done=true;};Goal.BlackboardDelay=function(t){this.Delay=t;};
Goal.Stop=function(){this.Stopped=true;};Goal.AddAimRequest=function(p,k,t){this.Aim=t;};
Goal.MarkInUse=function(){return MarkAllowed;};
Goal.RouteTo=function(g,r){global RouteCount;RouteCount+=1;return RouteResult;};
Goal.Goto=function(pos,radius){global GotoCount;GotoCount+=1;global LastRadius;LastRadius=radius;
 this.Dest=pos;if(RouteResult==1){global Pos;Pos=pos;}
 if(Changed){global Item;Item=Vec3(Item.x+20,Item.y,Item.z);}
 if(CarrierOnGoto){global OtherCarrier;OtherCarrier=true;}
 if(OtherCarrier){NativeGoal.State=3;}
 return RouteResult;};
// Native Village full body: floor128, feet30 below player origin. Item AABB
// centre191.984 is33.658u above the recorded actor, outside a32u goal sphere.
global Floor=128.0;global Pos=Vec3(-127.986,-1108.529,158.326);
global Item=Vec3(-143.716,-1091.623,191.984);
'''


class DroppedFlagContactVMTests(unittest.TestCase):
    setUpClass=classmethod(flag_vm.FlagActualVMTests.setUpClass.__func__)
    run_vm=flag_vm.FlagActualVMTests.run_vm

    def goal(self,setup='',probe=''):
        return self.run_vm(BODY_BOOT,
            'PickupGoal.Define=function(){\n'+PICKUP.read_text()+'\n};PickupGoal.Define();',
            'global Goal={};Goal.Define=function(){\n'+FLAG.read_text()+'\n};Goal.Define();',
            CONFIGURE+setup+'Goal.Update();',probe or 'local no_probe=0;')

    def test_recorded_floating_center_routes_supported_body_then_uses_real_carry_transition(self):
        out=self.goal(probe='print("PICKUP:"+RouteCount+":"+GotoCount+":"+LastRadius+":"+Goal.Dest.z+":"+UseCalls+":"+Releases+":"+Carrying+":"+NativeGoal.Points+":"+PriorCenterRoutes);')
        self.assertIn('PICKUP:0:1:2:158.326:1:1:1:0:0',out)
        self.assertIn('PICKED_UP',out);self.assertNotIn('BAD_NATIVE_HULL',out)

    def test_floor_water_body_block_and_unreachable_route_never_use_or_claim_pickup(self):
        for setup,event in (('global Water=1;','NO_SAFE_CONTACT'),('global NoFloor=true;','NO_SAFE_CONTACT'),
                            ('global Solid=true;','NO_SAFE_CONTACT'),('global Grounded=false;','NO_SAFE_CONTACT'),
                            ('global RouteResult=0;','ROUTE_FAILED')):
            with self.subTest(setup=setup):
                out=self.goal(setup,'print("CLOSED:"+UseCalls+":"+Carrying+":"+Goal.Delay+":"+PriorCenterRoutes);')
                self.assertIn('CLOSED:0:0:10:0',out);self.assertIn(event,out);self.assertNotIn('PICKED_UP',out)

    def test_moving_deleted_or_changed_owner_is_not_own_pickup(self):
        for setup in ('global Changed=true;','global Deleted=true;','global OtherCarrier=true;',
                      'global CarrierOnGoto=true;','global MarkAllowed=false;'):
            with self.subTest(setup=setup):
                out=self.goal(setup,'print("STALE:"+UseCalls+":"+Carrying+":"+PriorCenterRoutes);')
                self.assertIn('STALE:0:0:0',out);self.assertNotIn('PICKED_UP',out)

    def test_every_use_tick_rechecks_contact_and_releases_after_failure_or_timeout(self):
        for setup in ('global MoveAfterUse=true;','global WaterAfterUse=true;',''):
            with self.subTest(setup=setup):
                out=self.goal('global PickupAfterUse=false;'+setup,
                    'print("BOUNDED:"+(Clock<=3.1)+":"+(UseCalls<=31)+":"+Releases+":"+Carrying);')
                self.assertIn('BOUNDED:1:1:1:0',out);self.assertNotIn('PICKED_UP',out)

    def test_other_carrier_or_deletion_during_use_does_not_become_own_success(self):
        for setup in ('global CarrierAfterUse=true;','global DeleteAfterUse=true;'):
            with self.subTest(setup=setup):
                out=self.goal('global PickupAfterUse=false;'+setup,
                    'print("OTHER:"+UseCalls+":"+Releases+":"+Carrying+":"+Goal.Done);')
                self.assertIn('OTHER:1:1:0:1',out);self.assertNotIn('PICKED_UP',out)
                self.assertIn('DROPPED_STATE_CHANGED',out)

    def test_failure_releases_previously_held_use_and_never_runs_raw_route(self):
        out=self.goal('Goal.TCEUseHeld=true;global RouteResult=0;',
            'print("RELEASE:"+UseCalls+":"+Releases+":"+Goal.TCEUseHeld+":"+PriorCenterRoutes);')
        self.assertIn('RELEASE:0:1:0:0',out)


if __name__=='__main__':unittest.main()
