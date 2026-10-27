/* Actual mover state/blocked/team functions; synthetic failed-push service. */
#include "src/game/g_local.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_trajectory.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
vmCvar_t g_cheats;
static int checks,failures,links,damageCalls,damageAmount,freeCalls,flagCalls;
static gentity_t eventEntity,obstacle;
static int failPart=-1;
static int linkTimes[2];
/* PRODUCTION_PUSHED_TYPE */
static pushed_t pushed[MAX_GENTITIES],*pushed_p;

void trap_LinkEntity(gentity_t *e) {++links;if(e==&g_entities[0]||e==&g_entities[1])linkTimes[e-g_entities]=e->s.apos.trTime;}
void G_SetAASBlockingEntity(gentity_t *e,int flags) {(void)e;(void)flags;}
void QDECL G_Printf(const char *s,...) {(void)s;}
char *vtos(const vec3_t v) {(void)v;return "fixture";}
void QDECL Com_Error(int n,const char *s,...) {(void)n;(void)s;abort();}
void QDECL Com_Printf(const char *s,...) {(void)s;}
void Team_DroppedFlagThink(gentity_t *e) {(void)e;++flagCalls;}
gentity_t *G_TempEntity(vec3_t p,int event) {(void)p;(void)event;return &eventEntity;}
void G_FreeEntity(gentity_t *e) {(void)e;++freeCalls;}
tce_damageResult_t G_Damage(gentity_t *t,gentity_t *i,gentity_t *a,vec3_t d,vec3_t p,int amount,int flags,int mod)
{(void)t;(void)i;(void)a;(void)d;(void)p;(void)flags;if(mod!=MOD_CRUSH)abort();++damageCalls;damageAmount+=amount;return 1;}
splinePath_t *BG_GetSplineData(int n,qboolean *b) {(void)n;(void)b;abort();return NULL;}
void BG_CalculateSpline_r(splinePath_t *p,vec3_t a,vec3_t b,float f) {(void)p;(void)a;(void)b;(void)f;abort();}
qboolean BG_TraverseSpline(float *f,splinePath_t **p) {(void)f;(void)p;abort();return qfalse;}
void BG_LinearPathOrigin2(float r,splinePath_t **p,float *f,vec3_t o,qboolean b)
{(void)r;(void)p;(void)f;(void)o;(void)b;abort();}
void BG_EvaluateTrajectory(const trajectory_t *t,int at,vec3_t v,qboolean angle,int path)
{TCE_BG_EvaluateTrajectory(t,at,v,angle,path,1);}
qboolean G_MoverPush(gentity_t *e,vec3_t move,vec3_t turn,gentity_t **block)
{VectorAdd(e->r.currentOrigin,move,e->r.currentOrigin);VectorAdd(e->r.currentAngles,turn,e->r.currentAngles);*block=&obstacle;return e-g_entities!=failPart;}

/* PRODUCTION_FUNCTIONS */

/* FROZEN_SET_MOVER_STATE */

