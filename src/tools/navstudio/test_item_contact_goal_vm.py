"""Execute the production item/return goals in the existing real GameMonkey VM.

Engine queries are analytic controlled stubs, not a claim of live pathfinding.
The scripts' complete candidate search, contact proof, state loop and stock ET
branch execute unchanged. The contact interval is derived from g_items.c and
BG_PlayerTouchesItem, with both ambiguous native item origins covered below.
"""
from pathlib import Path
import unittest
import test_flag_goal_vm as flag_vm
ROOT=flag_vm.ROOT

PICKUP=ROOT/'src/vendor/omni-bot/tce2-scripts/goals/goal_pickupitems.gm'
RETURN=ROOT/'src/vendor/omni-bot/tce2-scripts/goals/goal_flagreturn.gm'

BOOT=r'''
global Clock=0;global Mode=2;global UseCalls=0;global Releases=0;global Routes=0;
global RouteResult=1;global RawMoves=0;global Water=0;global NoFloor=false;
global Solid=false;global Ceiling=false;global Slope=1.0;global Slick=0;global Grounded=true;
global Deleted=false;global Disabled=false;global Floor=32.0;global TraceCalls=0;
global Changed=false;global PickAfterUse=true;global ReturnAfterUse=false;global DeleteAfterUse=false;
global Ammo=5;global Clip=2;global ItemHeight=20.0;global ItemWidth=20.0;
global BTN={USE=1,SPRINT=2};global EVENT={PATH_SUCCESS=1,PATH_FAILED=0,ENT_ENTER_RADIUS=3};
global CAT={PICKUP=1,PICKUP_HEALTH=2,PICKUP_AMMO=3,PICKUP_WEAPON=4};
global ENTFLAG={DISABLED=1,DEAD=2,LIMBO=3,ON_LADDER=4,ON_GROUND=5,INWATER=6,UNDERWATER=7};
global CONTENT={WATER=1,SLIME=2,LAVA=4,MOVER=8};
global SURFACE={SLICK=1};
global TRACE={SOLID=1,PLAYERCLIP=2,PLAYER=4,NAVCAPSULE=4096};global Priority={High=3};
global Vec3=function(x,y,z){return {x=x,y=y,z=z};};
global Abs=function(v){if(v<0){return -v;}return v;};
// Only used for ordering fixed candidate points/stale-origin detection here.
global DistanceBetween=function(a,b){return Abs(a.x-b.x)+Abs(a.y-b.y)+Abs(a.z-b.z);};
global AABB=function(mins,maxs){return {mins=mins,maxs=maxs,
 CenterPoint=function(){return Vec3((this.mins.x+this.maxs.x)*0.5,(this.mins.y+this.maxs.y)*0.5,(this.mins.z+this.maxs.z)*0.5);},
 GetAxisLength=function(axis){return this.maxs[axis]-this.mins[axis];}};};
global Pos=Vec3(683.505,72.812,62.125);global Item=Vec3(655,116,43);
global GetTime=function(){return Clock;};global GetCvar=function(name){return 1;};
global sleep=function(t){global Clock;Clock+=t;};
global GetEntFlags=function(ent,flag){return ent==100 && Disabled;};
global GetEntPosition=function(ent){if(Deleted){return null;}return Item;};
global GetEntWorldAABB=function(ent){
 if(ent==1){return AABB(Vec3(Pos.x-17,Pos.y-17,Pos.z-29),Vec3(Pos.x+17,Pos.y+17,Pos.z+60));}
 if(Deleted){return null;}
 return AABB(Vec3(Item.x-ItemWidth*0.5-1,Item.y-ItemWidth*0.5-1,Item.z-ItemHeight*0.5-1),
 Vec3(Item.x+ItemWidth*0.5+1,Item.y+ItemWidth*0.5+1,Item.z+ItemHeight*0.5+1));};
global GetPointContents=function(pos){return Water;};
global TraceLine=function(start,end,body,mask,ent,pvs){
 global TraceCalls;TraceCalls+=1;
 if(!(mask&TRACE.NAVCAPSULE) || body.mins.x!=-16 || body.mins.z!=-30 || body.maxs.z!=57)
 {print("BAD_NATIVE_HULL");return {startsolid=1,fraction=0};}
 if(Solid || (Ceiling && start.z+body.maxs.z>80)){return {startsolid=1,fraction=0};}
 z=Floor-body.mins.z;
 if(end.z<start.z && !NoFloor && start.z>=z && end.z<z)
 {return {startsolid=0,fraction=0.5,end=Vec3(end.x,end.y,z),normal=Vec3(0,0,Slope),surface=Slick};}
 return {startsolid=0,fraction=1};};
global GetEntCategory=function(ent,cat){return cat==CAT.PICKUP_AMMO;};
global GetEntClass=function(ent){return 1;};global GetWeaponIdFromClassId=function(id){return 2;};
global RandInt=function(a,b){return 1;};
global Util={PathFailed=function(goal){goal.Failed=1;},ScriptDebugPrint=function(a,b){},
 GetPriority=function(goal,hash,a,b,callback){goal.Priority=callback(NativeGoal,Bot);}};
global Bot={Health=50,MaxHealth=100,GetStat=function(name){return Mode;},
 GetGameId=function(){return 1;},GetGameEntity=function(){return 1;},GetPosition=function(){return Pos;},
 GetCurrentWeapon=function(){return 2;},GetCurrentAmmo=function(t,mode,weapon){t.CurrentAmmo=Ammo;t.CurrentClip=Clip;return true;},
 HasAnyEntityFlag=function(a,b,c){return false;},HasEntityFlag=function(f){return Grounded;},
 HoldButton=function(button,time){if(button==BTN.USE){global UseCalls;UseCalls+=1;
 if(PickAfterUse){global Ammo;Ammo+=1;}
 if(ReturnAfterUse){NativeGoal.State=1;}
 if(DeleteAfterUse){global Deleted;Deleted=true;NativeGoal.State=0;}}},
 ReleaseButton=function(button){if(button==BTN.USE){global Releases;Releases+=1;}},
 CanGrabItem=function(ent){return !Deleted;},MoveTowards=function(p){global RawMoves;RawMoves+=1;}};
global NativeGoal={State=2,GetGoalState=function(){return this.State;},GetEntity=function(){return 100;},
 GetPosition=function(){return Item;}};
global PickupGoal={Events={},LimitToNoEntityFlag=function(a,b){}};
global ReturnGoal={};
'''

