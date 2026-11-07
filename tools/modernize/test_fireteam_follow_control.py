"""Shared-syntax Follow control-flow proof, not a GameMonkey VM/physics claim.

The actual GM Follow and GetPriority functions execute in a JS host. Yield is
a recorded service, and sleep ends one priority iteration. Native Stop bindings
are independently checked; killing a coroutine leaves the native path alive.
"""
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[2]
GM=ROOT/'vendor/omni-bot/runtime/et/scripts/goals/goal_fireteam.gm'


def function(name):
    source=GM.read_text();start=source.index('this.'+name+' = function()')
    begin=source.index('{',start);depth=1;end=begin+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return 'function()'+source[begin:end]


HARNESS=r'''
const assert=require('node:assert/strict'),vm=require('node:vm');
const input=JSON.parse(require('node:fs').readFileSync(0,'utf8'));
let leader={x:200,y:1,z:0,IsZero(){return false;}},target={},pathActive=true,stops=0,kills=0,forks=0,yields=0;
const botpos={x:1,y:1,z:0,IsZero(){return false;}};
const context={GetEntPosition:()=>leader,DistanceBetween:(a,b)=>Math.hypot(a.x-b.x,a.y-b.y,a.z-b.z),ENTFLAG:{ON_GROUND:1,ON_LADDER:2,ON_ICE:3},BTN:{SPRINT:1}};
const follow=vm.runInNewContext('('+input.follow.replaceAll('yield();','this.Yield();')+')',context);
const goal={TargetPosition:leader,FollowHolding:false,OnTarget:false,DynamicGotoThread:123,DynamicGoto:1,Debug:false,
  Bot:{FireTeam:[{Leader:44}],FireTeamNum:0,FireTeamFormation:'random',GetPosition:()=>botpos,
       HasEntityFlag:()=>true,HasAnyEntityFlag:()=>false,MoveTowards:p=>{target=p;},SayFireTeam:()=>{},SayTeam:()=>{},HoldButton:()=>{}},
  KillThread:()=>{++kills;},Stop:()=>{pathActive=false;++stops;},ForkThread:()=>{++forks;pathActive=true;return forks+1000;},
  Finished:()=>{throw Error('unexpected finish');},Yield:()=>{++yields;},FormationFront:()=>{},FormationRandom:()=>{}};
function tick(distance){leader={x:botpos.x+distance,y:1,z:0,IsZero(){return false;}};follow.call(goal);if(pathActive)target=leader;}
tick(95);assert(goal.FollowHolding);assert.equal(stops,1);assert.equal(kills,1);assert.equal(goal.DynamicGotoThread,null);assert.equal(target,botpos);
for(const distance of [96,100,127,128,110,95]){tick(distance);assert(goal.FollowHolding);assert.equal(stops,1);assert.equal(target,botpos);}
assert.equal(forks,0);assert.equal(yields,7);
tick(129);assert(!goal.FollowHolding);assert.equal(forks,1);assert(pathActive);assert.equal(stops,1);
tick(95);assert(goal.FollowHolding);assert.equal(stops,2);assert.equal(kills,2);assert.equal(target,botpos);
goal.FollowHolding=false;goal.Bot.HasAnyEntityFlag=()=>true;tick(90);assert(!goal.FollowHolding);assert.equal(stops,2);
goal.Bot.HasAnyEntityFlag=()=>false;goal.Bot.HasEntityFlag=()=>false;tick(90);assert(!goal.FollowHolding);assert.equal(stops,2);
console.log('actual GM Follow shared-syntax control: hysteresis, native-stop ownership and air/ladder guards passed');
'''


PRIORITY_HARNESS=r'''
const assert=require('node:assert/strict'),vm=require('node:vm');
const input=JSON.parse(require('node:fs').readFileSync(0,'utf8'));
const pause={},config={enabled:0};let reads=0,updates=0,active=false;
const context={ConfigGet:(section,key,fallback)=>{
  assert.equal(section,'FireTeam');assert.equal(key,'enabled');++reads;
  return config.enabled===undefined?fallback:config.enabled;
},sleep:seconds=>{assert.equal(seconds,1);throw pause;}};
const priority=vm.runInNewContext('('+input.priority+')',context);
const goal={FireTeamEnabled:0,Priority:99,CommandPriority:.7,
  Bot:{IsInFireTeam:()=>true,FireTeamNum:0},
  IsActive:()=>active,UpdateFireTeamInfo:()=>{++updates;}};
function tick(){const before=reads;assert.throws(()=>priority.call(goal),error=>error===pause);assert.equal(reads,before+1);}
tick();assert.equal(goal.Priority,0);assert.equal(updates,0);
// Same existing goal: only the persisted command configuration changes.
config.enabled=1;tick();assert.equal(goal.FireTeamEnabled,1);assert.equal(goal.Priority,.7);assert.equal(updates,1);
active=true;config.enabled=0;tick();assert.equal(goal.FireTeamEnabled,0);assert.equal(goal.Priority,0);assert.equal(updates,1);
active=false;config.enabled=1;tick();assert.equal(goal.Priority,.7);assert.equal(updates,2);
delete config.enabled;tick();assert.equal(goal.FireTeamEnabled,1);assert.equal(goal.Priority,.7);assert.equal(updates,3);
console.log('actual GM GetPriority shared-syntax: existing goal observes on/off configuration once per1s iteration');
'''