#define CHECK(x) do {++checks;if(!(x)){if(++failures<=16)printf("line%d: %s\n",__LINE__,#x);}}while(0)
static void setup(gentity_t *e,int duration,int flags,moverState_t state,int start,int now,int axis,float angle)
{
 memset(e,0,sizeof(*e));e->classname="func_door_rotating";e->gDuration=duration;
 e->flags=flags|FL_TOGGLE;e->spawnflags=48;e->rotate[axis]=1;e->angle=angle;
 e->s.pos.trType=TR_STATIONARY;e->blocked=Blocked_DoorRotate;
 level.time=now;SetMoverState(e,state,start);
 BG_EvaluateTrajectory(&e->s.apos,now,e->r.currentAngles,qtrue,0);
}
static float position(const gentity_t *e,int now,int axis)
{vec3_t v;BG_EvaluateTrajectory(&e->s.apos,now,v,qtrue,0);return v[axis];}
static void live_case(void)
{
 gentity_t *e=&g_entities[0];float before,after,next;
 setup(e,1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1436200,1436500,1,-90);
 before=e->r.currentAngles[1];links=0;level.previousTime=1436500;level.time=1436550;failPart=0;
 G_MoverTeam(e);after=position(e,level.time,1);next=position(e,level.time+50,1);
 printf("LIVE before=%.6f reversed=%.6f next=%.6f newTime=%d duration=%d links=%d\n",before,after,next,e->s.apos.trTime,e->s.apos.trDuration,links);
 CHECK(fabs(before+54)<.001);CHECK(fabs(after-before)<.001);CHECK(fabs(next+49.5)<.001);
 CHECK(e->s.apos.trTime==1436150);CHECK(e->moverState==MOVER_2TO1ROTATE);
 CHECK(e->s.apos.trDuration==1000);CHECK(links==3);CHECK(e->spawnflags==48);
 failPart=-1;
}
/* Full evaluator enumeration is independent of the phase-remapping helper. */
static double closest_error(const trajectory_t *trajectory,float before,int axis)
{
 trajectory_t t=*trajectory;vec3_t v;int i;double best=DBL_MAX;
 t.trTime=0;
 for(i=0;i<=t.trDuration;++i) {
  double error;BG_EvaluateTrajectory(&t,i,v,qtrue,0);error=fabs((double)v[axis]-before);
  if(error<best)best=error;
 }
 return best;
}
static void construction_parity(void)
{
 const int modes[]={0,FL_KICKACTIVATE,FL_SOFTACTIVATE,FL_KICKACTIVATE|FL_SOFTACTIVATE};
 const int durations[]={1,2,3,5,7,500,997,1000,1001,2000};
 int d,m,axis,direction,toggle;
 for(d=0;d<10;++d)for(m=0;m<4;++m)for(axis=0;axis<3;++axis)
 for(direction=0;direction<2;++direction)for(toggle=0;toggle<2;++toggle) {
  gentity_t original,actual;int actualLinks,originalLinks;memset(&actual,0,sizeof(actual));
  actual.classname="func_door_rotating";actual.gDuration=durations[d];actual.flags=modes[m]|(toggle?FL_TOGGLE:0);
  actual.rotate[axis]=1;actual.angle=axis==1?-90:60;actual.active=qtrue;actual.spawnflags=48;
  actual.s.pos.trType=TR_STATIONARY;actual.s.apos.trDuration=77;actual.s.apos.trTime=66;
  actual.r.currentAngles[axis]=23;original=actual;level.time=12345;links=0;
  SetMoverState(&actual,direction?MOVER_2TO1ROTATE:MOVER_1TO2ROTATE,12000);actualLinks=links;links=0;
  Baseline_SetMoverState(&original,direction?MOVER_2TO1ROTATE:MOVER_1TO2ROTATE,12000);originalLinks=links;
  CHECK(memcmp(&actual,&original,sizeof(actual))==0);CHECK(actualLinks==originalLinks);
 }
}
static void matrix(void)
{
 const int modes[]={0,FL_KICKACTIVATE,FL_SOFTACTIVATE,FL_KICKACTIVATE|FL_SOFTACTIVATE};
 const int durations[]={1,2,3,5,7,500,997,1000,1001,2000};const int percents[]={0,1,5,17,50,73,99,100};
 int direction,oldMode,newMode,d,p,axis,changed;
 for(direction=0;direction<2;++direction)for(oldMode=0;oldMode<4;++oldMode)for(newMode=0;newMode<4;++newMode)
 for(d=0;d<10;++d)for(p=0;p<8;++p)for(axis=0;axis<3;++axis)for(changed=0;changed<2;++changed) {
  gentity_t *e=&g_entities[0],expected;int oldDuration,oldElapsed,newDuration;
  float before,after,next;double best;
  setup(e,durations[d],modes[oldMode],direction?MOVER_2TO1ROTATE:MOVER_1TO2ROTATE,100000,100000,axis,-90);
  oldDuration=e->s.apos.trDuration;oldElapsed=oldDuration*percents[p]/100;
  level.time=100000+oldElapsed;before=position(e,level.time,axis);
  e->r.currentAngles[axis]=before;e->flags=modes[newMode]|FL_TOGGLE;
  if(changed)e->gDuration=1200;
  expected=*e;Baseline_SetMoverState(&expected,direction?MOVER_1TO2ROTATE:MOVER_2TO1ROTATE,0);
  best=closest_error(&expected.s.apos,before,axis);
  links=0;Blocked_DoorRotate(e,NULL);newDuration=e->s.apos.trDuration;
  after=position(e,level.time,axis);next=position(e,level.time+10,axis);
  CHECK(fabs((double)after-before)<=best+.0001);CHECK(links==2);CHECK(linkTimes[0]==e->s.apos.trTime);
  CHECK(e->spawnflags==48);CHECK(e->s.apos.trTime==e->s.pos.trTime);
  CHECK(e->s.apos.trDuration==expected.s.apos.trDuration);
  CHECK(memcmp(e->s.apos.trBase,expected.s.apos.trBase,sizeof(vec3_t))==0);
  CHECK(memcmp(e->s.apos.trDelta,expected.s.apos.trDelta,sizeof(vec3_t))==0);
  CHECK(direction?next<=after+.0001f:next>=after-.0001f);
  if(oldDuration>0&&newDuration>0&&durations[d]%2==0&&newMode==oldMode&&!changed)
   CHECK(fabs(after-before)<=fabs(e->s.apos.trDelta[axis])*.000501+.0001);
 }
}
static void team_case(void)
{
 float before[2];int i;
 setup(&g_entities[0],1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1000,1300,1,-90);
 setup(&g_entities[1],2000,FL_SOFTACTIVATE,MOVER_2TO1ROTATE,500,1300,2,60);
 g_entities[0].teamchain=&g_entities[1];before[0]=g_entities[0].r.currentAngles[1];before[1]=g_entities[1].r.currentAngles[2];
 level.previousTime=1300;level.time=1350;failPart=1;links=0;G_MoverTeam(&g_entities[0]);
 for(i=0;i<2;++i) {CHECK(fabs(position(&g_entities[i],level.time,i?2:1)-before[i])<.0001);CHECK(linkTimes[i]==g_entities[i].s.apos.trTime);}
 CHECK(links==6);CHECK(g_entities[0].moverState==MOVER_2TO1ROTATE);CHECK(g_entities[1].moverState==MOVER_1TO2ROTATE);
 failPart=-1;g_entities[0].teamchain=NULL;
}
static int reachedCalls;
static void reached_control(gentity_t *e)
{
 ++reachedCalls;
 SetMoverState(e,e->moverState==MOVER_2TO1ROTATE?MOVER_POS1ROTATE:MOVER_POS2ROTATE,level.time);
}
static void completion_control(void)
{
 gentity_t *e=&g_entities[0];int end;
 setup(e,1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1000,1300,1,-90);
 Blocked_DoorRotate(e,NULL);e->reached=reached_control;reachedCalls=0;failPart=-1;
 end=e->s.apos.trTime+e->s.apos.trDuration;level.previousTime=level.time;level.time=end-1;
 G_MoverTeam(e);CHECK(reachedCalls==0);
 level.previousTime=level.time;level.time=end;G_MoverTeam(e);
 CHECK(reachedCalls==1);CHECK(e->moverState==MOVER_POS1ROTATE);
 CHECK(e->s.apos.trType==TR_STATIONARY);CHECK(fabs(e->r.currentAngles[1])<.0001);
}
static void cleanup_controls(void)
{
 gentity_t other;gclient_t client;gitem_t item;
 memset(&other,0,sizeof(other));memset(&client,0,sizeof(client));memset(&item,0,sizeof(item));
 setup(&g_entities[0],1000,0,MOVER_1TO2ROTATE,1000,1250,1,-90);
 other.s.eType=ET_ITEM;other.item=&item;item.giType=IT_TEAM;flagCalls=0;links=0;
 Blocked_DoorRotate(&g_entities[0],&other);CHECK(flagCalls==1&&links==0);
 other.s.eType=ET_GENERAL;freeCalls=0;Blocked_DoorRotate(&g_entities[0],&other);CHECK(freeCalls==1&&links==0);
 other.client=&client;other.health=0;g_entities[0].damage=5;damageCalls=damageAmount=0;
 Blocked_DoorRotate(&g_entities[0],&other);CHECK(damageCalls==2&&damageAmount==100004);
}
#ifdef TCE_ROTATION_REVERSE_HELPERS
static void bounds(void)
{
 gentity_t *e=&g_entities[0];int now=INT_MIN+100;
 setup(e,1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1000,1300,1,0);
 CHECK(G_TCEReverseRotationTime(e,MOVER_2TO1ROTATE,1300)==1300);
 Blocked_DoorRotate(e,NULL);CHECK(position(e,1300,1)==0);
 setup(e,1,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1000,1000,1,-90);
 CHECK(e->s.apos.trDuration==0);Blocked_DoorRotate(e,NULL);
 printf("ENDPOINT 1ms=%.9g degrees\n",position(e,1000,1));
 CHECK(fabs(position(e,1000,1))<.0001);CHECK(e->s.apos.trTime==999);
 setup(e,1,0,MOVER_2TO1ROTATE,1000,1000,1,-90);e->flags=FL_KICKACTIVATE;
 CHECK(G_TCEReverseRotationTime(e,MOVER_1TO2ROTATE,1000)==1000);
 e->gDuration=-1;CHECK(G_TCEReverseRotationTime(e,MOVER_1TO2ROTATE,1000)==1000);
 e->gDuration=INT_MAX;e->flags=FL_SOFTACTIVATE;
 CHECK(G_TCEReverseRotationTime(e,MOVER_1TO2ROTATE,1000)==1000);
 /* Direct Use_BinaryMover reversal can occur before start or past end. */
 setup(e,1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1500,1400,1,-90);
 Blocked_DoorRotate(e,NULL);CHECK(fabs(position(e,1400,1))<.0001);
 setup(e,1000,0,MOVER_2TO1ROTATE,1500,1400,1,-90);
 Blocked_DoorRotate(e,NULL);CHECK(fabs(position(e,1400,1)+90)<.0001);
 setup(e,1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,1000,2000,1,-90);
 Blocked_DoorRotate(e,NULL);CHECK(fabs(position(e,2000,1)+90)<.0001);
 /* Safe helper timestamp subtraction; not a claim about legacy mover clock wrap. */
 setup(e,1000,FL_KICKACTIVATE,MOVER_1TO2ROTATE,TCE_SubInt32(now,300),now,1,-90);
 CHECK(G_TCEReverseRotationTime(e,MOVER_2TO1ROTATE,now)==TCE_SubInt32(now,400));
 setup(e,3,0,MOVER_2TO1ROTATE,1000,1000,1,-90);e->flags=FL_KICKACTIVATE;
 Blocked_DoorRotate(e,NULL);
 /* A 3ms kicked trajectory only reaches 60deg: the inherited 30deg endpoint
  * gap cannot be represented without changing legacy rate/duration semantics. */
 CHECK(fabs(position(e,1000,1)+60)<.0001);
 CHECK(fabs(position(e,1000,1)+90)>29.9);
}
#endif

int main(void)
{
 memset(&obstacle,0,sizeof(obstacle));obstacle.client=(gclient_t*)1;obstacle.health=100;
 construction_parity();live_case();matrix();team_case();completion_control();cleanup_controls();
#ifdef TCE_ROTATION_REVERSE_HELPERS
 bounds();
#endif
 printf("door reversal checks=%d failures=%d\n",checks,failures);return failures!=0;
}
