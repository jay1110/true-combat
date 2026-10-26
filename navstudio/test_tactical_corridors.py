"""Directed corridor/station plans and the real CAMP/Util GM consumer.

No game, project mutation, native module build or deployment. Synthetic walls
are encoded by absent/rejected graph links; no mocked collision query can make
an unproved link valid. Real map movement remains a separate live gate.
"""
import re
import inspect
import sys
from pathlib import Path
import unittest
from goalgen import corridor_routes, distributed_tactics, make_plan, map_logic, render_goal
import test_generated_tactics as legacy


def corridors(open_field=False):
    positions=[[-600,0,30],[-400,0,30],[-400,200,30],[-200,200,30],
               [0,200,30],[200,200,30],[400,200,30],[400,0,30],[600,0,30],
               [-400,-200,30],[-200,-200,30],[0,-200,30],[200,-200,30],[400,-200,30]]
    paths=[[0,1,2,3,4,5,6,7,8],[1,9,10,11,12,13,7]]
    if open_field:
        positions.extend([[-200,0,30],[0,0,30],[200,0,30]])
        paths.extend([[1,14,15,16,7],[3,14,10],[4,15,11],[5,16,12]])
    nodes=[dict(id=i,position=p,accepted=True,spawn_reachable=True,nav_flags=0,radius=35,crouch=False,
                reason='map_entity_spawn_seed' if i==0 else 'native_floor') for i,p in enumerate(positions)]
    edges=[]
    for path in paths:
        for a,b in zip(path,path[1:]):
            edges.extend([dict(**{'from':a,'to':b},accepted=True),dict(**{'from':b,'to':a},accepted=True)])
    return dict(generation_complete=True,bbox=dict(origin_above_feet=30),nodes=nodes,edges=edges)


def destination():
    return dict(name='DEFEND_site',type='Defend',source_node=8,position=[600,0,30],teams=6)