class FollowControl(unittest.TestCase):
    def test_existing_goal_observes_config_toggle_and_missing_poll_mutation(self):
        node=shutil.which('node')
        if not node:self.skipTest('Node JS host unavailable; this is not a GameMonkey VM test')
        actual=function('GetPriority')
        poll='this.FireTeamEnabled = ConfigGet("FireTeam", "enabled", this.FireTeamEnabled);'
        self.assertIn(poll,actual)
        for candidate,success in ((actual,True),(actual.replace(poll,''),False)):
            result=subprocess.run([node,'-e',PRIORITY_HARNESS],input=json.dumps({'priority':candidate}),capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode==0,success,result.stdout+result.stderr)

    def test_real_stop_binding_cancels_native_path_instead_of_only_a_thread(self):
        common=ROOT/'vendor/omni-bot/source/Omnibot/Common'
        bind=(common/'gmScriptGoal.cpp').read_text()
        self.assertRegex(bind,r'GMBIND_FUNCTION\(\s*"Stop",\s*gmfStop\s*\)')
        self.assertRegex(bind,r'int gmScriptGoal::gmfStop\([^)]*\)\s*\{\s*CHECK_THIS_SGOAL\(\);\s*native->Stop\(\);\s*return GM_OK;')
        self.assertRegex((common/'ScriptGoal.cpp').read_text(),r'void ScriptGoal::Stop\(\)\s*\{\s*GetClient\(\)->GetFollowPath\(\)->Stop\(\);\s*\}')
        stop=(common/'BotBaseStates.cpp').read_text().split('void FollowPath::Stop(bool _clearuser)',1)[1].split('void FollowPath::',1)[0]
        self.assertIn('m_PathStatus = PathFinished;',stop);self.assertIn('m_CurrentPath.Clear();',stop)

    def test_actual_follow_control_flow_and_missing_stop_mutation(self):
        node=shutil.which('node')
        if not node:self.skipTest('Node JS host unavailable; this is not a GameMonkey VM test')
        actual=function('Follow')
        for candidate,success in ((actual,True),(actual.replace('this.Stop();',''),False)):
            result=subprocess.run([node,'-e',HARNESS],input=json.dumps({'follow':candidate}),capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode==0,success,result.stdout+result.stderr)

    def test_slots_are_stable_unique_and_separated_for_full_six_member_fireteam(self):
        formation=function('FormationRandom')
        offsets=[tuple(map(float,item)) for item in re.findall(r'Vector3\(([-\d]+),([-\d]+),([-\d]+)\)',formation)]
        self.assertEqual(len(offsets),8);self.assertNotIn('RandInt',formation)
        self.assertIn('this.GetSlotNumber() % tableCount(offsets)',formation)
        self.assertTrue(all(math.dist((0,0,0),point)>=128 for point in offsets))
        self.assertTrue(all(math.dist(a,b)>=96 for a,b in itertools.combinations(offsets,2)))
        # The native binding fills a dense table0..n-1, not client-ID keys.
        binding=(ROOT/'vendor/omni-bot/source/Omnibot/ET/gmETBinds.cpp').read_text()
        self.assertIn('mbrtbl->Set(pM,m++,gmVariable::EntityVar(ft.m_Members[i].AsInt()))',binding)
        self.assertEqual(len({slot%len(offsets) for slot in range(6)}),6)

    def test_runtime_manifest_binds_actual_changed_script(self):
        source=json.loads((ROOT/'vendor/omni-bot/runtime/SOURCE.json').read_bytes())
        row=next(item for item in source['files'] if item['path']=='et/scripts/goals/goal_fireteam.gm')
        data=GM.read_bytes();self.assertEqual(row['bytes'],len(data));self.assertEqual(row['sha256'],hashlib.sha256(data).hexdigest())


if __name__=='__main__':unittest.main()
