"""Exercise actual append-only C repair/search using analytic collision fixtures.

No game/API or production module build. The graph fixture supplies already
validated authored records; real NG_Ground/NG_Walk perform all proposed moves.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_generator_capsule_support_native import program as support_program
from test_traversal_reset_native import function

ROOT=(Path(__file__).resolve().parents[1] / "src")


def program():
    source=(ROOT/'game/tce_navgen.c').read_text()
    external=(ROOT/'game/tce_navgen_external.inc').read_text()
    repair=(ROOT/'game/tce_navgen_pathrepair.inc').read_text()
    base=support_program().split('int main(int argc,char **argv){',1)[0]
    # A solid wall spanning y, bounded in x. Both authored endpoints are
    # outside its expanded hull; a real cross-wall sweep must still reject.
    base=base.replace('scenario==99&&a[0]+width>28.f+.001f',
                      'scenario==99&&fabsf(a[0]-32.f)<width+1.f-.001f')
    base=base.replace('scenario==99&&p[0]+width>28.f+.000001f',
                      'scenario==99&&fabsf(p[0]-32.f)<width+1.f-.000001f')
    base=base.replace('hit==3&&p[0]+width>28',
                      'hit==3&&fabsf(p[0]-32.f)<width+1.f')
    base=base.replace('static float support(float x,float radius,int capsule){',r'''
static float support(float x,float radius,int capsule){
 if(scenario==21){
  if(!capsule)return x<=radius?20.f:0.f;
  if(x<=0.f)return 20.f;
  if(x<radius){float lip=20.f-radius+sqrtf(radius*radius-x*x);return lip>0?lip:0;}
  return 0.f;
 }
''')
    # Configuration-space overhang: the tall hull must leave the ceiling's
    # horizontal expansion before the mixed terminal can be proved standing.
    base=base.replace('static void fixture_trace(',r'''
static float local_ceiling(float x,float width,float fallback){return scenario==20&&x+width>32.f?63.f:fallback;}
static void fixture_trace(''')
    base=base.replace('a[2]+height>ceiling', 'a[2]+height>local_ceiling(a[0],width,ceiling)')
    base=base.replace('p[2]+height>ceiling', 'p[2]+height>local_ceiling(p[0],width,ceiling)')
    edge=next(x for x in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;',source,re.S)
              if x.rsplit('}',1)[1].strip(' ;')=='ngEdge')
    return base+edge+r'''
#define NG_PORTAL_SEARCH 2048
typedef struct {ngNode node;int parent;float cost,score;qboolean closed;} ngPortalSearch;
static ngPortalSearch ngPortalSearchNodes[NG_PORTAL_SEARCH];
static ngNode ngNodes[NG_NODES];static ngEdge ngEdges[NG_EDGES];
static int ngCount,ngEdgeCount,ngExternalInputComplete,ngTrCacheFailed;
static int ngExternalManualDraft,ngSpawnSeeds,ngCampCount;
static unsigned char ngSpawnReachable[NG_NODES];
static char report[65536];static int reportLength;
static const char *va(const char *format,...) {static char line[8192];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);return line;}
static void NG_Text(const char *text){int n=(int)strlen(text);REQUIRE(reportLength+n<(int)sizeof(report),"bounded report");memcpy(report+reportLength,text,n+1);reportLength+=n;}
// Independent directed reachability for the already-authored graph fixture.
static void reachable(unsigned char *seen,int reverse) {
 int changed=1,i;memset(seen,0,NG_NODES);seen[0]=1;
 while(changed){changed=0;for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted){
  int a=reverse?ngEdges[i].to:ngEdges[i].from,b=reverse?ngEdges[i].from:ngEdges[i].to;
  if(seen[a]&&!seen[b]){seen[b]=1;changed=1;}
 }}
}
static void NG_SpawnReachability(void){reachable(ngSpawnReachable,0);}
static int NG_MainComponent(unsigned char *out){memset(out,1,NG_NODES);return ngCount;}
static int NG_TrDropTarget(int index){(void)index;return -1;}
static void Com_sprintf(char *out,int size,const char *format,...){va_list args;va_start(args,format);vsnprintf(out,size,format,args);va_end(args);}
static int NG_ExternalBuildComponents(unsigned char *out,qboolean actions){
 unsigned char forward[NG_NODES],backward[NG_NODES];int i;(void)actions;
 reachable(forward,0);reachable(backward,1);
 for(i=0;i<ngCount;i++)out[i]=forward[i]&&backward[i];return 0;
}
'''+function(source,'NG_Node')+'\n'+function(source,'NG_ImportOrdinaryCopy')+'\n'+function(source,'NG_OutputFlags')+'\n'+function(external,'NG_EditorJumpLaunchFloor')+'\n'+function(source,'NG_Camps')+'\n'+repair+r'''
int main(int argc,char **argv){
 int test=argc>1?atoi(argv[1]):0,i,k,oldEdges=2;ngNode original[2];ngEdge edges[2];
 scenario=test==4||test==7?99:0;
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;ngTraceBudget=2000000;
 ngExternalInputComplete=1;ngCount=2;ngEdgeCount=2;
 for(i=0;i<2;i++){
  ngNode *n=&ngNodes[i];n->id=7+i;n->editSourceId=n->id;n->authored=n->metadataSupplied=n->accepted=1;
  n->importUID=50+i;n->requestedRadius=n->radius=35;n->propertyHead=3;n->propertyCount=1;
  strcpy(n->name,i?"keep-target":"keep-spawn");VectorSet(n->facing,1,0,0);
  VectorSet(n->feet,64.f*i,0,.125f);VectorCopy(n->feet,n->raw);n->raw[2]+=30;
  n->reason="external_native_validated_floor";
 }
 for(i=0;i<2;i++){ngEdges[i].from=i;ngEdges[i].to=1-i;ngEdges[i].reason="previous_explicit_rejection";}
 if(test==1){ngNodes[1].editFlags=ngNodes[1].navFlags=1u<<27;ngNodes[1].editRole=1;ngNodes[1].radius=8;ngEdges[1].accepted=1;ngEdges[1].reason="manual_native_transition_live_pending";}
 if(test==2||test==3||test==12||test==13){ngNodes[1].editFlags=64;ngNodes[1].crouch=1;}
 if(test==2){ngNodes[0].editFlags=64;ngNodes[0].crouch=1;}
 if(test==5)ngTraceBudget=5000;
 if(test==12)scenario=3;
 if(test==13)scenario=20;
 if(test>=14){
  ngNode copy,candidate;vec3_t literal,ground;qboolean exact;
  memset(&candidate,0,sizeof(candidate));
  if(test==14){
   scenario=21;VectorSet(literal,.25f,0,20.125f);
   REQUIRE(!NG_Ground(literal,ground,ngStand),"high foot-box candidate itself is clear");
   REQUIRE(!NG_ImportOrdinaryCopy(literal,0,&copy,&exact)&&!strcmp(copy.reason,"blocked_clearance"),"real point-first floor pipeline rejects short-foot start inside ledge");
   VectorCopy(literal,candidate.feet);REQUIRE(!NG_PathRepairReimport(&candidate,0),"unimportable search bend rejected before append");
   VectorSet(literal,32,0,.125f);REQUIRE(NG_ImportOrdinaryCopy(literal,0,&copy,&exact)&&exact,"stable alternative passes identical import");
  }else if(test==15){
   scenario=3;VectorSet(literal,0,0,.125f);
   REQUIRE(!NG_ImportOrdinaryCopy(literal,0,&copy,&exact)&&!strcmp(copy.reason,"import_posture_mismatch"),"standing import cannot silently become crouch");
   REQUIRE(NG_ImportOrdinaryCopy(literal,1,&copy,&exact)&&exact&&copy.crouch,"explicit crouched import stays crouched");
  }else{
   VectorSet(literal,0,0,4.125f);
   REQUIRE(NG_ImportOrdinaryCopy(literal,0,&copy,&exact)&&!exact&&fabsf(copy.feet[2]-.125f)<.001f,"probe reports actual projected floor with exact false");
   VectorCopy(literal,candidate.feet);REQUIRE(!NG_PathRepairReimport(&candidate,0),"changed floor is not silently adopted by path repair");
  }
  printf("import%d passed\n",test);return 0;
 }
 if(test>=8&&test<=11){
  scenario=99;ngExternalManualDraft=test!=10;
  VectorSet(ngNodes[1].feet,0,128,.125f);ngEdges[0].accepted=1;
  ngNodes[0].authored=test==9;ngNodes[0].importUID=0;ngNodes[0].editSourceId=-1;
  if(test==11)ngNodes[0].metadataSupplied=0;
  if(test==9){ngNodes[0].editSourceId=ngNodes[0].id;strcpy(ngNodes[0].name,"keep-authored");}
  else ngNodes[0].name[0]=0;
  VectorClear(ngNodes[0].facing);original[0]=ngNodes[0];NG_Camps();
  if(test==10)REQUIRE(!strcmp(ngNodes[0].name,"tce2_camp_7")&&ngCampCount>0,"automatic camp derivation remains active");
  else REQUIRE(!memcmp(&original[0],&ngNodes[0],sizeof(ngNode)),"manual empty or named metadata survives camp pass");
  printf("camp%d passed\n",test);return 0;
 }
 memcpy(original,ngNodes,sizeof(original));memcpy(edges,ngEdges,sizeof(edges));
 NG_PathRepairReset();NG_PathRepair(test==6?8:1000);
 REQUIRE(!memcmp(original,ngNodes,sizeof(original)),"all old node bytes retained");
 REQUIRE(!memcmp(edges,ngEdges,sizeof(edges)),"all old rejected/accepted edge bytes retained");
 if(test!=6)REQUIRE(ngPathRepairPreserved,"input preservation receipt");
 if(test<=3||test==13){
  REQUIRE(ngCount>2&&ngEdgeCount>oldEdges,"fresh proved bend chain appended");
  REQUIRE(ngPathRepairChainCount==(test==1?1:2),"directed chains recorded exactly");
  for(i=2;i<ngCount;i++){
   REQUIRE(ngNodes[i].id>=1000&&ngNodes[i].accepted&&ngSpawnReachable[i],"new identity and spawn route");
   REQUIRE(!ngNodes[i].authored&&!ngNodes[i].importUID&&!ngNodes[i].propertyCount&&!ngNodes[i].editRole,"no inherited metadata/action recipe");
   REQUIRE(NG_OutputFlags(&ngNodes[i])==(test==2||test==3||test==13?64:0),"new stance follows endpoint contract");
   {ngNode copied;qboolean exact;REQUIRE(NG_ImportOrdinaryCopy(ngNodes[i].feet,ngNodes[i].crouch,&copied,&exact)&&exact,"every appended bend survives actual ordinary import");}
  }
  for(i=oldEdges;i<ngEdgeCount;i++){
   ngEdge *e=&ngEdges[i];REQUIRE(e->from>=2||e->to>=2,"never upgrade an old pair");
   if(test==1)REQUIRE(e->from!=1,"jump launch receives no walking exit");
   if(test==3||test==13){
    if(e->from==0||e->to==0)REQUIRE(!e->crouch,"mixed standing terminal fully proved standing");
    else REQUIRE(e->crouch,"interior crouch corridor keeps actual posture");
   }
   REQUIRE(!NG_Walk(&ngNodes[e->from],&ngNodes[e->to],e->crouch?ngDuck:ngStand),"every actual exported segment rechecks");
  }
  for(k=0;k<ngPathRepairChainCount;k++)REQUIRE(ngPathRepairChains[k].count<=NG_PATH_REPAIR_POINTS,"bounded compressed chain");
 }else REQUIRE(ngCount==2&&ngEdgeCount==2&&!ngPathRepairChainCount,"unproved/contract-changing path adds nothing");
 if(test==6){
  REQUIRE(!ngPathRepairPreserved&&!strcmp(ngPathRepairStop,"minimum_source_id_not_above_input"),"historical identity floor fails closed");
 }
 NG_Text("{");NG_PathRepairReport();NG_Text("\"test\":true}");puts(report);
 printf("case%d nodes%d edges%d body%d foot%d\n",test,ngCount,ngEdgeCount,body_calls,foot_calls);
 return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class PathRepairNativeTests(TestCase):
    def test_explicit_authored_format_gate_and_proposal_exit(self):
        source=(ROOT/'game/tce_navgen_external.inc').read_text()
        imported=function(source,'NG_ExternalImport')
        self.assertIn('if(repairPaths&&ngExternalVersion<5)',imported)
        self.assertIn('repairPaths=!Q_stricmp(option,"repair_paths")',imported)
        branch=imported.split('if(repairPaths) {',1)[1].split('return qtrue;',1)[0]
        self.assertIn('NG_Report(report,source,level.rawmapname,qfalse)',branch)
        for forbidden in ('NG_Save(', 'NG_Traversal(', 'NG_Camps(', 'NG_QuarantineUnreachable(', 'NG_FinalizeArrivalRadii('):
            self.assertNotIn(forbidden,branch)

    def test_actual_append_search_and_identity_contract(self):
        with tempfile.TemporaryDirectory(prefix='tce-pathrepair-') as folder:
            directory=Path(folder)
            (directory/'main.c').write_text(program())
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(PathRepair LANGUAGES C)\nadd_executable(pathrepair main.c)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                result=subprocess.run(command,**options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for case in range(17):
                with self.subTest(case=case):
                    result=subprocess.run([str(directory/'build/Release/pathrepair.exe'),str(case)],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    if 8<=case<=11:
                        self.assertIn(f'camp{case} passed',result.stdout)
                        continue
                    if case>=14:
                        self.assertIn(f'import{case} passed',result.stdout)
                        continue
                    receipt=json.loads(result.stdout.splitlines()[0])['path_repair']
                    self.assertEqual(receipt['model'],'native_append_paths_v1')
                    self.assertEqual(receipt['input_nodes'],2)
                    self.assertEqual(receipt['input_edges'],2)
                    for chain in receipt['chains']:
                        self.assertEqual(len(chain['crouch']),len(chain['source_ids'])-1)


if __name__=='__main__':main()