class TacticalCorridorPlanTests(unittest.TestCase):
    def test_actual_generator_closure_excludes_exact_native_stacked_floor_catchment(self):
        captured={}
        def trace(frame,event,arg):
            if frame.f_code.co_name=='corridor_routes' and 'outside_starts' in frame.f_locals:
                captured['function']=frame.f_locals['outside_starts']
            return trace
        old=sys.gettrace();sys.settrace(trace)
        try:corridor_routes(corridors(),[destination()])
        finally:sys.settrace(old)
        outside=captured['function'];values=inspect.getclosurevars(outside).nonlocals
        start=values['eligible'][0];radius=values['radii'][start['id']]
        source=(Path(__file__).resolve().parents[1]/'vendor/omni-bot/source/Omnibot/Common/MapGoal.cpp').read_text()
        # Execute the actual native catchment predicate, not a reimplemented
        # model. All inputs are integers/halves exactly representable in C++.
        expression=re.search(r'if\(\s*(vDist\.x.*?r\.m_Start->GetRadius\(\))\s*&&\s*// Most maps',source,re.S).group(1)
        expression=expression.replace('Mathf::Sqr(r.m_Start->GetRadius())','(radius*radius)').replace(
            'r.m_Start->GetRadius()','radius').replace('vDist.x','dx').replace('vDist.y','dy').replace('vDist.z','dz').replace('&&',' and ')
        expression=' '.join(expression.split())
        probe=lambda dz:dict(position=[start['position'][0],start['position'][1],start['position'][2]+dz])
        self.assertFalse(outside(probe(radius+129))) # the original real counterexample
        self.assertTrue(outside(probe(radius+193)))
        for height in (70,87):
            for dz in (radius+129,radius+130.5,radius+160,radius+192,radius+193,-radius-129,-radius-193):
                if not outside(probe(dz)):continue
                for arrival_z in (-32,0,32):
                    native=eval(expression,{'__builtins__':{},'abs':abs},dict(
                        radius=radius,dx=0.,dy=0.,dz=dz+arrival_z,fTolerance=height/2))
                    self.assertFalse(native,(height,dz,arrival_z))

    def test_route_anchors_and_assignments_are_bounded_and_shared_across_destinations(self):
        goals,routes,families=corridor_routes(corridors(),[dict(destination(),name='DEFEND_site'+str(i))for i in range(80)])
        self.assertLessEqual(len(goals),48)
        self.assertLessEqual(len(families),64)
        self.assertLessEqual(len(routes),128)
        self.assertGreater(len(routes),len(goals))
        self.assertEqual(len({g['name']for g in goals}),len(goals))
        self.assertTrue(all(sum(f['goal']==goal and f['team']==team for f in families)<=2
                            for team in (1,2)for goal in {f['goal']for f in families}))

    def test_corridors_cannot_reenter_existing_door_route_catchments(self):
        prior=[dict(name='ROUTE_door_start',position=[0,-200,30],radius=32),
               dict(name='ROUTE_door_end',position=[-600,0,30],radius=32)]
        old=[dict(goal='DEFEND_site',team=1,start='ROUTE_door_start',end='ROUTE_door_end')]
        goals,routes,_=corridor_routes(corridors(),[destination()],prior,old)
        by_name={g['name']:g for g in goals}
        for route in routes:
            if route['team']==1:
                self.assertNotEqual(route['source_node'],0)
                self.assertGreater(sum((a-b)**2 for a,b in zip(by_name[route['end']]['position'][:2],[0,-200]))**.5,72)

    def test_two_wall_separated_corridors_produce_real_weighted_paths_and_final_ends(self):
        report=corridors();goals,routes,families=corridor_routes(report,[destination()])
        self.assertTrue(families)
        family=next(f for f in families if f['source_node']==0 and f['team']==1)
        self.assertEqual(len(family['branches']),2)
        self.assertGreater(family['branches'][1]['corridor_separation'],159)
        accepted={(e['from'],e['to'])for e in report['edges']if e['accepted']}
        starts=[g for g in goals if g['corridor_role']=='entry' and g['teams']==2]
        by_name={g['name']:g for g in goals}
        for route in routes:
            self.assertEqual(route['path'][0],route['source_node'])
            self.assertEqual(route['path'][-1],8)
            self.assertEqual(len(set(route['path'])),len(route['path']))
            self.assertTrue(all((a,b)in accepted for a,b in zip(route['path'],route['path'][1:])))
            end=by_name[route['end']]
            for start in starts:
                if start['teams']==end['teams']:
                    distance=sum((a-b)**2 for a,b in zip(start['position'][:2],end['position'][:2]))**.5
                    self.assertGreater(distance,start['radius']+40)

    def test_open_mesh_and_single_corridor_do_not_invent_flanks(self):
        self.assertFalse(corridor_routes(corridors(True),[destination()])[2])
        report=corridors();report['nodes']=report['nodes'][:9]
        self.assertFalse(corridor_routes(report,[destination()])[2])

    def test_rejected_links_team_locks_and_action_transit_are_obeyed(self):
        report=corridors();report['nodes'][10]['nav_flags']=512
        routes=corridor_routes(report,[destination()])[1]
        self.assertTrue(any(10 in r['path']for r in routes))
        report['nodes'][10]['nav_flags']=0
        report['nodes'][11]['nav_flags']=16|1
        self.assertEqual({f['team']for f in corridor_routes(report,[destination()])[2]},{1})
        report['nodes'][11]['nav_flags']=0
        for e in report['edges']:
            if {e['from'],e['to']}=={10,11}:e['accepted']=False
        self.assertFalse(corridor_routes(report,[destination()])[2])

    def test_many_distributed_stations_use_only_roundtrip_native_floors_and_real_edge_facing(self):
        nodes=[dict(id=y*7+x,position=[x*384,y*384,30],accepted=True,spawn_reachable=True,
                    nav_flags=0,radius=35)for y in range(7)for x in range(7)]
        edges=[dict(**{'from':a['id'],'to':b['id']},accepted=True)for a in nodes for b in nodes
               if abs(a['id']//7-b['id']//7)+abs(a['id']%7-b['id']%7)==1]
        # Reachable one-way cul-de-sac cycle; no way back to a mission.
        nodes.extend(dict(id=50+i,position=[4000+i*384,0,30],accepted=True,spawn_reachable=True,
                          nav_flags=0,radius=35)for i in range(3))
        edges.extend(dict(**{'from':a,'to':b},accepted=True)for a,b in ((6,50),(50,51),(51,52),(52,50)))
        report=dict(generation_complete=True,nodes=nodes,edges=edges)
        mission=dict(name='TCEOBJECTIVE_site',source_node=0,position=[0,0,30],
                     tactical_defend_teams=2,tactical_attack_teams=4)
        goals=distributed_tactics(report,[mission],[])
        self.assertEqual(sum(g['type']=='Defend'for g in goals),12)
        self.assertEqual(sum(g['type']=='Attack'for g in goals),12)
        self.assertTrue(all(g['source_node']<49 for g in goals))
        self.assertTrue(all(g['station_route_checked']for g in goals))
        for kind in ('Defend','Attack'):
            stations=[g for g in goals if g['type']==kind]
            self.assertGreater(max(g['position'][0]for g in stations),2000)
            self.assertGreater(max(g['position'][1]for g in stations),2000)
            self.assertIn('GroupName = "NavStudio_'+kind,render_goal(stations[0]))


class TacticalDistributionVMTests(unittest.TestCase):
    setUpClass=classmethod(legacy.GeneratedTacticsVMTests.setUpClass.__func__)
    vm=legacy.GeneratedTacticsVMTests.vm
    def setUp(self):
        self.goals=[]
        for kind in ('Defend','Attack'):
            for i in range(4):
                self.goals.append(dict(name=kind.upper()+'_sector'+str(i),type=kind,position=[i*500,0,30],
                                       teams=2,aim_vector=[1,0,0]))
        self.logic=map_logic('obj_fixture',2,17,goals=self.goals)
        self.setup='''global TEAM={AXIS=1,ALLIES=2};global Registry={};global Queries={};
global SetAvailableMapGoals=function(a,b,c,d){};global SetCvar=function(a,b){};
global SetGoalPriority=function(name,priority){foreach(goal in Registry){
 if(name=="^"+goal.Name+"$"){goal.Prio=priority;}}};
Util.SetMaxUsers=function(users,name){print("MAX:"+users+":"+name);};
'''+''.join('Registry["'+g['name']+'"]=MakeGoal("'+g['name']+'",.5);\n'for g in self.goals)+'''
foreach(name and goal in Registry){Queries[tableCount(Queries)]=goal;}
'''

    def test_generated_role_quota_and_distinct_far_stations_with_real_util_priority(self):
        source=(Path(__file__).resolve().parents[1]/'tce2/omni-bot/global_scripts/utilities.gm').read_text()
        function='Util.GetPriority = function'+source.split('member GetPriority = function',1)[1].split('\n};',1)[0]+'\n};'
        selected=[]
        for identity in (1,2,3,4,5,6):
            output=self.vm(self.setup,self.logic,'OnMapLoad();',function,
                           'Goal.Bot=BotTable['+str(identity)+'];Goal.GetPriority();',
                           'print("CHOSEN:"+Goal.Priority+":"+Goal.MapGoal.GetName());')
            selected.append(re.search(r'CHOSEN:([^\r\n]+)',output).group(1).strip())
        self.assertEqual(selected[:4],['1.3:DEFEND_sector0','1.3:DEFEND_sector3',
                                      '1.25:ATTACK_sector0','1.25:ATTACK_sector3'])
        self.assertTrue(all(s.startswith('0:')for s in selected[4:]))
        self.assertEqual(len(set(selected[:2])),2)

    def test_station_rotation_missing_or_busy_fallback_and_carrier_release(self):
        output=self.vm(self.setup,self.logic,'OnMapLoad();Goal.Bot=BotTable[1];',
            '''foreach(i and goal in Queries){Util.QueryTable[i]=goal;}
foreach(goal in Queries){if(Goal.TCEEligible(goal,Goal.Bot,Goal)){print("FIRST:"+goal.GetName());}}
global Clock=1;Goal.TCEStationCycle=1;Goal.TCEStationPicks={};
foreach(goal in Queries){if(Goal.TCEEligible(goal,Goal.Bot,Goal)){print("NEXT:"+goal.GetName());}}
global Clock=2;Goal.TCEStationCycle=0;Goal.TCEStationPicks={};
BotTable[2].GetMapGoalName=function(){return "DEFEND_sector0";};
foreach(goal in Queries){if(Goal.TCEEligible(goal,Goal.Bot,Goal)){print("BUSY:"+goal.GetName());}}
Goal.MapGoal=Registry["DEFEND_sector1"];Goal.Bot.Carry=true;print("CARRIER:"+Goal.TCEActiveAllowed());
Goal.MapGoal=MakeGoal("DEFEND_authored",.9);print("AUTHORED:"+Goal.TCEActiveAllowed());''')
        self.assertIn('FIRST:DEFEND_sector0',output)
        self.assertIn('NEXT:DEFEND_sector1',output)
        self.assertIn('BUSY:DEFEND_sector3',output)
        self.assertIn('CARRIER:0',output);self.assertIn('AUTHORED:1',output)
        self.assertIn('MAX:1:^DEFEND_sector0$',output)

    def test_actual_stock_maxusers_applies_only_to_exact_generated_goals(self):
        source=(Path(__file__).resolve().parents[1]/'tce2/omni-bot/global_scripts/utilities.gm').read_text()
        function='Util.SetMaxUsers = function'+source.split('member SetMaxUsers = function',1)[1].split('\n};',1)[0]+'\n};'
        output=self.vm(self.setup,function,'''Registry["DEFEND_authored"]=MakeGoal("DEFEND_authored",.9);
Util.GetGoals=function(pattern,source){result={};foreach(goal in Registry){
 if(pattern=="^"+goal.Name+"$"){result[tableCount(result)]=goal;}}return result;};
foreach(goal in Registry){goal.Users=99;goal.Progress=99;
goal.MaxUsers_InUse=function(n){this.Users=n;};goal.MaxUsers_InProgress=function(n){this.Progress=n;};}''',
            self.logic,'''OnMapLoad();print("USERS:"+Registry["DEFEND_sector0"].Users+":"+Registry["DEFEND_sector0"].Progress);
print("AUTHORED:"+Registry["DEFEND_authored"].Users+":"+Registry["DEFEND_authored"].Prio);''')
        self.assertIn('USERS:1:1',output);self.assertIn('AUTHORED:99:0.9',output)


class TacticalRoutesVMTests(unittest.TestCase):
    setUpClass=classmethod(legacy.GeneratedTacticsVMTests.setUpClass.__func__)
    vm=legacy.GeneratedTacticsVMTests.vm

    def test_actual_util_routes_registers_both_real_corridors_for_the_same_start(self):
        import test_goal_routes as route_fixtures
        definitions,routes,_=corridor_routes(corridors(),[destination()])
        goal=dict(destination(),aim_vector=[1,0,0])
        data='global _MG={Version=1,\n'+''.join(render_goal(g)for g in [goal,*definitions])+'};'
        utilities=(route_fixtures.SCRIPTS/'utilities.gm').read_text()
        functions='\n'.join(route_fixtures.body(utilities,'member '+name+' = function').replace(
            'member '+name+' =','Util.'+name+' =')+';'for name in ('AddGoalRoute','GoalRoutes','Routes'))
        bootstrap='''global Registry={};global Calls={};
global GetGoal=function(name){return Registry[name];};global IsTable=function(value){return true;};
Util.GetGoal=function(name,source){return Registry[name];};
Util.GetGoals=function(name,source){if(Registry[name]){return {[0]=Registry[name]};}return {};};
Util.MapDebugPrint=function(message,level){print(message);};'''
        register='''foreach(name and props in _MG){if(name!="Version"){Registry[name]=props;props.Name=name;
props.AddRoute=function(start,end,weight){Calls[tableCount(Calls)]={goal=this.Name,start=start,end=end};return true;};}}
'''
        output=self.vm(bootstrap,functions,data,register,map_logic('obj_fixture',2,17,routes),
                      'InitializeRoutes();foreach(call in Calls){print("BOUND:"+call.goal+":"+call.start+":"+call.end);}')
        self.assertEqual(set(re.findall(r'BOUND:([^\s]+)',output)),
                         {r['goal']+':'+r['start']+':'+r['end']for r in routes})


if __name__=='__main__':unittest.main()
