"""Generated tactics in the real offline Omni GM VM; never controls a game."""
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from goalgen import TACTICAL_TIMES,generate,map_logic,refresh_generated_tactics,render_goal,tactical_logic
import test_goalgen as fixtures

ROOT=Path(__file__).resolve().parents[1]
DRIVER=ROOT/'build/navstudio-gmvm/gm-quiet-vm.exe'
SCRIPT=ROOT/'vendor/omni-bot/tce2-scripts/goals/goal_camp.gm'
BOOTSTRAP=r'''
global Clock=0;global CarryAt=9999;global Mode=2;global Planted=0;
global ENTFLAG={DEAD=1,LIMBO=2,CARRYINGGOAL=3};global BTN={CROUCH=1,PRONE=2,AIM=3};global Priority={Low=1};
global GetMapName=function(){return "obj_fixture";};global GetTime=function(){return Clock;};global GetCvar=function(name){return 0;};
global RandFloat=function(a,b){return a;};global ToInt=function(v){n=0;while(n+1<=v){n+=1;}return n;};
global Map={NavStudioTactics={MapName="obj_fixture",Gametype=2,Goals={
 DEFEND_generated="Defend",ATTACK_generated="Attack",CAMP_generated="CAMP"}}};
global MakeBot=function(id,team){return {Id=id,Team=team,Dead=false,Carry=false,Bomb=false,
 GetTeam=function(){return this.Team;},GetGameId=function(){return this.Id;},
 IsCarryingFlag=function(){return this.Carry;},HasAnyEntityFlag=function(a,b,c){return this.Dead||this.Carry;},
 GetStat=function(name){if(name=="tce_gametype"){return Mode;}if(name=="tce_bomb_carrier"){return this.Bomb;}
 if(name=="tce_bomb_planted"){return Planted;}return 0;}};};
global BotTable={};for(i=1;i<=12;i+=1){team=1;if(i>6){team=2;}BotTable[i]=MakeBot(i,team);}
global MakeGoal=function(name,priority){return {Name=name,Prio=priority,Weapons={},DisableCM=0,Stance="stand",
 MinCampTime=15,MaxCampTime=30,AimVectors={[0]={x=1,y=0,z=0}},GetName=function(){return this.Name;},
 GetGoalPriority=function(bot){return this.Prio;}};};
global Queries={[0]=MakeGoal("DEFEND_generated",1.3),[1]=MakeGoal("ATTACK_generated",.35),[2]=MakeGoal("CAMP_generated",.3)};
global Util={QueryTable={},RouteTo=function(goal){},GetRandomTableValue=function(tbl){return tbl[0];}};
global ETUtil={GetBotPrimaryWeapon=function(bot){return 0;}};
global Goal={};
'''
CONFIGURE=r'''
Goal.Bot=BotTable[1];Goal.MapGoal=Queries[0];
Goal.QueryGoals=function(result,query){tableClear(result);foreach(i and goal in Queries){result[i]=goal;}return true;};
Goal.LimitTo=function(callback,interval,flag){this.ActiveCheck=callback;this.CheckInterval=interval;};
Goal.MarkInProgress=function(){return true;};Goal.Finished=function(){this.FinishedCount=1;};
Goal.AddAimRequest=function(a,b,c){};Goal.BlackboardDelay=function(t){this.Delay=t;};
'''


class GeneratedTacticsVMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name!='nt' or not DRIVER.is_file():raise unittest.SkipTest('Existing offline Omni-bot VM required.')
        receipt=json.loads(DRIVER.with_name('build.json').read_text())
        assert receipt['executable_sha256']==hashlib.sha256(DRIVER.read_bytes()).hexdigest()

    def vm(self,*scripts):
        with tempfile.TemporaryDirectory(prefix='tce-tactics-vm-') as directory:
            paths=[]
            for i,source in enumerate((BOOTSTRAP,'Goal.Define=function(){\n'+SCRIPT.read_text()+'\n};Goal.Define();',CONFIGURE,*scripts)):
                path=Path(directory)/(str(i)+'.gm');path.write_text(source,encoding='ascii');paths.append(str(path))
            run=subprocess.run([str(DRIVER),*paths],capture_output=True,text=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            return run.stdout

    def test_actual_camp_script_parses_and_bounds_defenders_for_both_teams(self):
        output=self.vm('''for(i=1;i<=12;i+=1){if(Goal.TCEDefender(BotTable[i])){print("GUARD:"+i);}}''')
        self.assertEqual(re.findall(r'GUARD:(\d+)',output),['1','2','7','8'])

    def test_small_team_carriers_and_urgent_bombs_keep_objective_progress(self):
        output=self.vm('''BotTable[1].Carry=true;BotTable[2].Bomb=true;
for(i=1;i<=6;i+=1){if(Goal.TCEDefender(BotTable[i])){print("GUARD:"+i);}}
global BotTable={ [1]=MakeBot(1,1),[2]=MakeBot(2,1) };print("SMALL:"+Goal.TCEDefender(BotTable[1]));
global Mode=5;Map.NavStudioTactics.Gametype=5;global Planted=1;print("URGENT:"+Goal.TCEActiveAllowed());''')
        self.assertEqual(re.findall(r'GUARD:(\d+)',output),['3'])
        self.assertIn('SMALL:0',output);self.assertIn('URGENT:0',output)

    def test_actual_util_priority_selects_defend_only_for_the_bounded_subset(self):
        text=(ROOT/'tce2/omni-bot/global_scripts/utilities.gm').read_text()
        body=text.split('member GetPriority = function',1)[1].split('\n};',1)[0]
        function='Util.GetPriority = function'+body+'\n};'
        for identity,expected in ((1,'1.3:DEFEND_generated'),(3,'0.35:ATTACK_generated')):
            output=self.vm(function,'Goal.Bot=BotTable['+str(identity)+'];Goal.GetPriority();',
                           'print("PRIORITY:"+Goal.Priority+":"+Goal.MapGoal.GetName());')
            self.assertIn('PRIORITY:'+expected,output)

    def test_active_guard_releases_new_carrier_and_preserves_manual_and_et_goals(self):
        output=self.vm('''Goal.Initialize();print("ACTIVE:"+Goal.TCEActiveAllowed()+":"+Goal.CheckInterval);
Goal.Bot.Carry=true;print("CARRIER:"+Goal.TCEActiveAllowed());
Goal.MapGoal=MakeGoal("DEFEND_authored",.7);print("AUTHORED:"+Goal.TCEActiveAllowed());
global Mode=null;Goal.MapGoal=Queries[0];print("ET:"+Goal.TCEActiveAllowed());''')
        self.assertIn('ACTIVE:1:0.5',output);self.assertIn('CARRIER:0',output)
        self.assertIn('AUTHORED:1',output);self.assertIn('ET:1',output)

    def test_stock_update_obeys_saved_dwell_and_interrupts_carriers(self):
        sleep='global sleep=function(t){global Clock;Clock+=t;if(Clock>=CarryAt){Goal.Bot.Carry=true;}};'
        for minimum,maximum,carry_at,expected in ((25,45,9999,25),(7,9,9999,7),(25,45,2,2)):
            output=self.vm(sleep,f'Goal.MapGoal.MinCampTime={minimum};Goal.MapGoal.MaxCampTime={maximum};global CarryAt={carry_at};Goal.Update();',
                           'print("HOLD:"+Clock+":"+Goal.TCEHoldSeconds+":"+Goal.FinishedCount);')
            self.assertIn(f'HOLD:{expected}:{minimum}:1',output)

    def test_actual_generated_map_logic_whitelists_and_prioritizes_only_generated_names(self):
        goals=[dict(name='DEFEND_generated',type='Defend'),dict(name='CAMP_generated',type='CAMP')]
        logic=map_logic('obj_fixture',2,17,goals=goals)
        output=self.vm('''global TEAM={AXIS=1,ALLIES=2};
global SetAvailableMapGoals=function(a,b,c,d){};global SetCvar=function(a,b){};
global SetGoalPriority=function(name,priority){print("SET:"+name+":"+priority);};''',logic,
            'OnMapLoad();print("POLICY:"+Map.NavStudioTactics.Goals["DEFEND_generated"]);')
        self.assertIn('SET:^DEFEND_generated$:1.3',output)
        self.assertIn('SET:^CAMP_generated$:0.3',output)
        self.assertIn('POLICY:Defend',output)
        self.assertNotIn('SET:ATTACK_.*|DEFEND_.*',output)


class GeneratedTacticalRefreshTests(unittest.TestCase):
    def setUp(self):
        case=fixtures.GoalFilesTests();case.setUp();self.addCleanup(case.doCleanups)
        self.root=case.root;self.studio=case.studio;self.objective=case.objective

    def legacy(self):
        (self.root/'project.json').write_text(json.dumps(dict(id='project-one',map='obj_test',mod='tce2')))
        with patch('goalgen.objectives',return_value=[self.objective('plant_defuse')]):generate(self.studio,'obj_test')
        file=self.root/'work/generated-goals.json';manifest=json.loads(file.read_text())
        for plan in manifest['plans']:
            gt=plan['gametype'];goals=self.root/f'bundles/obj_test_gt{gt}_goals.gm';logic=self.root/f'bundles/obj_test_gt{gt}.gm'
            raw=re.sub(r'MinCampTime = [0-9.]+','MinCampTime = 2',goals.read_text())
            raw=re.sub(r'MaxCampTime = [0-9.]+','MaxCampTime = 6',raw);goals.write_text(raw)
            old=plan['map_logic_token'];token=int(hashlib.sha256(raw.encode()).hexdigest()[:7],16)
            text=logic.read_text().replace(tactical_logic('obj_test',gt,plan['goals']),
                '    SetGoalPriority("ATTACK_.*|DEFEND_.*", 0.35);\n    SetGoalPriority("CAMP_NavStudio_.*", 0.12);\n')
            text=text.replace(str(old),str(token));logic.write_text(text);plan['map_logic_token']=token
            for path in (goals,logic):manifest['files'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        file.write_text(json.dumps(manifest));return file

    def test_only_legacy_tactics_change_dry_run_and_second_apply_are_safe(self):
        file=self.legacy();before={p:p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        result=refresh_generated_tactics(self.root)
        self.assertFalse(result['applied']);self.assertTrue(result['changes'])
        self.assertTrue(all(path.read_bytes()==raw for path,raw in before.items()))
        applied=refresh_generated_tactics(self.root,apply_changes=True)
        self.assertTrue(applied['applied']);self.assertTrue(Path(applied['backup']).is_dir())
        self.assertEqual((self.root/'native/obj_test_navmesh.json').read_bytes(),before[self.root/'native/obj_test_navmesh.json'])
        manifest=json.loads(file.read_text())
        for plan in manifest['plans']:
            for goal in plan['goals']:
                if goal['type'] not in TACTICAL_TIMES:
                    self.assertIn(render_goal(goal),(self.root/f'bundles/obj_test_gt{plan["gametype"]}_goals.gm').read_text())
        self.assertFalse(refresh_generated_tactics(self.root,apply_changes=True)['applied'])

    def test_any_user_file_or_marker_change_blocks_both_pairs_before_writing(self):
        self.legacy();path=self.root/'bundles/obj_test_gt5_goals.gm';path.write_text(path.read_text()+'// user edit\n')
        before={p:p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        with self.assertRaisesRegex(RuntimeError,'Bearbeitete/importierte'):refresh_generated_tactics(self.root,apply_changes=True)
        self.assertTrue(all(path.read_bytes()==raw for path,raw in before.items()))
        self.assertFalse(list((self.root/'work').glob('tactics-backup-*')))

    def test_explicit_nonlegacy_saved_dwell_is_preserved(self):
        file=self.legacy();path=self.root/'bundles/obj_test_gt2_goals.gm'
        text=path.read_text().replace('MinCampTime = 2','MinCampTime = 7',1).replace('MaxCampTime = 6','MaxCampTime = 9',1)
        path.write_text(text);manifest=json.loads(file.read_text());manifest['files'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest();file.write_text(json.dumps(manifest))
        refresh_generated_tactics(self.root,apply_changes=True)
        self.assertIn('MinCampTime = 7',path.read_text());self.assertIn('MaxCampTime = 9',path.read_text())

    def test_missing_marker_even_with_matching_sha_and_stale_native_stop_safely(self):
        file=self.legacy();path=self.root/'bundles/obj_test_gt2_goals.gm'
        path.write_bytes(path.read_bytes().replace(b'// TCE2_NAVSTUDIO_GENERATED_GOALS_V1',b'// imported',1))
        manifest=json.loads(file.read_text());manifest['files'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest();file.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(RuntimeError,'Bearbeitete/importierte'):refresh_generated_tactics(self.root,apply_changes=True)
        self.assertFalse(list((self.root/'work').glob('tactics-backup-*')))
        (self.root/'native/obj_test_navmesh.json').write_bytes(b'newer user native network')
        with self.assertRaisesRegex(RuntimeError,'Quellbeleg ist veraltet'):refresh_generated_tactics(self.root,apply_changes=True)


class TacticalSerializationTests(unittest.TestCase):
    def test_longer_defaults_and_explicit_zero_or_custom_values_are_preserved(self):
        for kind,(minimum,maximum) in TACTICAL_TIMES.items():
            goal=dict(name='TEST',type=kind,position=[1,2,3],aim_vector=[1,0,0])
            self.assertIn('MinCampTime = '+str(minimum),render_goal(goal))
            self.assertIn('MaxCampTime = '+str(maximum),render_goal(goal))
            goal.update(min_camp_time=0,max_camp_time=0)
            self.assertIn('MinCampTime = 0',render_goal(goal));self.assertIn('MaxCampTime = 0',render_goal(goal))
            goal.update(min_camp_time=55,max_camp_time=60)
            self.assertIn('MinCampTime = 55',render_goal(goal));self.assertIn('MaxCampTime = 60',render_goal(goal))


if __name__=='__main__':unittest.main()
