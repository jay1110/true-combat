"""Door routes use Omni-bot's actual GM utility and MapGoal route consumer.

Offline fixtures test serialization/registration and directed progress. They
do not certify a live timed door or replace native waypoint collision proof.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

from goalgen import door_routes,make_plan,map_logic,render_goal
from test_door_runtime_native import body,compile_run,COMMON

ROOT=Path(__file__).resolve().parents[3]
SCRIPTS=ROOT/'tce2/omni-bot/global_scripts'
DRIVER=ROOT/'build/navstudio-gmvm/gm-quiet-vm.exe'


def fixture(exit_x=200,team=3):
    # The door is intentionally one-way. Its destination network must be an
    # unambiguous main component, rather than two tied three-node SCCs.
    positions=[[0,0,30],[100,0,30],[0,128,30],[exit_x,0,30],[320,0,30],[448,0,30],[576,0,30]]
    nodes=[dict(id=i,position=p,accepted=True,spawn_reachable=True,nav_flags=0,radius=35,crouch=False) for i,p in enumerate(positions)]
    recipe=dict(entity=96,use_entity=97,target_source_id=3,mode=2,team=team,role=2,model=1,use_model=2,
                use_source_id=2,entry_source_id=1,aim=[0,150,80],validated=True,exported=True)
    for i,role,target in ((1,2,3),(2,1,0),(3,3,-1)):
        nodes[i]['nav_flags']=128;nodes[i]['door_recipe']=dict(recipe,role=role,target_source_id=target)
    edges=[dict(**{'from':a,'to':b},accepted=True) for a,b in
        ((0,1),(1,0),(0,2),(2,0),(1,3),(3,4),(4,3),(4,5),(5,4),(5,6),(6,5))]
    return dict(generation_complete=True,bbox=dict(origin_above_feet=30),nodes=nodes,edges=edges)


def destination():
    return dict(name='TCEOBJECTIVE_site',type='TCEOBJECTIVE',source_node=5,position=[448,0,30],teams=6)


class DoorRoutePlanTests(TestCase):
    def test_staged_local_door_route_starts_at_safe_use_and_keeps_alignment(self):
        report=fixture()
        for node in report['nodes']:
            if node.get('door_recipe'):
                node['door_recipe'].update(mode=3,use_entity=96,use_model=1)
        report['nodes'][2]['door_recipe']['target_source_id']=1
        report['edges']=[e for e in report['edges'] if (e['from'],e['to']) not in ((0,1),(1,0))]
        report['edges'] += [dict(**{'from':a,'to':b},accepted=True) for a,b in ((2,1),(1,2))]
        goals,routes,unresolved=door_routes(report,[destination()])
        self.assertFalse(unresolved);self.assertTrue(routes)
        use=next(g for g in goals if g['door_role']=='use')
        for route in routes:
            self.assertEqual(route['start'],use['name'])
            self.assertEqual(route['source_node'],2)
            self.assertEqual(route['path'][:3],[2,1,3])

    def test_direct_activation_uses_the_same_certified_entry_not_an_invented_switch(self):
        report=fixture()
        for i in (1,3):
            report['nodes'][i]['door_recipe'].update(mode=1,use_entity=96,use_model=1,use_source_id=1)
        report['nodes'][2].pop('door_recipe');report['nodes'][2]['nav_flags']=0
        goals,routes,unresolved=door_routes(report,[destination()])
        self.assertFalse(unresolved);self.assertTrue(routes)
        self.assertEqual(next(g for g in goals if g['door_role']=='use')['source_node'],1)

    def test_checked_roles_become_standard_routes_and_directed_mission_assignments(self):
        goals,routes,unresolved=door_routes(fixture(),[destination()])
        self.assertFalse(unresolved);self.assertEqual(len(routes),2)
        for route in routes:
            self.assertEqual(route['path'],[1,3]);self.assertEqual(route['goal'],'TCEOBJECTIVE_site')
            self.assertEqual(route['activation'],'native_waypoint_door_recipe')
            end=next(g for g in goals if g['name']==route['end'])
            self.assertEqual(end['teams'],1<<route['team'])
        use=next(g for g in goals if g['door_role']=='use')
        self.assertEqual(use['source_node'],2)
        rendered=render_goal(use)
        self.assertIn('GoalType = "ROUTE"',rendered)
        self.assertIn('Radius = 32',rendered)
        self.assertNotIn('CampTime',rendered)
        self.assertFalse(any(r['start']==use['name'] or r['end']==use['name'] for r in routes))

    def test_near_exit_uses_native_continuation_no_overlap_loop(self):
        goals,routes,_=door_routes(fixture(exit_x=150),[destination()])
        self.assertTrue(routes)
        for route in routes:
            self.assertEqual(route['path'],[1,3,4]);self.assertEqual(route['target_node'],4)
            start=next(g for g in goals if g['name']==route['start'])
            end=next(g for g in goals if g['name']==route['end'])
            separation=sum((start['position'][i]-end['position'][i])**2 for i in (0,1))**.5
            self.assertGreater(separation,start['radius']+end['radius']+2)

    def test_invalid_stale_or_wrong_safe_exit_and_rejected_links_never_bound(self):
        for change in ('uncertified','unexported','wrong_use','old_exit','rejected_edge','wrong_aim'):
            report=fixture()
            if change=='uncertified':report['nodes'][1]['door_recipe']['validated']=False
            if change=='unexported':report['nodes'][3]['door_recipe']['exported']=False
            if change=='wrong_use':report['nodes'][2]['door_recipe']['role']=3
            if change=='old_exit':report['nodes'][3]['door_recipe']['role']=2
            if change=='rejected_edge':report['edges'][4]['accepted']=False
            if change=='wrong_aim':report['nodes'][2]['door_recipe']['aim']=[100,0,80]
            with self.subTest(change=change):
                goals,routes,unresolved=door_routes(report,[destination()])
                self.assertFalse(routes);self.assertTrue(unresolved)

    def test_team_restrictions_and_reverse_destination_cannot_invent_routes(self):
        goals,routes,_=door_routes(fixture(team=1),[destination()])
        self.assertEqual([r['team'] for r in routes],[1])
        self.assertTrue(all(g['teams']==2 for g in goals))
        reverse=dict(destination(),source_node=0,position=[0,0,30])
        self.assertFalse(door_routes(fixture(),[reverse])[1])
        report=fixture();report['nodes'][4]['nav_flags']=16|1
        self.assertEqual([r['team'] for r in door_routes(report,[destination()])[1]],[1])

    def test_generation_keeps_door_points_out_of_camps_and_objective_anchors(self):
        objective=dict(tag='site',entity_id=100,position=[448,0,30],aim_position=[448,0,60],
            bounds=[[430,-32,-8],[465,32,72]],owner=1,operation='plant_defuse',target='site')
        for gametype in (2,5):
            plan=make_plan(fixture(),[objective],gametype)
            self.assertTrue(plan['routes']);self.assertFalse(plan['route_behavior_live_verified'])
            self.assertTrue(all(g['source_node'] not in (1,2,3) for g in plan['goals'] if g['type']!='ROUTE'))
            logic=map_logic('obj_fixture',gametype,17,plan['routes'])
            self.assertIn('global InitializeRoutes = function()',logic)
            self.assertIn('Util.Routes(MapRoutes)',logic)
            self.assertNotIn('SetGoalPriority("ROUTE_',logic)


@skipUnless(os.name=='nt' and DRIVER.is_file(),'Existing offline real GameMonkey VM required')
class DoorRouteGMTests(TestCase):
    def test_real_vm_parses_generated_files_and_actual_util_routes_registers_all_assignments(self):
        goals,routes,_=door_routes(fixture(),[destination()])
        definition=destination();definition.update(aim_position=[448,0,60],activation_radius=2,operation='plant_defuse')
        data='global _MG={Version=1,\n'+''.join(render_goal(g) for g in [definition,*goals])+'};'
        util=(SCRIPTS/'utilities.gm').read_text()
        functions='\n'.join(body(util,'member '+name+' = function').replace('member '+name+' =','Util.'+name+' =')+';'
                            for name in ('AddGoalRoute','GoalRoutes','Routes'))
        bootstrap='''global Vec3=function(x,y,z){return {x=x,y=y,z=z};};
global Util={};global Registry={};global Calls={};
global GetGoal=function(name){return Registry[name];};
global IsTable=function(value){return true;};
Util.GetGoal=function(name,source){return Registry[name];};
Util.GetGoals=function(name,source){if(Registry[name]){return {[0]=Registry[name]};}return {};};
Util.MapDebugPrint=function(message,level){print(message);};
'''
        register='''foreach(name and props in _MG){if(name!="Version"){
 Registry[name]=props;props.Name=name;
 props.AddRoute=function(start,end,weight){global Calls;
 Calls[tableCount(Calls)]={goal=this.Name,start=start,end=end,weight=weight};return true;};}}
'''
        check='InitializeRoutes();foreach(call in Calls){print("BOUND:"+call.goal+":"+call.start+":"+call.end);}'
        # The VM compiles the actual TCE action script too; the test does not
        # pretend to supply engine movement or native objective completion.
        objective=(ROOT/'src/vendor/omni-bot/tce2-scripts/goals/goal_tceobjective.gm').read_text()
        scripts=[bootstrap,functions,data,register,map_logic('obj_fixture',5,17,routes),check,
                 'global Objective={};Objective.Load=function(){'+objective+'};Objective.Load();']
        with tempfile.TemporaryDirectory(prefix='navstudio-route-gm-') as folder:
            paths=[]
            for i,source in enumerate(scripts):
                p=Path(folder)/(str(i)+'.gm');p.write_text(source,encoding='ascii');paths.append(str(p))
            result=subprocess.run([str(DRIVER),*paths],capture_output=True,text=True,timeout=15,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        found=set(re.findall(r'BOUND:([^\s]+)',result.stdout))
        self.assertEqual(found,{r['goal']+':'+r['start']+':'+r['end'] for r in routes})
        self.assertIn('this.RouteTo(this.MapGoal,32)',objective)
        self.assertLess(objective.index('this.RouteTo('),objective.index('this.Goto(usePos,'))


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class DoorRouteNativeConsumerTests(TestCase):
    def test_actual_mapgoal_route_to_and_scriptgoal_finish_at_original_checked_target(self):
        goals,routes,_=door_routes(fixture(),[destination()])
        production=(COMMON/'MapGoal.cpp').read_text()
        methods='\n'.join(body(production,marker) for marker in
            ('bool MapGoal::AddRoute_Script(', 'bool MapGoal::AddRoute(', 'bool MapGoal::RouteTo('))
        consumer=body((COMMON/'ScriptGoal.cpp').read_text(),'bool ScriptGoal::GetNextDestination(')
        shim=r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <vector>
using obuint32=unsigned;using obint32=int;
struct Vector3f{float x,y,z;Vector3f(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 Vector3f operator-(const Vector3f&b)const{return{x-b.x,y-b.y,z-b.z};}};
namespace Mathf{float Sqr(float v){return v*v;}float Max(float a,float b){return a>b?a:b;}
 float IntervalRandom(float a,float){return a;}int IntervalRandomInt(int a,int){return a;}}
struct Bounds{float Extent[3]={16,16,43.5f};};
struct Client{Vector3f position;int team=1;Bounds GetWorldBounds(){return{};}
 Vector3f GetPosition(){return position;}int GetTeam(){return team;}int GetClass(){return 1;}};
struct Destination{Vector3f m_Position;float m_Radius;};using DestinationVector=std::vector<Destination>;
class MapGoal;using MapGoalPtr=MapGoal*;
class GoalManager{public:std::map<std::string,MapGoalPtr> goals;
 static GoalManager*GetInstance(){static GoalManager instance;return &instance;}
 MapGoalPtr GetGoal(const std::string&n){return goals[n];}};
class MapGoal{public:
 struct Route{MapGoalPtr m_Start,m_End;float m_Weight;};using Routes=std::vector<Route>;
 Routes m_Routes;struct ClassPriorities{float Priorities[2][1]={{1},{1}};}m_ClassPriority;
 Vector3f pos,use;float radius=32;int teams=6;bool m_RandomUsePoint=false;
 Vector3f GetPosition(){return pos;}float GetRadius(){return radius;}bool IsAvailable(int team){return(teams&(1<<team))!=0;}
 int GetNumUsePoints(){return 0;}Vector3f GetWorldUsePoint(int=-1){return use;}
 bool AddRoute_Script(const std::string&,const std::string&,float);
 bool AddRoute(const MapGoalPtr&,const MapGoalPtr&,float);bool RouteTo(Client*,DestinationVector&,float=0);
};
class ScriptGoal{public:MapGoalPtr m_MapGoalRoute;float m_MinRadius=32;bool m_SkipLastWp=false;Client bot;
 Client*GetClient(){return &bot;}bool GetNextDestination(DestinationVector&,bool&,bool&);};
'''
        setup='\n'.join('MapGoal g'+str(i)+';g'+str(i)+'.pos=Vector3f('+','.join(str(v) for v in g['position'])+');'+
             'g'+str(i)+'.use=g'+str(i)+'.pos;g'+str(i)+'.teams='+str(g['teams'])+';'+
             'GoalManager::GetInstance()->goals['+json.dumps(g['name'])+']=&g'+str(i)+';'
             for i,g in enumerate(goals))
        calls='\n'.join('assert(mission.AddRoute_Script('+json.dumps(r['start'])+','+json.dumps(r['end'])+',1));' for r in routes)
        main_code=r'''
int main(){
 MapGoal mission;mission.pos=Vector3f(999,999,999);mission.use=Vector3f(448,0,30);
 SETUP
 CALLS
 assert(mission.m_Routes.size()==2);
 ScriptGoal actor;actor.m_MapGoalRoute=&mission;bool final=true,skip=true;DestinationVector out;
 for(int team=1;team<=2;++team){
  actor.bot.team=team;actor.bot.position=Vector3f(100,0,30);out.clear();
  assert(actor.GetNextDestination(out,final,skip)&&!final&&!skip&&out.size()==1);
  assert(out[0].m_Position.x==200&&out[0].m_Radius==32);
  actor.bot.position=out[0].m_Position;out.clear();
  assert(actor.GetNextDestination(out,final,skip)&&final&&out.size()==1);
  assert(out[0].m_Position.x==448&&out[0].m_Position.y==0&&out[0].m_Position.z==30);
 }
 actor.bot.position=Vector3f(0,0,30);out.clear();
 assert(actor.GetNextDestination(out,final,skip)&&final&&out[0].m_Position.x==448);
 return 0;
}
'''.replace('SETUP',setup).replace('CALLS',calls)
        compile_run(self,shim+methods+consumer+main_code)


if __name__=='__main__':main()
