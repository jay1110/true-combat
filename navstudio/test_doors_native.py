"""Compile actual native door discovery, pose restoration and Format6 gates.

Independent swept brush-box fixtures provide doors, a remote button, a wall,
a real unsupported gap and a low ceiling. No game/RCON/scripts run here.
The fixture collider is conservative box geometry, not an ET capsule parity
claim; actual NG_Ground/NG_Walk/NG_Doors/NG_Save functions execute unchanged.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

ROOT = (Path(__file__).resolve().parents[1] / "src")
FIXTURE = r'''
#include <stdarg.h>
#undef _vsnprintf
#undef vsnprintf
int Q_vsnprintf(char *d,int n,const char *f,va_list a){return _vsnprintf(d,n,f,a);}
level_locals_t level;gentity_t g_entities[MAX_GENTITIES];
vmCvar_t g_newbbox,g_developer,g_gravity,g_speed,g_realism,g_gametype,g_movespeed;
static int scenario;static unsigned char output[1024*1024];static int outputLength;static const char *inputDocument;
void Use_BinaryMover(gentity_t *e,gentity_t *o,gentity_t *a){exit(91);}
void target_relay_use(gentity_t *e,gentity_t *o,gentity_t *a){exit(90);}
void use_invisible_user(gentity_t *e,gentity_t *o,gentity_t *a){exit(89);}
int trap_Milliseconds(void){return 0;}
int trap_Argc(void){exit(92);return 0;}
void trap_Argv(int n,char *b,int l){exit(93);}
int trap_FS_FOpenFile(const char *p,fileHandle_t *f,fsMode_t m){if(m==FS_WRITE){*f=1;outputLength=0;return 0;}if(inputDocument&&!strcmp(p,"candidate")){*f=2;return (int)strlen(inputDocument);}*f=0;return -1;}
void trap_FS_Read(void *b,int n,fileHandle_t f){if(f!=2||!inputDocument)exit(94);memcpy(b,inputDocument,n);}
int trap_FS_Write(const void *b,int n,fileHandle_t f){if(outputLength+n>(int)sizeof(output))exit(95);memcpy(output+outputLength,b,n);outputLength+=n;return n;}
void trap_FS_FCloseFile(fileHandle_t f){}
int trap_Cvar_VariableIntegerValue(const char *n){exit(96);return 0;}
void QDECL G_Printf(const char *f,...){ }
void QDECL Com_Printf(const char *f,...){ }
void QDECL Com_Error(int e,const char *f,...){exit(97);}
int trap_EntitiesInBox(const vec3_t a,const vec3_t b,int *list,int max){return 0;}
int trap_PointContents(const vec3_t p,int i){return 0;}
qboolean trap_EntityContactCapsule(const vec3_t mins,const vec3_t maxs,const gentity_t *ent){int k;
 for(k=0;k<3;k++)if(maxs[k]<=ent->r.absmin[k]||mins[k]>=ent->r.absmax[k])return qfalse;return qtrue;
}
void trap_UnlinkEntity(gentity_t *e){e->r.linked=qfalse;}
void trap_LinkEntity(gentity_t *e){int k;e->r.linked=qtrue;e->r.linkcount++;
 for(k=0;k<3;k++){e->r.absmin[k]=e->r.currentOrigin[k]+e->r.mins[k];e->r.absmax[k]=e->r.currentOrigin[k]+e->r.maxs[k];}
 // Independent transformed brush bounds, exact at the fixture's axial endpoints.
 if(e->r.currentAngles[1]!=0.f){int corner;float yaw=e->r.currentAngles[1]*(float)M_PI/180.f,c=cos(yaw),s=sin(yaw);
  VectorSet(e->r.absmin,1e20f,1e20f,1e20f);VectorSet(e->r.absmax,-1e20f,-1e20f,-1e20f);
  for(corner=0;corner<8;corner++){vec3_t p;float x=corner&1?e->r.maxs[0]:e->r.mins[0],y=corner&2?e->r.maxs[1]:e->r.mins[1];
   VectorSet(p,x*c-y*s+e->r.currentOrigin[0],x*s+y*c+e->r.currentOrigin[1],(corner&4?e->r.maxs[2]:e->r.mins[2])+e->r.currentOrigin[2]);
   for(k=0;k<3;k++){if(p[k]<e->r.absmin[k])e->r.absmin[k]=p[k];if(p[k]>e->r.absmax[k])e->r.absmax[k]=p[k];}
  }
 }
}
static void brush(trace_t *tr,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,const vec3_t lo,const vec3_t hi,int entity){
 float enter=-1e20f,leave=1e20f;int k,axis=-1,inside=1;float sign=0;
 for(k=0;k<3;k++){float low=lo[k]-(ma?ma[k]:0),high=hi[k]-(mi?mi[k]:0),d=b[k]-a[k],t1,t2,s=-1;
  if(a[k]<=low||a[k]>=high)inside=0;
  if(fabs(d)<1e-9f){if(a[k]<=low||a[k]>=high)return;continue;}
  t1=(low-a[k])/d;t2=(high-a[k])/d;if(t1>t2){float swap=t1;t1=t2;t2=swap;s=1;}
  if(t1>enter){enter=t1;axis=k;sign=s;}if(t2<leave)leave=t2;if(enter>leave)return;
 }
 if(inside){tr->startsolid=qtrue;tr->allsolid=leave>=1;tr->fraction=0;tr->entityNum=entity;VectorCopy(a,tr->endpos);return;}
 if(enter<0||enter>tr->fraction||leave<0||axis<0)return;
 tr->fraction=enter;tr->entityNum=entity;VectorSubtract(b,a,tr->endpos);VectorMA(a,enter,tr->endpos,tr->endpos);VectorClear(tr->plane.normal);tr->plane.normal[axis]=sign;tr->contents=CONTENTS_SOLID;
}
static void collide(trace_t *tr,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,int mask){
 int n;float low=mi?mi[2]:0;vec3_t lo,hi;memset(tr,0,sizeof(*tr));tr->fraction=1;tr->entityNum=ENTITYNUM_WORLD;VectorCopy(b,tr->endpos);
 if(!(scenario==2&&fabs(a[0])<24.f||scenario==10&&a[0]>0.f)){
  if(a[2]+low<.125f){tr->startsolid=tr->allsolid=qtrue;tr->fraction=0;VectorCopy(a,tr->endpos);}
  else if(b[2]+low<.125f){tr->fraction=(a[2]+low-.125f)/(a[2]-b[2]);VectorSubtract(b,a,tr->endpos);VectorMA(a,tr->fraction,tr->endpos,tr->endpos);tr->plane.normal[2]=1.f;}
 }
 for(n=MAX_CLIENTS;n<level.num_entities;n++)if(g_entities[n].inuse&&g_entities[n].r.linked&&(g_entities[n].r.contents&mask))
  brush(tr,a,mi,ma,b,g_entities[n].r.absmin,g_entities[n].r.absmax,n);
 if(scenario==3){VectorSet(lo,-5,-80,0);VectorSet(hi,5,80,160);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);}
 if(scenario==4){VectorSet(lo,-256,-256,70);VectorSet(hi,256,256,80);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);}
 if(scenario==8){VectorSet(lo,-96,80,0);VectorSet(hi,-70,112,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);}
 if(scenario==9||scenario==10){
  VectorSet(lo,-4,-512,0);VectorSet(hi,4,-64,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);
  VectorSet(lo,-4,64,0);VectorSet(hi,4,512,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);
 }
 if(scenario==11||scenario==12||scenario==14){
  VectorSet(lo,-256,-4,0);VectorSet(hi,0,96,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);
  VectorSet(lo,48,-4,0);VectorSet(hi,256,96,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);
  if(scenario==12){
   VectorSet(lo,40,-64,0);VectorSet(hi,44,-20,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);
   VectorSet(lo,-20,-64,0);VectorSet(hi,-16,-20,128);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);
  }
 }
 if(scenario==13){VectorSet(lo,-40,-80,70);VectorSet(hi,40,80,80);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);}
}
void trap_Trace(trace_t *t,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,int i,int m){collide(t,a,mi,ma,b,m);}
void trap_TraceCapsule(trace_t *t,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,int i,int m){collide(t,a,mi,ma,b,m);}
static void require(int okay,const char *name){if(!okay){fprintf(stderr,"NATIVE DOOR: %s scenario=%d\n",name,scenario);exit(9);}}
#define REQUIRE(c,n) require((c),(n))
static void setup(int test){gentity_t *door;int i;scenario=test;memset(g_entities,0,sizeof(g_entities));memset(&level,0,sizeof(level));level.num_entities=68;g_newbbox.integer=1;g_gametype.integer=2;NG_ExternalReset();ngSpawnSeeds=2;ngCount=2;
 for(i=0;i<2;i++){ngNodes[i].id=i;ngNodes[i].accepted=qtrue;ngNodes[i].reason="map_entity_spawn_seed";ngNodes[i].index=-1;VectorSet(ngNodes[i].feet,i?96:-96,0,.25f);}
 door=&g_entities[64];door->inuse=qtrue;door->classname="func_door";door->use=Use_BinaryMover;door->s.number=64;door->s.modelindex=1;door->r.bmodel=qtrue;door->r.contents=CONTENTS_SOLID;door->moverState=MOVER_POS1;VectorSet(door->pos2,0,160,0);VectorSet(door->r.mins,-4,-64,0);VectorSet(door->r.maxs,4,64,128);trap_LinkEntity(door);
 if(test==1)door->key=-1;
 if(test==5)door->allowteams=1;
 if(test==6){gentity_t *button=&g_entities[65];door->targetname="door";button->inuse=qtrue;button->classname="func_button";button->use=Use_BinaryMover;button->target="door";button->s.modelindex=2;button->s.number=65;button->r.bmodel=qtrue;button->r.contents=CONTENTS_SOLID;VectorSet(button->r.currentOrigin,-96,-48,0);VectorSet(button->r.mins,-4,-4,56);VectorSet(button->r.maxs,4,4,64);trap_LinkEntity(button);}
 if(test==7||test==8){VectorSet(ngNodes[0].feet,-112,192,.25f);VectorSet(ngNodes[1].feet,112,192,.25f);}
 if(test==9||test==10){ngCount=1;ngSpawnSeeds=1;VectorSet(ngNodes[0].feet,-112,192,.25f);}
 if(test==11||test==12||test==14){
  VectorSet(ngNodes[0].feet,24,-80,.25f);VectorSet(ngNodes[1].feet,24,80,.25f);
  door->classname="func_door_rotating";door->moverState=MOVER_POS1ROTATE;door->spawnflags=32;door->rotate[1]=1;door->angle=-90;
  VectorClear(door->r.currentOrigin);VectorSet(door->r.mins,0,-4,0);VectorSet(door->r.maxs,48,4,128);trap_LinkEntity(door);
 }
 if(test==14){ngNodes[0].authored=ngNodes[1].authored=qtrue;ngNodes[0].crouch=qtrue;ngNodes[0].editFlags=64;}
 if(test==13)for(i=0;i<2;i++){
  ngNodes[i].authored=qtrue;ngNodes[i].editSourceId=ngNodes[i].id;ngNodes[i].editFlags=0;
  ngNodes[i].radius=ngNodes[i].requestedRadius=35;ngNodes[i].importUID=700+i;
  sprintf(ngNodes[i].name,"authored_standing_%d",i);VectorSet(ngNodes[i].facing,.6f,.8f,0);
 }
}
int main(int argc,char **argv){int test,i,k;entityShared_t before;ngDoorPose pose;int team;const char *why;
 for(test=0;test<=6;test++){setup(test);before=g_entities[64].r;NG_Doors();REQUIRE(!memcmp(&before,&g_entities[64].r,sizeof(before)),"whole shared state/linkcount/pose restored");
  if(test==1||test==2||test==3){REQUIRE(ngDoorLinks==0&&ngCount==2,"locked/gap/wall cannot create fake passage or leave orphan approaches");continue;}
  REQUIRE(ngDoorLinks==(test==6?1:2),"direction-specific physical generation");
  for(i=2;i<ngCount;i++)if(ngNodes[i].door.role==2){ngDoorRecipe *r=&ngNodes[i].door;REQUIRE(r->validated&&NG_Find(r->target)>=0,"real directed recipe source refs");if(test==5)REQUIRE((ngNodes[i].navFlags&19u)==17u,"axis-only flag enforces planner gate");if(test==4)REQUIRE(ngNodes[i].crouch,"low doorway generates real crouch stance");if(test==6)REQUIRE(r->mode==2&&NG_DoorOrdinaryReach(i,NG_Find(r->useSource),r->team),"button reachable without conditional crossing");}
  if(test==6){int exits=0,uses=0;for(i=0;i<ngCount;i++){exits+=ngNodes[i].door.role==3;uses+=ngNodes[i].door.role==1;}REQUIRE(exits==1&&uses==1,"remote-only opposite room not invented as return");}
  REQUIRE(NG_DoorManualFinalize(),"actual recipe revalidation");
  if(test==4)for(i=2;i<ngCount;i++)if(ngNodes[i].door.role==2){
   ngNode a=ngNodes[i],b=ngNodes[NG_Find(a.door.target)],savedA,savedB;a.crouch=b.crouch=qfalse;savedA=a;savedB=b;
   REQUIRE(!NG_DoorGroup(64,&pose,&team),"saved low-door stance fixture uses actual mover");why=NG_DoorPassage(&pose,&a,&b);
   REQUIRE(why&&!strcmp(why,"door_saved_stance_requires_crouch"),"already-bound standing recipe cannot borrow a duck-only passage");
   REQUIRE(!memcmp(&a,&savedA,sizeof(a))&&!memcmp(&b,&savedB,sizeof(b)),"failed saved passage never rewrites either stance or saved aim");break;
  }
  NG_ExternalDiscardUnusedActionSamples(2);for(i=2;i<ngCount;i++)if(ngNodes[i].door.mode)REQUIRE(ngNodes[i].accepted,"door recipe survives real action-sample cleanup");
 }
 setup(0);NG_Doors();for(i=0;i<ngCount;i++)ngNodes[i].index=i;
 REQUIRE(NG_Save("navgen/test.way",ngCount),"actual WAY writer retains recipes");
 if(argc>1){FILE *file=fopen(argv[1],"wb");REQUIRE(file!=NULL,"fixture output");fwrite(output,1,outputLength,file);fclose(file);}
 {char document[32768];int offset=0,edgeCount=0;const char *bad;
  for(k=0;k<ngEdgeCount;k++)edgeCount+=ngEdges[k].accepted;
  offset+=sprintf(document+offset,"TCE_NAVMESH 6 obj_test %d %d\n",ngCount,edgeCount);
  for(i=0;i<ngCount;i++){const ngNode *n=&ngNodes[i];const ngDoorRecipe *r=&n->door;
   offset+=sprintf(document+offset,"%d %.4f %.4f %.4f %d %d %d %.4f %u 0 0 0 %d %d %d %d %d %d %d %d %d %d %.4f %.4f %.4f\n",
    n->id,n->feet[0],n->feet[1],n->feet[2],n->crouch,i<2,n->topologyProtected,n->radius>0?n->radius:35,(unsigned int)NG_OutputFlags(n),
    r->mode?r->entity+1:0,r->mode?r->useEntity+1:0,r->mode?r->target+1:0,r->mode,r->team,r->role,r->model,r->useModel,r->mode?r->useSource+1:0,r->mode?r->entry+1:0,r->aim[0],r->aim[1],r->aim[2]);
  }
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted)offset+=sprintf(document+offset,"%d %d\n",ngNodes[ngEdges[k].from].id,ngNodes[ngEdges[k].to].id);
  inputDocument=document;strcpy(level.rawmapname,"obj_test");NG_ExternalReset();
  REQUIRE(NG_ExternalRead("candidate"),"actual Format6 parser roundtrip");REQUIRE(ngExternalVersion==6&&ngExternalManualDraft,"Format6 authored mode");
  for(i=0;i<ngCount;i++){NG_Node(&ngNodes[i]);REQUIRE(ngNodes[i].accepted,"actual reimported point floor");}
  for(k=0;k<ngEdgeCount;k++){bad=NG_EditorEdge(&ngEdges[k]);REQUIRE(bad==NULL,"actual reimported directed transition proof");ngEdges[k].accepted=qtrue;}
  REQUIRE(NG_DoorManualFinalize(),"actual Format6 recipe references and physical revalidation");
  ngNodes[2].door.model=2;REQUIRE(!NG_DoorManualFinalize(),"changed door model cannot borrow old receipt");inputDocument=NULL;
 }
 // Moving, shoot-only, cyclic and noncanonical rotating states are explicit failures.
 setup(0);g_entities[64].moverState=MOVER_1TO2;REQUIRE(NG_DoorGroup(64,&pose,&team)!=NULL,"moving door not guessed");
 setup(0);g_entities[64].takedamage=qtrue;REQUIRE(NG_DoorGroup(64,&pose,&team)!=NULL,"shoot-only door not guessed");
 setup(6);g_entities[66].inuse=qtrue;g_entities[66].classname="target_relay";g_entities[66].use=target_relay_use;g_entities[66].targetname="relay";g_entities[66].target="relay";g_entities[65].target="relay";REQUIRE(NG_DoorRelay(65,64,&team)!=NULL,"cyclic relay rejected");
 setup(6);g_entities[66].inuse=qtrue;g_entities[66].classname="target_relay";g_entities[66].use=target_relay_use;g_entities[66].targetname="relay";g_entities[66].target="door";g_entities[65].target="relay";REQUIRE(!NG_DoorRelay(65,64,&team),"actual simple relay target resolution");REQUIRE(TCE_NavDoorUseMatches(65,64,1),"runtime shares real relay binding");g_entities[66].spawnflags=4;REQUIRE(NG_DoorRelay(65,64,&team)!=NULL,"random relay rejected");REQUIRE(!TCE_NavDoorUseMatches(65,64,1),"runtime rejects changed relay");
 setup(0);g_entities[64].classname="func_door_rotating";g_entities[64].moverState=MOVER_POS1ROTATE;g_entities[64].rotate[1]=1;g_entities[64].angle=90;g_entities[64].r.currentOrigin[0]=123;trap_LinkEntity(&g_entities[64]);before=g_entities[64].r;
 REQUIRE(!NG_DoorGroup(64,&pose,&team),"known rotating group");NG_DoorPoseSet(&pose,1,1);REQUIRE(g_entities[64].r.currentOrigin[0]==123&&g_entities[64].r.currentAngles[1]==90,"actual hinge origin, not unset pos1");NG_DoorPoseRestore(&pose);REQUIRE(!memcmp(&before,&g_entities[64].r,sizeof(before)),"rotating full restoration");
 g_entities[64].r.currentAngles[1]=30;REQUIRE(NG_DoorGroup(64,&pose,&team)!=NULL,"noncanonical closed orientation not invented");
 setup(0);g_entities[64].classname="func_door_rotating";g_entities[64].moverState=MOVER_POS1ROTATE;g_entities[64].rotate[1]=1;g_entities[64].angle=90;
 VectorSet(g_entities[64].r.currentOrigin,0,-64,0);VectorSet(g_entities[64].r.mins,-4,0,0);VectorSet(g_entities[64].r.maxs,4,128,128);trap_LinkEntity(&g_entities[64]);
 {ngNode entry=ngNodes[0],bad=ngNodes[0],good=ngNodes[0];VectorSet(entry.feet,-26,0,.25f);VectorSet(bad.feet,-96,-100,.25f);VectorSet(good.feet,-96,0,.25f);
  REQUIRE(!NG_DoorGroup(64,&pose,&team),"hinged quarter-turn fixture");before=g_entities[64].r;
  REQUIRE(!NG_DoorAnchorWalk(&pose,&entry,&bad),"closed clear anchor crossing opened leaf must reject");
  REQUIRE(NG_DoorAnchorWalk(&pose,&entry,&good),"same-room short anchor clear in both actual open signs");
  REQUIRE(!memcmp(&before,&g_entities[64].r,sizeof(before)),"all anchor pose traces restored");
 }
 {vec3_t inside,outside;int traces,budget,proof;
  VectorSet(inside,26,0,.25f);VectorSet(outside,96,96,.25f);
  REQUIRE(!NG_DoorSweepFootClear(&pose,inside,16,87,2),"endpoints-clear old EXIT is inside real intermediate rotor poses");
  REQUIRE(NG_DoorSweepFootClear(&pose,outside,16,87,2),"diagonal exit outside actual full rotor sweep");
  traces=ngTraces;budget=ngTraceBudget;proof=ngDoorProofTraces;
  REQUIRE(TCE_NavDoorSweepSafe(64,outside,16,87,2)&&!TCE_NavDoorSweepSafe(64,inside,16,87,2),"runtime physical capsule uses same complete pose gate");
  REQUIRE(traces==ngTraces&&budget==ngTraceBudget&&proof==ngDoorProofTraces&&!memcmp(&before,&g_entities[64].r,sizeof(before)),"sweep runtime query preserves all accounting and collision state");
  ngNodes[0].feet[0]=-96;ngNodes[1].feet[0]=96;NG_Doors();REQUIRE(ngDoorLinks==2,"rotating generation finds sweep-safe paired exits");
  for(i=0;i<ngCount;i++)if(ngNodes[i].door.role==2){int target=NG_Find(ngNodes[i].door.target);REQUIRE(target>=0&&ngNodes[target].door.role==3&&ngNodes[target].radius==2,"dedicated precision sweep-safe EXIT");REQUIRE(NG_DoorSweepFootClear(&pose,ngNodes[target].feet,16,87,2),"every generated rotor target stays safe during natural closure");}
 }
 ngCount=2;ngEdgeCount=0;g_entities[64].spawnflags=32;REQUIRE(TCE_NavDoorUseMatches(64,64,1),"actual FORCE door still accepts direct activate");
 REQUIRE(!NG_DoorGroup(64,&pose,&team)&&NG_DoorFixedRotation(&pose),"FORCE master fixes actual opening sign");
 {ngNode entry=ngNodes[1],near=ngNodes[0],anchor=ngNodes[0],exit;
  VectorSet(entry.feet,26,-48,.25f);VectorSet(near.feet,-26,-48,.25f);VectorSet(anchor.feet,-96,0,.25f);
  REQUIRE(!NG_DoorAnchorWalk(&pose,&near,&anchor),"opposite provisional near-USE floor intersects actual opened leaf");
  REQUIRE(NG_DoorSafeExit(&pose,&entry,&near,&anchor,&exit,NG_TRACE_BUDGET),"actual safe exit binds real network floor, not provisional opposite USE entry");
  REQUIRE(NG_DoorAnchorWalk(&pose,&exit,&anchor)&&NG_DoorSweepFootClear(&pose,exit.feet,16,87,2),"safe exit retains full network walk and complete swept-body proof");
 }
 {vec3_t safe;VectorSet(safe,26,0,.25f);
  REQUIRE(NG_DoorSweepFootClear(&pose,safe,16,87,2),"actual fixed FORCE swing leaves its opposite wait body clear");
  g_entities[64].spawnflags=0;REQUIRE(!NG_DoorSweepFootClear(&pose,safe,16,87,2),"ordinary rotor must still prove both possible opening signs");
  g_entities[64].spawnflags=32;
 }
 NG_Doors();REQUIRE(ngDoorLinks>0,"FORCE generation finds physically swept-safe direct use floors");
 for(i=0;i<ngCount;i++)if(ngNodes[i].door.role==2&&ngNodes[i].door.mode==1||ngNodes[i].door.role==1&&ngNodes[i].door.mode==3){
  REQUIRE(NG_DoorSweepFootClear(&pose,ngNodes[i].feet,ngWidth,ngNodes[i].crouch?ngDuck:ngStand,2),"every FORCE wait body outside actual complete swing");
 }
 REQUIRE(NG_DoorManualFinalize(),"FORCE recipes retain native use/wait/passage proof on authored recheck");
 for(i=0;i<ngCount;i++)if(ngNodes[i].door.role==2){ngNode unsafe=ngNodes[i];
  VectorSet(unsafe.feet,-26,0,.25f);why=NG_DoorRecipeIdentity(&unsafe,&pose);
  REQUIRE(why&&!strcmp(why,"force_rotating_entry_inside_motion_sweep"),"unsafe manual FORCE entry cannot borrow a recipe");break;
 }
 REQUIRE(!NG_DoorGroup(64,&pose,&team),"FORCE group metadata remains usable for remote proof");
 {ngNode near=ngNodes[0],far=ngNodes[0];VectorSet(near.feet,0,0,.25f);VectorSet(far.feet,-400,0,.25f);REQUIRE(!NG_DoorForceUseSafe(&pose,&near),"remote FORCE waiter inside rotating envelope rejected");REQUIRE(NG_DoorForceUseSafe(&pose,&far),"remote FORCE waiter fully outside envelope");}
 // A small physical rotating leaf reaches into the initial near-face seed.
 // The ordinary face seed must move back; the USE entry remains separate.
 setup(0);g_entities[64].classname="func_door_rotating";g_entities[64].moverState=MOVER_POS1ROTATE;g_entities[64].spawnflags=32;g_entities[64].rotate[1]=1;g_entities[64].angle=90;
 VectorSet(g_entities[64].r.currentOrigin,0,-24,0);VectorSet(g_entities[64].r.mins,-4,0,0);VectorSet(g_entities[64].r.maxs,4,48,128);trap_LinkEntity(&g_entities[64]);
 {ngDoorNeighbor seeds[2];qboolean bootstrap;vec3_t centre;int maxid=200;
  REQUIRE(!NG_DoorGroup(64,&pose,&team),"small real FORCE rotor supports local surface proof");
  VectorAdd(g_entities[64].r.absmin,g_entities[64].r.absmax,centre);VectorScale(centre,.5f,centre);centre[2]=g_entities[64].r.absmin[2];
  REQUIRE(NG_DoorFaceStage(&pose,centre,0,0,2,seeds,&maxid,NG_TRACE_BUDGET,&bootstrap),"local surface moves ordinary waiting floors outside actual rotor sweep");
  REQUIRE(ngNodes[seeds[0].index].feet[0]<-52.f,"pull-side ordinary face seed moves behind the initial tight near-face proposal");
  for(i=0;i<2;i++)REQUIRE(NG_DoorSweepFootClear(&pose,ngNodes[seeds[i].index].feet,16,87,2),"every staged ordinary wait floor proves complete moving-leaf body clearance");
 }
 // Explicit repair preserves the current graph and never duplicates recipes.
 setup(0);ngNodes[0].id=100;ngNodes[1].id=900;
 REQUIRE(NG_DoorRepair(1200)&&ngDoorRepairRun&&ngDoorLinks==2,"explicit repair adds only proved door actions");
 REQUIRE(ngNodes[0].id==100&&ngNodes[1].id==900&&ngNodes[0].feet[0]==-96&&ngNodes[1].feet[0]==96,"authored source IDs and positions stay unchanged");
 for(i=2;i<ngCount;i++)REQUIRE(ngNodes[i].id>=1200,"new door IDs respect deleted-history next_id floor");
 {int count=ngCount,edges=ngEdgeCount;REQUIRE(NG_DoorRepair(2000),"repeat repair accepts current next_id");
  REQUIRE(ngCount==count&&ngEdgeCount==edges&&!ngDoorLinks,"repeat repair does not recreate existing authored actions");
  REQUIRE(ngDoorCaseCount==1&&!strcmp(ngDoorCases[0].reason,"existing_authored_door_recipe_preserved"),"existing recipe preserved explicitly in report");
  REQUIRE(!NG_DoorRepair(-1)&&!NG_DoorRepair(0x7fffffff)&&ngCount==count&&ngEdgeCount==edges,"invalid ID floor rejected without graph mutation");
  ngNodes[0].id=0x7fffffff;REQUIRE(!NG_DoorRepair(0)&&ngCount==count&&ngEdgeCount==edges,"existing maximum cannot overflow appended source ID");
 }
 // A standing direct USE near the actual activation limit must retain its
 // exact eye height when a genuinely checked crouched floor is connected.
 setup(0);NG_Doors();
 for(i=0;i<ngCount;i++)if(ngNodes[i].door.mode==1&&ngNodes[i].door.role==2){
  int count=ngCount,edges=ngEdgeCount;ngNode original=ngNodes[i],saved,neighbor=ngNodes[0];vec3_t aim;
  ngNodes[i].feet[0]=ngNodes[i].feet[0]<0?-38.f:38.f;ngNodes[i].crouch=qfalse;
  REQUIRE(NG_DoorAim(&ngNodes[i],64,qtrue,aim,qfalse),"standing direct USE really hits its door within actual38.4 range");VectorCopy(aim,ngNodes[i].door.aim);saved=ngNodes[i];
  memset(&neighbor.door,0,sizeof(neighbor.door));neighbor.id=3100;neighbor.crouch=qtrue;neighbor.authored=qtrue;neighbor.editFlags=64;
  VectorCopy(ngNodes[i].feet,neighbor.feet);ngNodes[ngCount++]=neighbor;
  REQUIRE(!NG_DoorGroup(64,&pose,&team)&&NG_DoorAnchorWalk(&pose,&ngNodes[i],&ngNodes[count]),"mixed standing-use/crouched-floor link retains actual closed/open whole-body proof");
  REQUIRE(!NG_DoorRecipeIdentity(&ngNodes[i],&pose),"standing direct USE saved aim is valid before linking");
  NG_DoorLink(i,count,"actual_mixed_stance_approach");NG_DoorLink(count,i,"actual_mixed_stance_approach");
  REQUIRE(ngEdges[edges].crouch&&!memcmp(&saved,&ngNodes[i],sizeof(saved)),"a crouched approach marks only its edge and cannot mutate any standing USE field");
  REQUIRE(!NG_DoorRecipeIdentity(&ngNodes[i],&pose),"standing direct USE retains its exact saved aim after linking");
  {ngNode bad=ngNodes[i];bad.crouch=qtrue;why=NG_DoorRecipeIdentity(&bad,&pose);REQUIRE(why&&!strcmp(why,"use_aim_occluded_or_out_of_range"),"real aim geometry detects the old accidental eye-height mutation");}
  ngNodes[i]=original;ngCount=count;ngEdgeCount=edges;break;
 }
 // No old seed-to-seed line intersects the door. The actual closed face
 // supplies a local proposal, and an independent wall demands a real elbow.
 for(test=7;test<=8;test++){int guides=0;setup(test);REQUIRE(NG_DoorRepair(1500)&&ngDoorLinks==2,"local surface finds door missed by all existing straight seed pairs");
  REQUIRE(!NG_DoorGroup(64,&pose,&team),"local surface test uses current actual door pose");
  REQUIRE(ngNodes[0].feet[0]==-112&&ngNodes[0].feet[1]==192&&ngNodes[1].feet[0]==112&&ngNodes[1].feet[1]==192,"local repair preserves authored seed positions");
  for(i=2;i<ngCount;i++)if(!ngNodes[i].door.mode){int incoming=0,outgoing=0;
   REQUIRE(ngNodes[i].topologyProtected&&!strcmp(ngNodes[i].reason,"generated_door_approach_live_pending"),"every ordinary approach explicitly identified");
   for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted){incoming+=ngEdges[k].to==i;outgoing+=ngEdges[k].from==i;}
   REQUIRE(incoming&&outgoing&&ngNodes[i].id>=1500&&NG_DoorApproachGuide(i),"every added approach is bound to real directed door routes");guides++;
   REQUIRE(NG_DoorSweepFootClear(&pose,ngNodes[i].feet,ngWidth,ngNodes[i].crouch?ngDuck:ngStand,2.f),"ordinary local surface guide remains outside complete moving leaf sweep");
  }
  REQUIRE(guides>=(test==8?3:2),"blocked straight approach gets a genuinely checked bend");
  REQUIRE(NG_DoorManualFinalize(),"surface/turn approach retains physical door proof");
  {ngNode unrelated=ngNodes[0];int scratchId=5000,orphan=NG_DoorAppend(&unrelated,&scratchId);ngNodes[orphan].radius=35;
   REQUIRE(!NG_DoorApproachGuide(orphan),"an unbound ordinary search proposal is never certified as a door guide");ngCount--;
  }
  NG_ExternalDiscardUnusedActionSamples(2);
  {int retained=0;for(i=2;i<ngCount;i++)retained+=NG_DoorApproachGuide(i);
   REQUIRE(retained==guides,"actual external action cleanup preserves every physically bound face/bend guide");
  }
 }
 // A genuinely closed partition has no old node on its inside. A native
 // local floor can be bootstrapped only with complete entry AND return.
 for(test=9;test<=10;test++){int inside=0;setup(test);before=g_entities[64].r;
  REQUIRE(NG_DoorRepair(1800),"empty-side repair accepts bounded source IDs");
  REQUIRE(!memcmp(&before,&g_entities[64].r,sizeof(before)),"empty-side search restores actual collision pose");
  REQUIRE(ngNodes[0].feet[0]==-112&&ngNodes[0].feet[1]==192,"empty-side search preserves the sole authored anchor");
  if(test==10){REQUIRE(!ngDoorLinks&&ngCount==1&&!ngEdgeCount,"missing real inner floor cannot leave recipes or orphan guides");continue;}
  REQUIRE(ngDoorLinks==2,"empty room receives actual opening and return recipes without an authored inside point");
  for(i=1;i<ngCount;i++)if(!ngNodes[i].door.mode){
   REQUIRE(NG_DoorApproachGuide(i),"each bootstrapped ordinary point has incoming and outgoing validated recipe bindings");
   if(ngNodes[i].feet[0]>0)inside++;
  }
  REQUIRE(inside>0&&NG_DoorManualFinalize(),"new inner floor retains native floor/use/sweep/exit/return validation");
  NG_ExternalDiscardUnusedActionSamples(1);
  for(i=1;i<ngCount;i++)REQUIRE(ngNodes[i].accepted,"actual external cleanup preserves only fully bound empty-room navigation");
 }
 // Actual 48-unit jambs: safe lateral USE cannot walk straight through the
 // portal, but a separate aligned ENTRY and fully-open passage are legal.
 for(test=11;test<=14;test++){int local=0;if(test==13)continue;setup(test);before=g_entities[64].r;
  REQUIRE(NG_DoorRepair(2200),"local staged direct-use discovery accepts bounded ID floor");
  REQUIRE(!memcmp(&before,&g_entities[64].r,sizeof(before)),"local staged search restores complete real collision state");
  REQUIRE(!NG_DoorGroup(64,&pose,&team),"local staged fixture retains actual fixed-sign rotor identity");
  for(i=2;i<ngCount;i++)if(ngNodes[i].door.mode==3&&ngNodes[i].door.role==2){
   int use=NG_Find(ngNodes[i].door.useSource),exit=NG_Find(ngNodes[i].door.target);ngNode original=ngNodes[i],bad;vec3_t aim;
   REQUIRE(use>=0&&exit>=0&&ngNodes[use].door.role==1&&ngNodes[exit].door.role==3,"local staged recipe keeps all three actual roles and source refs");
   REQUIRE(ngNodes[use].door.entity==64&&ngNodes[use].door.useEntity==64&&ngNodes[use].door.target==ngNodes[i].id,"local activation is the real door, never a fake button");
   REQUIRE(NG_DoorSweepFootClear(&pose,ngNodes[use].feet,16,87,2)&&!NG_DoorSweepFootClear(&pose,ngNodes[i].feet,16,87,2),"only actual safe USE waits; aligned ENTRY may lie inside intermediate swing");
   REQUIRE(NG_DoorAim(&ngNodes[use],64,qtrue,aim,qfalse),"local USE must satisfy actual direct 38.4 eye-hit range");
   REQUIRE(NG_DoorAnchorWalk(&pose,&ngNodes[use],&original)&&NG_DoorPassage(&pose,&original,&ngNodes[exit])==NULL,"local lateral ground leg and real fully-open passage separately proved");
   REQUIRE(NG_DoorPassage(&pose,&ngNodes[use],&ngNodes[exit])!=NULL,"independent jamb really rejects the old direct USE-to-EXIT chord");
   REQUIRE(NG_HasEdge(use,i)&&NG_HasEdge(i,use)&&!NG_HasEdge(0,i),"safe USE cannot be bypassed by an authored network-to-ENTRY edge");
   bad=ngNodes[use];VectorCopy(ngNodes[i].feet,bad.feet);why=NG_DoorRecipeIdentity(&bad,&pose);REQUIRE(why&&!strcmp(why,"local_door_use_inside_motion_sweep"),"unsafe local waiting body cannot borrow staged receipt");
   bad=ngNodes[use];bad.feet[0]+=128;REQUIRE(NG_DoorRecipeIdentity(&bad,&pose)!=NULL,"remote distance cannot borrow local direct-use range");local++;
   bad=ngNodes[use];for(k=0;k<3;k++){float eye=bad.feet[k]+(k==2?NG_DoorEye(&bad):0),lo=g_entities[64].r.absmin[k]+.25f,hi=g_entities[64].r.absmax[k]-.25f;bad.door.aim[k]=eye<lo?lo:(eye>hi?hi:eye);}
   why=NG_DoorRecipeIdentity(&bad,&pose);REQUIRE(why&&!strcmp(why,"use_aim_occluded_or_out_of_range"),"occluded saved nearest-corner aim cannot be silently replaced with inset target");
   bad=ngNodes[use];bad.door.mode=2;REQUIRE(NG_DoorRecipeIdentity(&bad,&pose)!=NULL,"legacy remote-button mode still rejects local door self-use");
  }
  if(test==12){REQUIRE(!local,"blocked actual lateral ground approach cannot invent a staged door recipe");continue;}
  for(i=2;i<ngCount;i++)if(ngNodes[i].door.mode==3&&ngNodes[i].door.role==1){
   int count=ngCount,edges=ngEdgeCount;ngNode saved=ngNodes[i],neighbor=ngNodes[i];
   memset(&neighbor.door,0,sizeof(neighbor.door));neighbor.id=3101;neighbor.crouch=qtrue;neighbor.authored=qtrue;neighbor.editFlags=64;neighbor.navFlags=64;
   ngNodes[ngCount++]=neighbor;REQUIRE(!NG_DoorGroup(64,&pose,&team)&&NG_DoorAnchorWalk(&pose,&ngNodes[i],&ngNodes[count]),"staged USE has a real mixed-stance closed/open floor connection");
   NG_DoorLink(i,count,"actual_mixed_stance_local_use");NG_DoorLink(count,i,"actual_mixed_stance_local_use");
   REQUIRE(ngEdges[edges].crouch&&!memcmp(&saved,&ngNodes[i],sizeof(saved)),"staged standing USE cannot inherit crouch after its aim was bound");
   REQUIRE(!NG_DoorRecipeIdentity(&ngNodes[i],&pose)&&NG_DoorManualFinalize(),"staged saved aim and complete native recipe remain valid after mixed-stance linking");
   ngCount=count;ngEdgeCount=edges;break;
  }
  {int valid=NG_DoorManualFinalize();if(!valid||ngDoorLinks!=2||!local){
    fprintf(stderr,"local=%d links=%d valid=%d traces=%d\n",local,ngDoorLinks,valid,ngTraces);
    for(k=0;k<ngCount;k++)fprintf(stderr,"node%d role%d mode%d valid%d reason=%s feet%.2f,%.2f\n",ngNodes[k].id,ngNodes[k].door.role,ngNodes[k].door.mode,ngNodes[k].accepted,ngNodes[k].reason?ngNodes[k].reason:"none",ngNodes[k].feet[0],ngNodes[k].feet[1]);
   }REQUIRE(local>0&&ngDoorLinks==2&&valid,"native staged local USE plus reverse recipe survive physical revalidation");}
  if(test==14){
   REQUIRE(ngNodes[0].crouch&&!ngNodes[1].crouch,"complete mixed-stance generation preserves each original network stance");
   for(i=2;i<ngCount;i++)if(ngNodes[i].door.mode==3&&ngNodes[i].door.role==1){ngNode bad=ngNodes[i];
    REQUIRE(!ngNodes[i].crouch&&!NG_DoorRecipeIdentity(&ngNodes[i],&pose),"whole NG_Doors retains standing staged USE and actual saved aim beside crouched seed");
    bad.crouch=qtrue;REQUIRE(NG_DoorRecipeIdentity(&bad,&pose)!=NULL,"whole-generator saved aim exposes actual old crouch-propagation failure");
   }
   ngExternalSuppliedCount=2;ngExternalInputComplete=qtrue;REQUIRE(NG_ExternalFinalGraph(),"whole mixed-stance generated graph passes actual edited-WAY final publication gate");
  }
  for(i=0;i<ngCount;i++)ngNodes[i].index=i;
  REQUIRE(NG_Save("navgen/test-mode3.way",ngCount),"actual binary WAY serializer retains numeric mode3 and distinct role UIDs");
  if(argc>2){FILE *file=fopen(argv[2],"wb");REQUIRE(file!=NULL,"mode3 binary output");fwrite(output,1,outputLength,file);fclose(file);}
  {char document[32768];int offset=0,edges=0;const char *bad;
   for(k=0;k<ngEdgeCount;k++)edges+=ngEdges[k].accepted;
   offset+=sprintf(document+offset,"TCE_NAVMESH 6 obj_test %d %d\n",ngCount,edges);
   for(i=0;i<ngCount;i++){const ngNode *n=&ngNodes[i];const ngDoorRecipe *r=&n->door;
    offset+=sprintf(document+offset,"%d %.4f %.4f %.4f %d %d %d %.4f %u 0 0 0 %d %d %d %d %d %d %d %d %d %d %.4f %.4f %.4f\n",
     n->id,n->feet[0],n->feet[1],n->feet[2],n->crouch,i<2,n->topologyProtected,n->radius>0?n->radius:35,(unsigned int)NG_OutputFlags(n),
     r->mode?r->entity+1:0,r->mode?r->useEntity+1:0,r->mode?r->target+1:0,r->mode,r->team,r->role,r->model,r->useModel,r->mode?r->useSource+1:0,r->mode?r->entry+1:0,r->aim[0],r->aim[1],r->aim[2]);
   }
   for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted)offset+=sprintf(document+offset,"%d %d\n",ngNodes[ngEdges[k].from].id,ngNodes[ngEdges[k].to].id);
   inputDocument=document;strcpy(level.rawmapname,"obj_test");NG_ExternalReset();REQUIRE(NG_ExternalRead("candidate"),"actual Format6 mode3 reimport");
   for(i=0;i<ngCount;i++){NG_Node(&ngNodes[i]);REQUIRE(ngNodes[i].accepted,"mode3 reimport retains actual grounded role positions");}
   for(k=0;k<ngEdgeCount;k++){bad=NG_EditorEdge(&ngEdges[k]);REQUIRE(bad==NULL,"mode3 reimport reproves every actual native conditional/ordinary edge");ngEdges[k].accepted=qtrue;if(ngEdges[k].crouch)ngNodes[ngEdges[k].from].crouch=ngNodes[ngEdges[k].to].crouch=qtrue;}
   ngExternalInputComplete=qtrue;REQUIRE(NG_DoorManualFinalize()&&NG_ExternalFinalGraph(),"mode3 actual parser and native physical-recipe/full FinalGraph roundtrip");inputDocument=NULL;
  }
  for(i=0;i<ngCount;i++)if(ngNodes[i].door.mode==3&&ngNodes[i].door.role==2){
   NG_DoorLink(0,i,"spoofed_bypass_to_entry");REQUIRE(!NG_DoorManualFinalize(),"new direct network-to-ENTRY edge cannot bypass staged safe USE");break;
  }
 }
 // A low local doorway may require crouching while every old network
 // candidate stands safely outside it. Neither selected nor rejected
 // candidate searches may silently mutate an authored point's stance.
 setup(13);
 {ngNode original[2];int guides=0;memcpy(original,ngNodes,sizeof(original));
  REQUIRE(NG_DoorRepair(2600)&&ngDoorLinks==2,"local low doorway retains real mixed-stance approach and complete door recipes");
  REQUIRE(!memcmp(original,ngNodes,sizeof(original)),"all authored candidate fields remain byte-exact after door repair, including stance/name/facing/UID");
  REQUIRE(!NG_DoorGroup(64,&pose,&team),"low-door standing-boundary fixture uses its actual current door pose");
  for(i=2;i<ngCount;i++)if(!ngNodes[i].door.mode){
   ngNode stand=ngNodes[i],anchor;stand.crouch=qfalse;stand.authored=qtrue;
   REQUIRE(NG_DoorApproachGuide(i),"low doorway's new ordinary guides retain physical recipe bindings");
   if(NG_HasEdge(i,0)||NG_HasEdge(0,i)){anchor=ngNodes[0];REQUIRE(NG_DoorAnchorWalk(&pose,&stand,&anchor),"old standing boundary retains genuine full-standing-body walk");}
   guides++;
  }
  REQUIRE(NG_DoorManualFinalize(),"low doorway has complete native recipes without forcing standing neighbours to crouch");
  ngExternalSuppliedCount=2;ngExternalInputComplete=qtrue;
  REQUIRE(NG_ExternalFinalGraph(),"actual final publication gate preserves authored standing editFlags");
  {char document[32768];int offset=0,edges=0;const char *bad;
   for(k=0;k<ngEdgeCount;k++)edges+=ngEdges[k].accepted;
   offset+=sprintf(document+offset,"TCE_NAVMESH 6 obj_test %d %d\n",ngCount,edges);
   for(i=0;i<ngCount;i++){const ngNode *n=&ngNodes[i];const ngDoorRecipe *r=&n->door;
    offset+=sprintf(document+offset,"%d %.4f %.4f %.4f %d %d %d %.4f %u 0 0 0 %d %d %d %d %d %d %d %d %d %d %.4f %.4f %.4f\n",
     n->id,n->feet[0],n->feet[1],n->feet[2],n->crouch,i<2,n->topologyProtected,n->radius>0?n->radius:35,(unsigned int)NG_OutputFlags(n),
     r->mode?r->entity+1:0,r->mode?r->useEntity+1:0,r->mode?r->target+1:0,r->mode,r->team,r->role,r->model,r->useModel,r->mode?r->useSource+1:0,r->mode?r->entry+1:0,r->aim[0],r->aim[1],r->aim[2]);
   }
   for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted)offset+=sprintf(document+offset,"%d %d\n",ngNodes[ngEdges[k].from].id,ngNodes[ngEdges[k].to].id);
   inputDocument=document;strcpy(level.rawmapname,"obj_test");NG_ExternalReset();REQUIRE(NG_ExternalRead("candidate"),"real low-door authored candidate reimport");
   for(i=0;i<ngCount;i++){NG_Node(&ngNodes[i]);REQUIRE(ngNodes[i].accepted,"low-door imported point retains actual body/floor clearance");}
   for(k=0;k<ngEdgeCount;k++){bad=NG_EditorEdge(&ngEdges[k]);REQUIRE(bad==NULL,"low-door imported edge passes real native walk");ngEdges[k].accepted=qtrue;if(ngEdges[k].crouch)ngNodes[ngEdges[k].from].crouch=ngNodes[ngEdges[k].to].crouch=qtrue;}
   REQUIRE(!ngNodes[0].crouch&&!ngNodes[1].crouch&&!ngNodes[0].editFlags&&!ngNodes[1].editFlags,"actual import crouch propagation cannot alter either authored standing point");
   ngExternalInputComplete=qtrue;REQUIRE(NG_DoorManualFinalize()&&NG_ExternalFinalGraph(),"real reimport completes the same final edited-WAY publication checks");inputDocument=NULL;
  }
 }
 setup(6);NG_Doors();for(i=0;i<ngCount;i++)if(ngNodes[i].door.role==2){ngNodes[i].door.useSource=0;break;}REQUIRE(!NG_DoorManualFinalize(),"ordinary point cannot spoof button USE receipt");
 // First source IDs are far away: closest validated floor anchors must still join.
 setup(0);ngCount=4;ngNodes[2]=ngNodes[0];ngNodes[3]=ngNodes[1];ngNodes[2].id=2;ngNodes[3].id=3;ngNodes[0].feet[0]=-300;ngNodes[1].feet[0]=300;NG_Doors();
 for(i=4;i<ngCount;i++)if(ngNodes[i].door.role==2){int near=ngNodes[i].feet[0]<0?2:3;REQUIRE(NG_HasEdge(i,near)&&NG_HasEdge(near,i),"nearest physical anchor beats first source index");}
 // Actual weighted closed-graph distances differ from a first/hop-count route.
 setup(0);ngCount=4;ngEdgeCount=0;for(i=0;i<4;i++){ngNodes[i].id=i;ngNodes[i].accepted=qtrue;ngNodes[i].navFlags=0;}
 VectorSet(ngNodes[0].feet,-600,0,.25f);VectorSet(ngNodes[1].feet,-500,0,.25f);VectorSet(ngNodes[2].feet,-500,1000,.25f);VectorSet(ngNodes[3].feet,-400,0,.25f);
 REQUIRE(NG_BothWays(&ngNodes[0],&ngNodes[2])&&NG_BothWays(&ngNodes[2],&ngNodes[3])&&NG_BothWays(&ngNodes[0],&ngNodes[1])&&NG_BothWays(&ngNodes[1],&ngNodes[3]),"actual full-body ground proof for weighted fixture edges");
 NG_DoorLink(0,2,"independent_long_route");NG_DoorLink(2,3,"independent_long_route");NG_DoorLink(0,1,"independent_short_route");NG_DoorLink(1,3,"independent_short_route");
 {float distances[NG_NODES];NG_DoorDistances(0,1,qfalse,distances);REQUIRE(fabs(distances[3]-200.f)<.01f,"actual weighted shortest ground route");
  ngNodes[1].navFlags=16|2;NG_DoorDistances(0,1,qfalse,distances);REQUIRE(distances[3]>2000,"team-only gate retained in weighted search");
  ngNodes[0].door.mode=2;ngNodes[0].door.role=2;ngNodes[0].door.target=1;NG_DoorDistances(0,2,qfalse,distances);REQUIRE(distances[3]>2000,"own conditional crossing cannot shortcut to switch");
 }
 puts("actual native door/remote/pose/WAY controls passed");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class NativeDoors(TestCase):
    def test_actual_native_discovery_and_negative_controls(self):
        with tempfile.TemporaryDirectory(prefix='tce-native-doors-') as directory:
            target = Path(directory)
            source = (ROOT / 'game/tce_navgen.c').read_text()
            (target / 'main.c').write_text(source + FIXTURE)
            cmake = 'cmake_minimum_required(VERSION 3.15)\nproject(Doors LANGUAGES C)\n'
            cmake += 'add_executable(doors main.c ' + ' '.join('"' + (ROOT / 'game' / name).as_posix() + '"' for name in ('q_shared.c', 'q_math.c', 'tce_movement.c')) + ')\n'
            cmake += 'target_include_directories(doors PRIVATE "' + (ROOT / 'game').as_posix() + '")\ntarget_compile_options(doors PRIVATE /Gy)\ntarget_link_options(doors PRIVATE /OPT:REF)\n'
            (target / 'CMakeLists.txt').write_text(cmake)
            way = target / 'test.way'
            mode3_way = target / 'test-mode3.way'
            commands = (['cmake', '-S', directory, '-B', str(target / 'build'), '-A', 'Win32'],
                        ['cmake', '--build', str(target / 'build'), '--config', 'Release'],
                        [str(target / 'build/Release/doors.exe'), str(way), str(mode3_way)])
            for command in commands:
                result = subprocess.run(command, capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            import sys
            sys.path.insert(0, str(ROOT / 'tools'))
            from tce_navgen import read_way
            nodes, edges = read_way(way)
            recipes = [node['properties'] for node in nodes if 'tce_door_entity' in node['properties']]
            self.assertEqual(len(recipes), 4)
            self.assertEqual({int(recipe['tce_door_entity']) for recipe in recipes}, {64})
            self.assertEqual({int(recipe['tce_door_target']) for recipe in recipes}, {0, 5, 6})
            self.assertTrue(all(len(recipe) == 12 for recipe in recipes))  # source identity + eleven door fields
            mode3_nodes,mode3_edges=read_way(mode3_way)
            local=[n for n in mode3_nodes if n.get('properties',{}).get('tce_door_mode')=='3']
            self.assertEqual({n['properties']['tce_door_role'] for n in local},{'1','2','3'})
            by_uid={n['uid']:n for n in mode3_nodes}
            for node in local:
                props=node['properties'];use=by_uid[int(props['tce_door_use_nav'])];entry=by_uid[int(props['tce_door_entry'])]
                self.assertEqual(props['tce_door_entity'],props['tce_door_use_entity'])
                self.assertNotEqual(use['uid'],entry['uid'])
                self.assertEqual(int(use['properties']['tce_door_target']),entry['uid'])


if __name__ == '__main__':
    main()