CONFIG=r'''
global Configure=function(goal){goal.Bot=Bot;goal.MapGoal=NativeGoal;goal.TargetEntity=100;
 goal.Finished=function(){this.Done=true;};goal.BlackboardDelay=function(t){this.Delay=t;};
 goal.Stop=function(){this.Stopped=true;};goal.AddAimRequest=function(p,k,t){this.Aim=t;};
 goal.MarkInUse=function(){return true;};goal.MarkInProgress=function(){return true;};
 goal.Goto=function(pos,radius){global Routes;Routes+=1;this.Dest=pos;this.Radius=radius;
  if(RouteResult==1){global Pos;Pos=pos;}
  if(Changed){global Item;Item=Vec3(Item.x+20,Item.y,Item.z);}
  return RouteResult;};
 goal.AddFinishCriteria=function(ent,kind,flag){this.FinishCriteria=true;};
 goal.ForkThread=function(f){this.ThreadStarted=true;return 1;};
 goal.KillThread=function(id){this.ThreadKilled=true;};};
Configure(PickupGoal);Configure(ReturnGoal);
'''

class ItemContactActualVMTests(unittest.TestCase):
    setUpClass=classmethod(flag_vm.FlagActualVMTests.setUpClass.__func__)
    run_vm=flag_vm.FlagActualVMTests.run_vm
    def goal(self, setup='', probe='', which='PickupGoal'):
        return self.run_vm(BOOT,
            'PickupGoal.Define=function(){\n'+PICKUP.read_text()+'\n};PickupGoal.Define();',
            'ReturnGoal.Define=function(){\n'+RETURN.read_text()+'\n};ReturnGoal.Define();',
            CONFIG+setup+which+'.Update();',probe or 'local no_probe=0;')

    def test_village_item_center_becomes_supported_contact_not_point_goal(self):
        out=self.goal(probe='print("RESULT:"+Routes+":"+PickupGoal.Radius+":"+PickupGoal.Dest.z+":"+UseCalls+":"+Releases+":"+Ammo+":"+RawMoves);')
        self.assertIn('RESULT:1:2:62.125:1:1:6:0',out)
        self.assertIn('INVENTORY_INCREASE',out);self.assertNotIn('BAD_NATIVE_HULL',out)

    def test_native_body_floor_hazard_and_dimensions_cannot_be_bypassed(self):
        for setup in ('global Solid=true;','global NoFloor=true;','global Water=1;',
                      'global Slope=0.69;','global Grounded=false;','global ItemWidth=24;',
                      'global Ceiling=true;','global Slick=1;global Slope=0.9;'):
            with self.subTest(setup=setup):
                out=self.goal(setup,'print("REJECT:"+Routes+":"+UseCalls+":"+PickupGoal.Done);')
                self.assertIn('REJECT:0:0:1',out);self.assertIn('NO_SAFE_CONTACT',out)

    def test_no_use_after_route_failure_or_item_motion(self):
        for setup,event in (('global RouteResult=0;','ROUTE_FAILED'),('global Changed=true;','CONTACT_CHANGED')):
            out=self.goal(setup,'print("NO_USE:"+UseCalls+":"+RawMoves);')
            self.assertIn('NO_USE:0:0',out);self.assertIn(event,out)

    def test_elapsed_time_and_deleted_item_are_not_claimed_as_pickup(self):
        for extra in ('','global DeleteAfterUse=true;'):
            out=self.goal('global PickAfterUse=false;'+extra,
                'print("BOUNDED:"+(Clock<=2.1)+":"+(UseCalls<=20)+":"+Releases);')
            self.assertIn('BOUNDED:1:1:1',out);self.assertIn('NOT_CONFIRMED',out)
            self.assertNotIn(' INVENTORY_INCREASE ',out)

    def test_real_flag_state_changes_are_distinguished_from_disappearance(self):
        out=self.goal('global PickAfterUse=false;global ReturnAfterUse=true;',
            'print("RETURN:"+Routes+":"+ReturnGoal.Radius+":"+UseCalls+":"+Releases);','ReturnGoal')
        self.assertIn('RETURN:1:2:1:1',out);self.assertIn('RETURN_STATE_OBSERVED',out)
        out=self.goal('global PickAfterUse=false;global DeleteAfterUse=true;',which='ReturnGoal')
        self.assertIn('FLAG_STATE_CHANGED',out);self.assertNotIn('RETURN_STATE_OBSERVED',out)
        out=self.goal('global PickAfterUse=false;', 'print("DELAY:"+ReturnGoal.Delay);','ReturnGoal')
        self.assertIn('RETURN_NOT_CONFIRMED',out);self.assertIn('DELAY:10',out)

    def test_flag_carried_or_at_base_never_uses_return_contact(self):
        for state in (1,3,4):
            out=self.goal(f'NativeGoal.State={state};','print("SKIP:"+Routes+":"+UseCalls);','ReturnGoal')
            self.assertIn('SKIP:0:0',out)

    def test_ordinary_et_keeps_original_goal_behavior(self):
        for mode in ('null','0'):
            out=self.goal('global Mode='+mode+';','print("STOCK:"+Routes+":"+PickupGoal.Radius+":"+UseCalls);')
            self.assertIn('STOCK:1:32:0',out)
        out=self.goal('global Mode=null;ReturnGoal.TargetPosition=Item;',
            'ReturnGoal.DynamicGoto();print("RETURN_STOCK:"+Routes+":"+ReturnGoal.Radius+":"+UseCalls);','ReturnGoal')
        self.assertIn('RETURN_STOCK:1:0:0',out)

    def test_tce_gt7_uses_checked_contact_and_native_use(self):
        out=self.goal('global Mode=7;',
            'print("GT7:"+Routes+":"+PickupGoal.Radius+":"+PickupGoal.Dest.z+":"+UseCalls+":"+Releases+":"+RawMoves);')
        self.assertIn('GT7:1:2:62.125:1:1:0',out)
        self.assertIn('INVENTORY_INCREASE',out)

    def test_contact_region_guarantees_both_ambiguous_item_origins(self):
        out=self.run_vm(BOOT,'PickupGoal.Define=function(){\n'+PICKUP.read_text()+'\n};PickupGoal.Define();',
            '''local b=TCE2ItemContact.Bounds(100);
print("BOUNDS:"+b.minZ+":"+b.maxZ+":"+TCE2ItemContact.Contains(b,Vec3(655,116,67))+":"+TCE2ItemContact.Contains(b,Vec3(655,116,68)));
global ItemHeight=10;local b=TCE2ItemContact.Bounds(100);print("SPAWN:"+b.minZ+":"+b.maxZ);''')
        self.assertIn('BOUNDS:9:67:1:0',out);self.assertIn('SPAWN:4:72',out)

    def test_tce_enter_avoids_raw_dynamic_thread_and_exit_releases_use(self):
        out=self.run_vm(BOOT,'PickupGoal.Define=function(){\n'+PICKUP.read_text()+'\n};PickupGoal.Define();',
            'ReturnGoal.Define=function(){\n'+RETURN.read_text()+'\n};ReturnGoal.Define();',CONFIG+
            '''PickupGoal.Enter();ReturnGoal.Enter();
PickupGoal.TCEUseHeld=true;ReturnGoal.TCEUseHeld=true;PickupGoal.Exit();ReturnGoal.Exit();
print("CANCEL:"+PickupGoal.FinishCriteria+":"+ReturnGoal.ThreadStarted+":"+Releases);''')
        self.assertIn('CANCEL:null:null:2',out)

if __name__=='__main__':unittest.main()
