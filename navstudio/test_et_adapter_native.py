"""Compile the actual adapter against deterministic engine collision fixtures.

This checks parser/serialization/body-query behavior. It is explicitly not an
original ET game or a live movement test. The driver creates no game instance.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

ADAPTER = Path(__file__).resolve().parents[1]/'vendor/omni-bot/source/Omnibot/Common/NavStudioAdapter.cpp'
SHIM = r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
using String=std::string;using StringVector=std::vector<String>;
namespace fs=std::filesystem;
struct Vector3f {
 float x=0,y=0,z=0;Vector3f(){}Vector3f(float a,float b,float c):x(a),y(b),z(c){}
 float&operator[](int i){return (&x)[i];}const float&operator[](int i)const{return (&x)[i];}
 operator const float*()const{return &x;}Vector3f operator+(const Vector3f&p)const{return{x+p.x,y+p.y,z+p.z};}
 Vector3f operator-(const Vector3f&p)const{return{x-p.x,y-p.y,z-p.z};}Vector3f operator*(float f)const{return{x*f,y*f,z*f};}
 float SquaredLength()const{return x*x+y*y+z*z;}float Length()const{return std::sqrt(SquaredLength());}
 static const Vector3f ZERO;
};const Vector3f Vector3f::ZERO{};
struct AABB{float m_Mins[3]={0,0,0},m_Maxs[3]={0,0,0};};
struct GameEntity{int id=-1;bool IsValid()const{return id>=0;}int GetIndex()const{return id;}bool operator==(const GameEntity&e)const{return id==e.id;}};
struct obTraceResult{float m_Fraction=1,m_Normal[3]={0,0,1},m_Endpos[3]={0,0,0};GameEntity m_HitEntity{};int m_StartSolid=0,m_Surface=0;};
struct AutoNavFeature{int m_Type=0,m_Team=0;float m_Position[3]={},m_TargetPosition[3]={};AABB m_Bounds;bool m_ObstacleEntity=false,m_BiDirectional=false;};
enum{TR_MASK_SOLID=2,TR_MASK_PLAYERCLIP=256,CONT_WATER=2,CONT_SLIME=4,CONT_LAVA=64,CONT_LADDER=128,SURFACE_LADDER=2,ENT_CLASS_GENERIC_PLAYERSTART=10,ENT_CLASS_GENERIC_PLAYERSTART_TEAM1=11,ENT_CLASS_GENERIC_PLAYERSTART_TEAM2=12};
using obBool=int;enum{False=0};
String scenario,folder,mapname;std::vector<String> errors;
struct Engine {
 const char*GetLogPath(){return folder.c_str();}const char*GetMapName(){return mapname.c_str();}
 int GetPointContents(const float*p){return scenario=="water"?CONT_WATER:0;}
 void GetMapExtents(AABB&b){for(int i=0;i<3;i++){b.m_Mins[i]=-4096;b.m_Maxs[i]=4096;}}
 int GetAutoNavFeatures(AutoNavFeature*,int){return 0;}GameEntity EntityFromID(int){return{};}int GetEntityClass(GameEntity){return 0;}
};Engine engine;Engine*g_EngineFuncs=&engine;
const char*va(const char*f,...){static char b[4096];va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);return b;}
namespace EngineFuncs {
 void ConsoleMessage(const char*m){std::cout<<m<<'\n';}void ConsoleError(const char*m){errors.push_back(m);std::cout<<m<<'\n';}
 bool EntityPosition(GameEntity,Vector3f&){return false;}bool EntityLocalAABB(GameEntity,AABB&){return false;}bool EntityWorldAABB(GameEntity,AABB&){return false;}
 bool EntityOrientation(GameEntity,float*,float*,float*){return false;}String EntityName(GameEntity){return"";}
 // Independent plane/ceiling/wall fixtures. Adapter decides how to query them.
 bool TraceLine(obTraceResult&r,const Vector3f&a,const Vector3f&b,const AABB*h,int,int,obBool){
  r=obTraceResult{};for(int i=0;i<3;i++)r.m_Endpos[i]=b[i];
  float minz=h?h->m_Mins[2]:0,maxz=h?h->m_Maxs[2]:0,width=h?std::abs(h->m_Mins[0]):0;
  float ceiling=scenario=="duck"?60:10000;
  if(a.z+minz<-.001f||a.z+maxz>ceiling+.001f){r.m_StartSolid=1;r.m_Fraction=0;return true;}
  if(b.z+maxz>ceiling+.001f&&b.z>a.z){r.m_Fraction=(ceiling-a.z-maxz)/(b.z-a.z);r.m_Normal[2]=-1;}
  bool gap=scenario=="gap"||scenario=="jump";
  if(b.z+minz<0&&b.z<a.z&&(!gap||b.x<40||b.x>88)){
   float hit=-(a.z+minz)/(b.z-a.z);if(hit<r.m_Fraction){r.m_Fraction=hit;r.m_Normal[2]=1;r.m_HitEntity.id=1022;}
  }
  if(scenario=="wall"){
   float left=56-width,right=72+width;
   if(a.x>left&&a.x<right){r.m_StartSolid=1;r.m_Fraction=0;}
   else if(a.x<=left&&b.x>left)r.m_Fraction=std::min(r.m_Fraction,(left-a.x)/(b.x-a.x));
   else if(a.x>=right&&b.x<right)r.m_Fraction=std::min(r.m_Fraction,(right-a.x)/(b.x-a.x));
  }
  for(int i=0;i<3;i++)r.m_Endpos[i]=a[i]+(b[i]-a[i])*r.m_Fraction;
  return true;
 }
}
'''
DRIVER = r'''
int main(int argc,char**argv){if(argc!=5)return 2;folder=argv[1];scenario=argv[2];mapname=argv[3];
 NavStudioAdapter::Import({"navstudio_import",String("navgen/")+argv[4]});
 for(int i=0;i<100;i++)NavStudioAdapter::Update();return errors.empty()?0:1;}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'), 'Windows CMake/MSVC fixture compiler required')
class NativeAdapterTests(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.root=Path(cls.temp.name)
        source=ADAPTER.read_text()
        source='\n'.join(line for line in source.splitlines() if not line.startswith('#include "'))
        (cls.root/'main.cpp').write_text(SHIM+'\n'+source+'\n'+DRIVER)
        (cls.root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(AdapterFixture LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 17)\nadd_executable(adapter-fixture main.cpp)\n')
        flags=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
        for command in (['cmake','-S',str(cls.root),'-B',str(cls.root/'build'),'-A','Win32'],
                        ['cmake','--build',str(cls.root/'build'),'--config','Release']):
            result=subprocess.run(command,**flags)
            if result.returncode:raise AssertionError(result.stdout+result.stderr)
        cls.exe=cls.root/'build/Release/adapter-fixture.exe'

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def run_fixture(self,scenario='flat',version=5,flags=0,crouch=0,role=0):
        home=self.root/scenario;runtime=home/'navgen';runtime.mkdir(parents=True,exist_ok=True)
        name='adapter_'+scenario;filename=name+'_candidate.txt'
        low,high=flags&0xffffffff,flags>>32
        rows=[f'TCE_NAVMESH {version} {name} 2 2']
        for identity,x,spawn in ((17,0,1),(29,128,0)):
            row=f'{identity} {x} 0 0 {crouch} {spawn} 1'
            if version==5:row+=f' 35 {low} {high} {role} 0'
            rows.append(row)
        rows+=['17 29','29 17'];(runtime/filename).write_text('\n'.join(rows)+'\n')
        result=subprocess.run([str(self.exe),str(home),scenario,name,filename],capture_output=True,text=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        report=json.loads((runtime/(name+'_navmesh.json')).read_text())
        return report,runtime/(name+'_navmesh.way')

    def test_real_adapter_parser_and_v9_serializer_keep_high_flags_ids_and_links(self):
        report,way=self.run_fixture(flags=1<<53)
        self.assertTrue(report['generation_complete'])
        import sys
        sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))
        from tce_navgen import read_way,inspect
        nodes,edges=read_way(way)
        self.assertEqual([n['flags'] for n in nodes],[1<<53,1<<53])
        self.assertEqual([n['properties']['tce_node'] for n in nodes],['17','29'])
        self.assertEqual(edges,{(0,1),(1,0)})
        audited=inspect(way.with_suffix('.json'))
        self.assertTrue(audited['binary_report_match'])
        self.assertTrue(audited['spatial_samples']['applicable'] is False) # manual format

    def test_wall_is_not_connected_through_clear_endpoint_floors(self):
        report,way=self.run_fixture('wall')
        self.assertFalse(report['generation_complete']);self.assertFalse(way.exists())
        self.assertTrue(all(n['native_input']['floor_accepted'] for n in report['nodes']))
        self.assertFalse(any(e['accepted'] for e in report['edges']))

    def test_automatic_crouch_passage_uses_full_duck_body_after_foot_probe(self):
        report,way=self.run_fixture('duck',version=4)
        self.assertTrue(report['generation_complete']);self.assertTrue(way.exists())
        self.assertTrue(all(n['crouch'] and n['nav_flags']==64 for n in report['nodes']))
        import sys
        sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))
        from tce_navgen import inspect
        self.assertTrue(inspect(way.with_suffix('.json'))['spatial_samples']['applicable'])

    def test_gap_is_rejected_for_walk_but_swept_jump_can_cross(self):
        walking,_=self.run_fixture('gap');self.assertFalse(walking['generation_complete'])
        jumping,way=self.run_fixture('jump',flags=512)
        self.assertTrue(jumping['generation_complete']);self.assertTrue(way.exists())
        self.assertTrue(all(e['reason']=='et_swept_ballistic_jump_live_pending' for e in jumping['edges']))

    def test_deep_water_and_unsupported_authored_roles_do_not_create_way(self):
        water,way=self.run_fixture('water');self.assertFalse(water['generation_complete']);self.assertFalse(way.exists())
        role,way=self.run_fixture('roles',role=16);self.assertFalse(role['generation_complete']);self.assertFalse(way.exists())
        self.assertTrue(all(n['reason']=='authored_action_role_requires_et_recipe' for n in role['nodes']))


if __name__=='__main__':main()
