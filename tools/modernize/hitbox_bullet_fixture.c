/* Engine services for production rewind and active TC damage-volume tests. */
#include "src/game/g_local.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_pose_precision.h"
#include "src/game/tce_damage.h"
#include "src/game/tce_damage_trace.h"
#include "src/game/tce_bullet.h"
#include "src/game/tce_lua.h"
#include "src/game/g_tce_bullet_damage.h"
#include <stdio.h>
#include <math.h>
#include <float.h>

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_gamestate, g_debugDamage, g_debugBullets, g_newbbox, g_gametype;
tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS]; vec3_t forward;
const char *modNames[MOD_NUM_MODS];
mapEntityData_Team_t mapEntityData[2];
static int hitOrder[16], hitAmount[16], hitCount, eventCount;
static int wallSurface;
static gentity_t events[128];
static gclient_t testClients[MAX_CLIENTS];
static int failures, tests, traceCalls, spawnCalls, links, partLinks;
static int regionFractionOverride,regionProbeIndex;
static float regionProbeFractions[3];
static vec3_t firstVolume;
static gentity_t debugEntity;
#define CHECK(c) do { ++tests; if (!(c)) { ++failures; printf("FAIL line%d: %s\n", __LINE__, #c); } } while (0)
static int closeFloat(float a, float b) { return fabs((double)a - b) < .0003; }

void QDECL G_Printf(const char *format, ...) { (void)format; }
void trap_LinkEntity(gentity_t *ent)
{
    ++links; ent->r.linked = qtrue;
    if (ent == &g_entities[70]) {
        if (!(partLinks++ % 3)) { VectorCopy(ent->r.currentOrigin, firstVolume); }
    }
}
void trap_UnlinkEntity(gentity_t *ent) { ent->r.linked = qfalse; }
gentity_t *G_Spawn(void) { gentity_t *ent = &g_entities[70]; ++spawnCalls; memset(ent, 0, sizeof(*ent)); ent->s.number = 70; ent->inuse = qtrue; return ent; }
void G_FreeEntity(gentity_t *ent) { ent->inuse = ent->r.linked = qfalse; }
void G_SetOrigin(gentity_t *ent, vec3_t point) { VectorCopy(point, ent->r.currentOrigin); VectorCopy(point, ent->s.origin); }
gentity_t *G_TempEntity(vec3_t point, int event) { gentity_t *e=&events[eventCount++ % 128]; memset(e,0,sizeof(*e));e->s.event=event;VectorCopy(point,e->s.origin);return e; }
int trap_Milliseconds(void) { return level.time; }
qboolean OnSameTeam(gentity_t *a, gentity_t *b) { return a->client && b->client && a->client->sess.sessionTeam == b->client->sess.sessionTeam; }
void G_LogRegionHit(gentity_t *ent, hitRegion_t region) { ++ent->client->pers.playerStats.hitRegions[region]; }
void AngleVectors(const vec3_t a, vec3_t forward, vec3_t right, vec3_t up)
{
    double p = a[0] * (M_PI / 180.0), y = a[1] * (M_PI / 180.0), r = a[2] * (M_PI / 180.0);
    double sp = sin(p), cp = cos(p), sy = sin(y), cy = cos(y), sr = sin(r), cr = cos(r);
    if (forward) { VectorSet(forward, (float)(cp * cy), (float)(cp * sy), (float)-sp); }
    if (right) { VectorSet(right, (float)(-sr * sp * cy + cr * sy), (float)(-sr * sp * sy - cr * cy), (float)(-sr * cp)); }
    if (up) { VectorSet(up, (float)(cr * sp * cy + sr * sy), (float)(cr * sp * sy - sr * cy), (float)(cr * cp)); }
}
vec_t VectorLength(const vec3_t v) { return (float)sqrt((double)v[0]*v[0] + (double)v[1]*v[1] + (double)v[2]*v[2]); }
void VectorNormalizeFast(vec3_t v) { float n = VectorLength(v); if (n) { VectorScale(v, 1.0f / n, v); } }

static int rayBox(const vec3_t start, const vec3_t end, const vec3_t mins,
                  const vec3_t maxs, float *fraction)
{
    double nearPoint = 0, farPoint = 1; int axis;
    for (axis = 0; axis < 3; ++axis) {
        double d = end[axis] - start[axis], a, b, swap;
        if (!d) { if (start[axis] < mins[axis] || start[axis] > maxs[axis]) return 0; continue; }
        a = (mins[axis] - start[axis]) / d; b = (maxs[axis] - start[axis]) / d;
        if (a > b) { swap = a; a = b; b = swap; }
        if (a > nearPoint) nearPoint = a;
        if (b < farPoint) farPoint = b;
        if (nearPoint > farPoint) return 0;
    }
    *fraction = (float)nearPoint; return nearPoint <= 1 && farPoint >= 0;
}
void trap_Trace(trace_t *tr, const vec3_t start, const vec3_t mins,
                 const vec3_t maxs, const vec3_t end, int ignore, int mask)
{
    int id, axis; (void)mins; (void)maxs; ++traceCalls;
    memset(tr, 0, sizeof(*tr)); tr->fraction = 1; tr->entityNum = ENTITYNUM_WORLD;
    for (id = 0; id < MAX_GENTITIES; ++id) {
        gentity_t *ent = &g_entities[id]; float fraction; vec3_t low, high;
        if (id == ignore || !ent->inuse || !ent->r.linked || !(ent->r.contents & mask)) continue;
        for (axis = 0; axis < 3; ++axis) { low[axis] = ent->r.currentOrigin[axis] + ent->r.mins[axis]; high[axis] = ent->r.currentOrigin[axis] + ent->r.maxs[axis]; }
        {
            int startInside=1,endInside=1;
            for(axis=0;axis<3;++axis){if(start[axis]<low[axis]||start[axis]>high[axis])startInside=0;if(end[axis]<low[axis]||end[axis]>high[axis])endInside=0;}
            /* Box-brush startsolid semantics: leaving a brush is not a new
             * entry hit. This matters for wall exits and coincident hulls. */
            if(startInside){tr->startsolid=qtrue;if(endInside){tr->allsolid=qtrue;tr->fraction=0;tr->entityNum=id;}continue;}
        }
        if (rayBox(start, end, low, high, &fraction) && fraction < tr->fraction) { tr->fraction = fraction; tr->entityNum = id;tr->surfaceFlags=id==100?wallSurface:0; }
    }
    if(regionFractionOverride && g_entities[70].r.linked && (mask & CONTENTS_SOLID)) {
        tr->fraction=regionProbeFractions[regionProbeIndex++ % 3];tr->entityNum=70;
    }
    for (axis = 0; axis < 3; ++axis) { tr->endpos[axis] = start[axis] + tr->fraction * (end[axis] - start[axis]); }
}


#define TCE_INTERNAL_BOT(ent) qfalse
qboolean IsHeadShotWeapon(int mod);
vec_t VectorNormalize(vec3_t v) { float n=VectorLength(v);if(n){VectorScale(v,1.0f/n,v);}return n; }
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_Error(int code,const char *format, ...) { (void)code;(void)format;abort(); }
char *QDECL va(const char *format, ...) { static char text[256];va_list args;va_start(args,format);vsprintf(text,format,args);va_end(args);return text; }
vec_t Distance(const vec3_t a,const vec3_t b) { vec3_t d;VectorSubtract(a,b,d);return VectorLength(d); }
int DirToByte(vec3_t dir) { (void)dir;abort();return 0; }
void AddScore(gentity_t *e,int score) { (void)e;(void)score;abort(); }
void G_ExplodeMissile(gentity_t *e) { (void)e;abort(); }
mapEntityData_t *G_FindMapEntityData(mapEntityData_Team_t *data,int number) { (void)data;(void)number;abort();return NULL; }
mapEntityData_t *G_FreeMapEntityData(mapEntityData_Team_t *data,mapEntityData_t *e) { (void)data;(void)e;abort();return NULL; }
void RubbleFlagCheck(gentity_t *e,trace_t tr) { (void)e;(void)tr; }
void EmitterCheck(gentity_t *e,gentity_t *a,trace_t *tr) { (void)e;(void)a;(void)tr; }
qboolean AccuracyHit(gentity_t *t,gentity_t *a) { (void)t;(void)a;return qtrue; }
void G_addStats(gentity_t *t,gentity_t *a,int take,int mod) { (void)a;(void)mod;if(hitCount>=16)abort();hitOrder[hitCount]=t->s.number;hitAmount[hitCount++]=take; }
void G_Voice(gentity_t *e,gentity_t *o,int mode,const char *id,qboolean voiceonly) { (void)e;(void)o;(void)mode;(void)id;(void)voiceonly; }
void G_Script_ScriptEvent(gentity_t *e,char *event,char *params) { (void)e;(void)event;(void)params; }
void Bot_ScriptEvent(int e,char *event,char *params) { (void)e;(void)event;(void)params;abort(); }
void BotRecordPain(int e,int a,int mod) { (void)e;(void)a;(void)mod;abort(); }
void BotRecordDeath(int e,int a) { (void)e;(void)a;abort(); }
void G_AddKillSkillPoints(gentity_t *a,meansOfDeath_t mod,hitRegion_t region,qboolean splash) { (void)a;(void)mod;(void)region;(void)splash; }
void limbo(gentity_t *e,qboolean corpse) { (void)e;(void)corpse;abort(); }
qboolean G_WeaponIsExplosive(meansOfDeath_t mod) { (void)mod;abort();return qfalse; }
int G_GetWeaponClassForMOD(meansOfDeath_t mod) { (void)mod;abort();return 0; }
team_t G_GetTeamFromEntity(gentity_t *e) { return e->client ? e->client->sess.sessionTeam : TEAM_FREE; }
void G_UseEntity(gentity_t *e,gentity_t *a,gentity_t *b) { (void)e;(void)a;(void)b;abort(); }
static void died(gentity_t *t,gentity_t *i,gentity_t *a,int take,int mod) { (void)i;G_addStats(t,a,take,mod);t->client->ps.pm_type=PM_DEAD;t->client->ps.eFlags|=EF_DEAD;t->r.contents=CONTENTS_CORPSE;t->r.maxs[2]=24; }

#if defined(__linux__) && !defined(_MSC_VER)
/* Host CRT headers are already read. Select native Linux production branches. */
#undef _WIN32
#undef WIN32
#undef _WIN64
#endif
/* Presentation-history/skin services are controlled inputs for these glue
 * contracts; the separate actual-asset provider fixtures validate geometry. */
static int presentationPolicy[MAX_CLIENTS],presentationRegion[MAX_CLIENTS];
static double presentationFraction[MAX_CLIENTS];
static unsigned queryCalls,skinCalls,matchCalls;
static uint32_t seenShotTime,seenReceiptCeiling;
static int invalidateMatch,nestedDamage,nestedGuard,nestedSecond,luaDamageCalls;
static vec3_t queryPoseOrigin[MAX_CLIENTS],seenPoseOrigin;
int G_TCEPresentationQuery(int clientNum,uint32_t time,uint32_t ceiling,g_tcePresentationQuery_t *out)
{
 ++queryCalls;seenShotTime=time;seenReceiptCeiling=ceiling;memset(out,0,sizeof(*out));
 VectorCopy(queryPoseOrigin[clientNum],out->origin);
 out->clientNum=clientNum;out->serverTime=time;out->sourceTime=time;out->generation=7;out->mapGeneration=9;out->profile=11;
 return presentationPolicy[clientNum];
}
g_tcePoseTraceState_t G_TCEPosePresentationTrace(const g_tcePresentationQuery_t *query,
 const float start[3],const float end[3],double cap,g_tcePoseTraceResult_t *out)
{
 int i,client=query->clientNum;++skinCalls;memset(out,0,sizeof(*out));VectorCopy(query->origin,seenPoseOrigin);
 VectorCopy(query->origin,out->key.origin);
 out->key.clientNum=client;out->key.serverTime=query->serverTime;out->key.sourceTime=query->sourceTime;
 out->key.generation=query->generation;out->key.mapGeneration=query->mapGeneration;out->key.profile=query->profile;
 VectorCopy(start,out->key.start);VectorCopy(end,out->key.end);out->key.maxFraction=cap;
 if(presentationRegion[client]<0)return out->state=presentationRegion[client]==-2?G_TCE_POSE_TRACE_UNSUPPORTED:G_TCE_POSE_TRACE_MISS;
 if(end[0]==start[0])return out->state=G_TCE_POSE_TRACE_MISS;
 out->surface.fraction=(presentationFraction[client]-start[0])/(end[0]-start[0]);
 if(!(out->surface.fraction>=0&&out->surface.fraction<=cap))return out->state=G_TCE_POSE_TRACE_MISS;
 out->state=G_TCE_POSE_TRACE_HIT;out->surface.region=(tcePoseRegion_t)presentationRegion[client];
 for(i=0;i<3;++i)out->surface.point[i]=(float)(start[i]+out->surface.fraction*(end[i]-start[i]));
 out->nominal[out->surface.region]=out->surface.region==0?2.0f:out->surface.region==1?1.0f:.5f;
 return out->state;
}
int G_TCEPosePresentationTraceMatches(const g_tcePoseTraceResult_t *result,
 const g_tcePresentationQuery_t *query,const float start[3],const float end[3],double cap)
{
 ++matchCalls;
 return !invalidateMatch&&result->state==G_TCE_POSE_TRACE_HIT&&result->key.clientNum==query->clientNum&&
 result->key.serverTime==query->serverTime&&result->key.sourceTime==query->sourceTime&&
 result->key.generation==query->generation&&result->key.mapGeneration==query->mapGeneration&&result->key.profile==query->profile&&
 !memcmp(query->origin,result->key.origin,sizeof(vec3_t))&&
 !memcmp(start,result->key.start,sizeof(vec3_t))&&!memcmp(end,result->key.end,sizeof(vec3_t))&&cap==result->key.maxFraction;
}

static int fixtureLuaDamage(int target,int attacker,int damage,int flags,int mod);
#undef TCE_LuaDamage
#define TCE_LuaDamage fixtureLuaDamage
/* PRODUCTION_FUNCTIONS */

static void actor(int id,float x,int weapon,int prone,int yaw)
{
    gentity_t *e=&g_entities[id];gclient_t *c=&testClients[id];memset(e,0,sizeof(*e));memset(c,0,sizeof(*c));
    e->s.number=id;e->client=c;e->inuse=e->r.linked=e->takedamage=qtrue;e->health=100;e->s.eType=ET_PLAYER;e->r.contents=CONTENTS_BODY;e->die=died;
    c->sess.sessionTeam=id?TEAM_AXIS:TEAM_ALLIES;c->ps.pm_type=PM_NORMAL;c->ps.weapon=weapon;c->ps.crouchMaxZ=24;c->ps.viewangles[YAW]=(float)yaw;
    c->ps.eFlags=prone?EF_PRONE:0;VectorSet(e->r.currentOrigin,x,0,0);VectorSet(c->ps.origin,x,0,0);
    VectorSet(e->r.mins,prone?-35:-21,prone?-35:-21,-24);VectorSet(e->r.maxs,prone?35:21,prone?35:21,prone?23:57.5f);
}
static void resetWorld(void)
{
    memset(g_entities,0,sizeof(g_entities));memset(testClients,0,sizeof(testClients));memset(&level,0,sizeof(level));memset(weaponDef,0,sizeof(weaponDef));
    g_gamestate.integer=GS_PLAYING;g_gametype.integer=0;g_newbbox.integer=0;level.time=10000;level.maxclients=MAX_CLIENTS;
    memset(presentationPolicy,0,sizeof(presentationPolicy));memset(presentationRegion,0,sizeof(presentationRegion));
    memset(presentationFraction,0,sizeof(presentationFraction));memset(queryPoseOrigin,0,sizeof(queryPoseOrigin));VectorClear(seenPoseOrigin);queryCalls=skinCalls=matchCalls=0;invalidateMatch=0;
    seenShotTime=seenReceiptCeiling=0;nestedDamage=nestedGuard=nestedSecond=luaDamageCalls=0;
    traceCalls=spawnCalls=links=partLinks=hitCount=eventCount=wallSurface=0;actor(0,-100,8,0,0);g_entities[0].r.linked=qfalse;g_entities[0].s.weapon=8;
    regionFractionOverride=regionProbeIndex=0;
    weaponDef[8].mod=MOD_MP40;weaponDef[8].unknown_134=1000;weaponDef[8].unknown_130=1000;VectorSet(forward,1,0,0);
}
static void shoot(float z,int damage,int penetration,int bodies,int walls)
{
    vec3_t start={0,0,0},end={300,0,0};start[2]=end[2]=z;
    Bullet_Fire_Extended(&g_entities[0],&g_entities[0],start,end,0,damage,penetration,0,0,walls,bodies,8,42,1);
}
static int fixtureLuaDamage(int target,int attacker,int damage,int flags,int mod)
{
 (void)attacker;(void)damage;(void)flags;(void)mod;++luaDamageCalls;
 if(nestedDamage&&target==1&&!nestedGuard) {
  nestedGuard=1;g_entities[1].r.linked=qfalse;
  if(nestedSecond){g_entities[2].r.linked=qfalse;g_entities[3].r.linked=qtrue;}
  else g_entities[2].r.linked=qtrue;
  shoot(42,10,1000,1,0);
  g_entities[1].r.linked=qtrue;g_entities[2].r.linked=nestedSecond?qtrue:qfalse;
  if(nestedSecond)g_entities[3].r.linked=qfalse;
  presentationRegion[1]=TCE_POSE_REGION_LEGS;presentationFraction[1]=250;
  g_entities[0].client->tceRewind.requestedTime=22222;
  g_entities[0].client->tceRewind.receiptCeiling=33333;
 }
 return 0;
}
static void presentationAvailability(void)
{
 int mode,corpse;
 for(corpse=0;corpse<2;++corpse)for(mode=0;mode<5;++mode) {
  resetWorld();actor(1,100,1,0,0);
  if(corpse)g_entities[1].r.contents=CONTENTS_CORPSE;
  presentationPolicy[1]=mode==0?G_TCE_PRESENTATION_LEGACY:mode==1?G_TCE_PRESENTATION_UNKNOWN:G_TCE_PRESENTATION_AFFECTED;
  presentationRegion[1]=mode==2?-1:mode==3?-2:TCE_POSE_REGION_HEAD;presentationFraction[1]=100;
  shoot(42,20,1000,1,0);
  CHECK(mode==0||mode==4?hitCount==1:hitCount==0);
  CHECK(queryCalls==1);
  CHECK(mode<2?skinCalls==0:skinCalls==1);
  if(mode>0&&mode<4)CHECK(g_entities[1].health==100&&spawnCalls==0&&matchCalls==0);
 }
}
#ifdef HAS_TCE_PRESENTATION_DAMAGE
static void livingCorpseEligibility(void)
{
 int region,reverse,kind;
 for(region=0;region<3;++region) {
  resetWorld();actor(1,100,1,0,0);g_entities[1].r.contents=CONTENTS_CORPSE;
  presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationRegion[1]=region;presentationFraction[1]=100;
  shoot(42,20,1000,1,0);
  CHECK(hitCount==1&&hitAmount[0]==(region==0?52:region==1?26:13));
  CHECK(spawnCalls==0&&skinCalls==1&&matchCalls==2&&g_entities[1].r.linked);
  CHECK(g_entities[1].client->ps.holdable[2+region]==hitAmount[0]);
 }
 /* Contents do not let a movement hull preempt the nearest actual skin. */
 for(reverse=0;reverse<2;++reverse) {
  resetWorld();actor(1,100,1,0,0);actor(2,105,1,0,0);g_entities[1].r.contents=CONTENTS_CORPSE;
  presentationPolicy[1]=presentationPolicy[2]=G_TCE_PRESENTATION_AFFECTED;
  presentationRegion[1]=presentationRegion[2]=TCE_POSE_REGION_BODY;
  presentationFraction[1]=reverse?90:140;presentationFraction[2]=reverse?140:90;
  shoot(42,20,1000,1,0);
  CHECK(hitCount==1&&hitOrder[0]==(reverse?1:2));
  CHECK(skinCalls==2&&matchCalls==2&&spawnCalls==0&&g_entities[1].r.linked);
 }
 /* MISS, unavailable geometry and UNKNOWN cannot become hull hits or block
  * another ordinary player. LEGACY retains its ordinary damage path. */
 for(kind=0;kind<4;++kind) {
  resetWorld();actor(1,90,1,0,0);actor(2,180,1,0,0);g_entities[1].r.contents=CONTENTS_CORPSE;
  presentationPolicy[1]=kind==0?G_TCE_PRESENTATION_UNKNOWN:kind==3?G_TCE_PRESENTATION_LEGACY:G_TCE_PRESENTATION_AFFECTED;
  presentationRegion[1]=kind==1?-1:-2;
  shoot(42,20,1000,1,0);
  CHECK(hitCount==1&&hitOrder[0]==(kind==3?1:2));
  CHECK(g_entities[kind==3?2:1].health==100&&matchCalls==0&&spawnCalls==1&&g_entities[1].r.linked);
 }
 /* A real unvisited dead corpse remains part of the engine world probe. */
 for(kind=0;kind<2;++kind) {
  tceBulletTraceContext_t context;trace_t tr;vec3_t start={0,0,0},end={300,0,0};
  resetWorld();actor(1,100,1,0,0);actor(2,180,1,0,0);
  g_entities[1].health=0;g_entities[1].s.eType=ET_CORPSE;g_entities[1].r.contents=CONTENTS_CORPSE;
  presentationPolicy[2]=G_TCE_PRESENTATION_AFFECTED;presentationRegion[2]=TCE_POSE_REGION_BODY;presentationFraction[2]=180;
  memset(&context,0,sizeof(context));context.shotTime=context.receiptCeiling=(uint32_t)level.time;
  context.hit[1]=(qboolean)kind;G_TCETraceBullet(&tr,&g_entities[0],start,end,&context);
  CHECK(tr.entityNum==(kind?2:1)&&context.hasSelection==(qboolean)kind);
  CHECK(g_entities[1].r.linked&&g_entities[1].r.contents==CONTENTS_CORPSE&&g_entities[1].health==0);
 }
 /* An ordinary living-CORPSE target retains the exact engine hull entry and
  * legacy/no-selection dispatch. It is not silently changed to a TC volume. */
 {
  tceBulletTraceContext_t context;trace_t tr;vec3_t start={0,0,42},end={300,0,42};
  resetWorld();actor(1,100,1,0,0);g_entities[1].r.contents=CONTENTS_CORPSE;
  memset(&context,0,sizeof(context));context.shotTime=context.receiptCeiling=(uint32_t)level.time;
  G_TCETraceBullet(&tr,&g_entities[0],start,end,&context);
  CHECK(tr.entityNum==1&&closeFloat(tr.fraction,79.f/300.f)&&!context.hasSelection);
  CHECK(queryCalls==1&&skinCalls==0&&g_entities[1].r.linked);
 }
}
static void presentationRegionsAndClock(void)
{
 int region,i;
 for(region=0;region<3;++region) {
  resetWorld();actor(1,100,1,0,0);presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;
  presentationRegion[1]=region;presentationFraction[1]=100;
  shoot(42,20,1000,1,0);
  CHECK(hitCount==1);CHECK(TCE_BulletClientDamage(&weaponDef[8],20,100,0)==26);
  CHECK(hitAmount[0]==(region==0?52:region==1?26:13));
  for(i=0;i<3;++i)CHECK(g_entities[1].client->ps.holdable[2+i]==(i==region?hitAmount[0]:0));
  CHECK(spawnCalls==0&&skinCalls==1&&matchCalls==2);
  CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==(region==0));
 }
 resetWorld();actor(1,100,1,0,0);presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;
 presentationRegion[1]=TCE_POSE_REGION_BODY;presentationFraction[1]=100;
 VectorSet(queryPoseOrigin[1],999,888,777);
 g_entities[0].client->pers.cmd.serverTime=7000;G_TimeShiftAllClients(&g_entities[0],qtrue);
 CHECK(g_entities[0].client->tceRewind.requestedTime==7000);
 CHECK(g_entities[1].client->tceRewind.requestedTime==7000&&g_entities[1].client->tceRewind.receiptCeiling==10000);
 g_entities[0].client->pers.cmd.serverTime=1;level.time=10050;shoot(42,20,1000,1,0);
 CHECK(seenShotTime==7000&&seenReceiptCeiling==10000);
 CHECK(seenPoseOrigin[0]==999&&seenPoseOrigin[1]==888&&seenPoseOrigin[2]==777);G_UnTimeShiftAllClients(&g_entities[0]);
 CHECK(!g_entities[0].client->tceRewind.shotQueryActive&&!g_entities[1].client->tceRewind.active);
 resetWorld();actor(1,100,1,0,0);presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationFraction[1]=100;
 g_entities[0].client->pers.cmd.serverTime=20000;G_TimeShiftAllClients(&g_entities[0],qtrue);
 CHECK(g_entities[0].client->tceRewind.requestedTime==10000);level.time=10050;shoot(42,10,1000,1,0);
 CHECK(seenShotTime==10000&&seenReceiptCeiling==10000);G_UnTimeShiftAllClients(&g_entities[0]);
 resetWorld();actor(1,100,1,0,0);presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationFraction[1]=100;
 g_entities[0].client->pers.cmd.serverTime=1;shoot(42,10,1000,1,0);
 CHECK(seenShotTime==10000&&seenReceiptCeiling==10000);
 /* The public/radius path never asks the new skin provider. */
 resetWorld();actor(1,100,1,0,0);
 G_Damage(&g_entities[1],&g_entities[0],&g_entities[0],NULL,NULL,10,DAMAGE_RADIUS,MOD_GRENADE_LAUNCHER);
 CHECK(queryCalls==0&&skinCalls==0&&matchCalls==0);
 CHECK(g_entities[1].client->ps.holdable[2]==0&&g_entities[1].client->ps.holdable[3]==10&&g_entities[1].client->ps.holdable[4]==10);
}
static void presentationOrderingAndCover(void)
{
 int reverse,kind,corpse;
 for(reverse=0;reverse<2;++reverse) {
  resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
  presentationPolicy[1]=presentationPolicy[2]=G_TCE_PRESENTATION_AFFECTED;
  presentationRegion[1]=presentationRegion[2]=TCE_POSE_REGION_BODY;
  presentationFraction[1]=reverse?90:140;presentationFraction[2]=reverse?140:90;
  shoot(42,20,1000,1,0);CHECK(hitCount==1&&hitOrder[0]==(reverse?1:2));CHECK(spawnCalls==0);
 }
 for(corpse=0;corpse<2;++corpse)for(kind=0;kind<7;++kind) {
  gentity_t *wall;resetWorld();actor(1,100,1,0,0);presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;
  if(corpse)g_entities[1].r.contents=CONTENTS_CORPSE;
  presentationRegion[1]=TCE_POSE_REGION_HEAD;
  wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;wall->r.contents=kind==1?CONTENTS_MISSILECLIP:CONTENTS_SOLID;
  VectorSet(wall->r.currentOrigin,100,0,0);VectorSet(wall->r.mins,0,-20,-20);VectorSet(wall->r.maxs,1,20,80);
  presentationFraction[1]=kind==2?99:kind==3?99.99999237060546875:kind==4?100.00000762939453125:kind==5?100.-1e-9:kind==6?100.+1e-9:100;
  shoot(42,20,1000,1,0);
  CHECK(kind==2||kind==3?hitCount==1:hitCount==0);CHECK(spawnCalls==0);
 }
 /* Two affected kills retain penetration/visited-corpse cleanup. */
 resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
 presentationPolicy[1]=presentationPolicy[2]=G_TCE_PRESENTATION_AFFECTED;
 presentationFraction[1]=90;presentationFraction[2]=120;
 shoot(42,60,1000,4,0);CHECK(hitCount==2&&hitOrder[0]==1&&hitOrder[1]==2);
 CHECK(g_entities[1].health<=0&&g_entities[2].health<=0);CHECK(skinCalls==3&&matchCalls==4);
 CHECK(g_entities[1].r.linked&&g_entities[2].r.linked);
 resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
 presentationPolicy[1]=presentationPolicy[2]=G_TCE_PRESENTATION_AFFECTED;
 presentationFraction[1]=90;presentationFraction[2]=120;
 shoot(42,20,1,4,0);CHECK(hitCount==1&&hitOrder[0]==1);
 /* An available skin miss cannot occlude a later ordinary target. */
 resetWorld();actor(1,90,1,0,0);actor(2,130,1,0,0);
 presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationRegion[1]=-1;
 shoot(42,20,1000,1,0);CHECK(hitCount==1&&hitOrder[0]==2);
 /* A later ordinary candidate may replace an affected tentative winner. */
 resetWorld();actor(1,90,1,0,0);actor(2,100,1,0,0);
 presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationFraction[1]=200;
 shoot(42,20,1000,1,0);CHECK(hitCount==1&&hitOrder[0]==2);CHECK(spawnCalls==1&&matchCalls==0);
}
static void selectedValidationAndReentrancy(void)
{
 vec3_t start={0,0,42},end={300,0,42},dir={1,0,0},point={100,0,42};int kind;
 for(kind=0;kind<8;++kind) {
  g_tceBulletDamageSelection_t s;resetWorld();actor(1,100,1,0,0);
  presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationFraction[1]=100;
  G_TCEPresentationQuery(1,7000,10000,&s.query);G_TCEPosePresentationTrace(&s.query,start,end,1,&s.trace);
  VectorCopy(start,s.start);VectorCopy(end,s.end);s.maxFraction=1;
  if(kind==0)s.query.clientNum=2;else if(kind==1)s.start[1]=1;else if(kind==2)s.maxFraction=.9;
  else if(kind==3)s.trace.state=G_TCE_POSE_TRACE_MISS;else if(kind==4)invalidateMatch=1;
  else if(kind==5)s.trace.surface.region=TCE_POSE_REGION_NONE;else if(kind==6)s.query.generation++;
  else s.trace.surface.fraction=1;
  G_TCEPresentationDamage(&g_entities[1],&g_entities[0],&g_entities[0],dir,point,20,0,MOD_MP40,&s);
  CHECK(g_entities[1].health==100&&spawnCalls==0&&hitCount==0);
  CHECK(luaDamageCalls==0);
  CHECK(g_entities[1].client->combatState==0&&g_entities[0].client->combatState==0);
  CHECK(g_entities[1].client->ps.holdable[2]==0&&g_entities[1].client->ps.holdable[3]==0&&g_entities[1].client->ps.holdable[4]==0);
 }
 resetWorld();actor(1,100,1,0,0);actor(2,180,1,0,0);g_entities[2].r.linked=qfalse;
 presentationPolicy[1]=presentationPolicy[2]=G_TCE_PRESENTATION_AFFECTED;
 presentationRegion[1]=TCE_POSE_REGION_HEAD;presentationRegion[2]=TCE_POSE_REGION_LEGS;
 presentationFraction[1]=100;presentationFraction[2]=180;nestedDamage=1;
 shoot(42,20,1000,1,0);
 CHECK(nestedGuard&&hitCount==2&&hitOrder[0]==2&&hitOrder[1]==1);
 CHECK(g_entities[1].client->ps.holdable[2]==52&&g_entities[1].client->ps.holdable[4]==0);
 CHECK(g_entities[2].client->ps.holdable[4]==6&&g_entities[2].client->ps.holdable[2]==0);
 CHECK(queryCalls==2&&skinCalls==2&&matchCalls==4);
 /* Recursive outer segments retain their original clock despite nested
  * callback mutation of the shooter's private current transaction fields. */
 resetWorld();actor(1,100,1,0,0);actor(2,180,1,0,0);actor(3,220,1,0,0);g_entities[3].r.linked=qfalse;
 presentationPolicy[1]=presentationPolicy[2]=presentationPolicy[3]=G_TCE_PRESENTATION_AFFECTED;
 presentationFraction[1]=100;presentationFraction[2]=180;presentationFraction[3]=220;
 presentationRegion[3]=TCE_POSE_REGION_LEGS;nestedDamage=nestedSecond=1;
 g_entities[0].client->pers.cmd.serverTime=7000;G_TimeShiftAllClients(&g_entities[0],qtrue);
 g_entities[3].r.linked=qfalse;shoot(42,20,1000,4,0);
 CHECK(hitCount==3&&hitOrder[0]==3&&hitOrder[1]==1&&hitOrder[2]==2);
 CHECK(seenShotTime==7000&&seenReceiptCeiling==10000);
 CHECK(queryCalls==4&&skinCalls==4&&matchCalls==6);
 G_UnTimeShiftAllClients(&g_entities[0]);
 CHECK(!g_entities[1].client->tceRewind.active&&!g_entities[2].client->tceRewind.active);
}
#endif
static void ordering(void)
{
    int reverse,prone;
    for(prone=0;prone<2;++prone)for(reverse=0;reverse<2;++reverse) {
        int first=reverse?1:2;resetWorld();
        actor(1,reverse?105:100,reverse?30:1,prone,prone?(reverse?180:0):0);
        actor(2,reverse?100:105,reverse?1:30,prone,prone?(reverse?0:180):0);
        shoot(prone?-6:42,20,1000,1,0);
        printf("order prone%d reverse%d: expected%d, hits%d first%d headshots%u\n",prone,reverse,first,hitCount,hitCount?hitOrder[0]:-1,g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots);
        CHECK(hitCount==1 && hitOrder[0]==first);
        CHECK(g_entities[first].client->tceLastAppliedDamage>0 && g_entities[3-first].client->tceLastAppliedDamage==0);
        CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==1);
    }
    resetWorld();actor(1,100,1,0,0);actor(2,120,30,0,0);shoot(42,20,1000,1,0);
    printf("order broad/real agree: expected1, hits%d first%d\n",hitCount,hitCount?hitOrder[0]:-1);
    CHECK(hitCount==1&&hitOrder[0]==1);CHECK(g_entities[1].client->tceLastAppliedDamage>0&&g_entities[2].client->tceLastAppliedDamage==0);
    CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==1);
}
static void penetrationAndCover(void)
{
    int kind;
    for(kind=0;kind<5;++kind) {
        resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
        if(kind<3){gentity_t *wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;wall->r.contents=kind==2?CONTENTS_MISSILECLIP:CONTENTS_SOLID;VectorSet(wall->r.currentOrigin,kind==0?97:75,0,0);VectorSet(wall->r.mins,-1,-20,-20);VectorSet(wall->r.maxs,1,20,80);}
        shoot(42,20,1000,4,0);
        if(kind<3){CHECK(hitCount==0);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==0);}
        else {CHECK(hitCount==2 && hitOrder[0]==2 && hitOrder[1]==1);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==2);CHECK(hitAmount[0]>0 && hitAmount[1]>0 && hitAmount[1]<=hitAmount[0]);}
    }
    resetWorld();actor(1,100,1,0,0);actor(2,100,1,0,0);shoot(42,20,1000,4,0);
    CHECK(hitCount==2 && hitOrder[0]!=hitOrder[1]);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==2);
    resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);shoot(42,20,1,4,0);CHECK(hitCount==1 && hitOrder[0]==2);
    resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);shoot(42,60,1000,4,0);CHECK(hitCount==2 && hitOrder[0]==2 && hitOrder[1]==1);
    CHECK(g_entities[1].health<=0 && g_entities[2].health<=0);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==2);
    resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
    {vec3_t start={0,-100,42},end={300,200,42};g_entities[2].r.currentOrigin[1]=5;g_entities[2].client->ps.origin[1]=5;
     Bullet_Fire_Extended(&g_entities[0],&g_entities[0],start,end,0,20,1000,0,0,0,1,8,42,1);}
    CHECK(hitCount==1 && hitOrder[0]==2);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==1);
    resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);weaponDef[8].unknown_134=1000000;
    {gentity_t *wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;wall->r.contents=CONTENTS_SOLID;wallSurface=0x0c000000;VectorSet(wall->r.currentOrigin,25,0,0);VectorSet(wall->r.mins,-5,-20,-20);VectorSet(wall->r.maxs,5,20,80);}
    shoot(42,20,1000000,4,1);
    CHECK(hitCount==2 && hitOrder[0]==2 && hitOrder[1]==1);CHECK(hitAmount[0]<=52 && hitAmount[1]<=52);
    CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==2);
    resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
    {gentity_t *wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;wall->r.contents=CONTENTS_SOLID;VectorSet(wall->r.currentOrigin,102,0,0);VectorSet(wall->r.mins,-1,-20,-20);VectorSet(wall->r.maxs,1,20,80);}
    shoot(42,20,1000,4,0);CHECK(hitCount==1 && hitOrder[0]==2);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==1);
    for(kind=0;kind<2;++kind){vec3_t start={0,0,42},end={8192,0,42};resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);
     Bullet_Fire_Extended(&g_entities[0],&g_entities[0],start,end,0,20,1000,kind?101:90,0,0,1,8,42,1);CHECK(kind?hitCount==1&&hitOrder[0]==2:hitCount==0);}
    resetWorld();actor(62,100,1,0,0);actor(63,105,30,0,0);shoot(42,20,1000,1,0);CHECK(hitCount==1&&hitOrder[0]==63);
    resetWorld();actor(1,100,1,0,0);actor(2,105,30,0,0);g_entities[1].client->ps.stats[STAT_TCE_FLAGS]|=0x200;g_entities[2].client->ps.stats[STAT_TCE_FLAGS]|=0x200;
    shoot(52.5f,20,1000,1,0);CHECK(hitCount==1&&hitOrder[0]==2);CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==1);
}
static void spawnProtectionROE(void)
{
    /* Deliberate CQB policy: unlike the original TC:E oracle, GT7 protects
     * the newly spawned target without MOD65 self-punishment. GT5 retains it.
     * Run the actual controller with both bullet and non-bullet input. */
    static const struct {
        int gametype, sameTeam, elapsed, bypass, damagesTarget, punishesAttacker;
    } cases[] = {
        {7,0,0,0,0,0}, {7,0,2999,0,0,0},
        {7,0,3000,0,1,0}, {7,0,3001,0,1,0},
        {7,1,2999,0,0,0}, {7,1,3000,0,1,0},
        {5,1,0,0,0,1}, {5,1,2999,0,0,1},
        {5,1,3000,0,1,0}, {5,0,2999,0,0,0},
        {5,0,3000,0,1,0}, {2,0,2999,0,0,0},
        {2,1,2999,0,0,0}, {2,0,3000,0,1,0},
        {7,0,2999,1,1,0}, {5,1,2999,1,1,0},
        {2,0,2999,1,1,0}
    };
    vec3_t direction={1,0,0},point={70,0,42};
    int c,bullet,i,mode;
    for(bullet=0;bullet<2;++bullet)for(c=0;c<(int)(sizeof(cases)/sizeof(cases[0]));++c)
    {
        gentity_t *target,*attacker;
        tce_damageResult_t result;
        resetWorld();actor(1,100,8,0,0);target=&g_entities[1];attacker=&g_entities[0];
        g_gametype.integer=cases[c].gametype;
        if(cases[c].sameTeam)target->client->sess.sessionTeam=attacker->client->sess.sessionTeam;
        target->client->tceDamageSpawnTime=10000;level.time=10000+cases[c].elapsed;
        /* A stale rewind/request clock must not extend or bypass protection. */
        attacker->client->pers.cmd.serverTime=1;
        attacker->client->tceRewind.requestedTime=1;
        regionFractionOverride=1;regionProbeFractions[0]=regionProbeFractions[2]=1;regionProbeFractions[1]=.25f;
        result=G_Damage(target,attacker,attacker,direction,point,20,
            cases[c].bypass?DAMAGE_NO_PROTECTION:0,bullet?MOD_MP40:MOD_UNKNOWN);
        CHECK(cases[c].damagesTarget?target->health<100:target->health==100);
        CHECK(target->client->tceLastAppliedDamage==(cases[c].damagesTarget?20:0));
        CHECK(target->client->ps.holdable[2]==0);
        CHECK(target->client->ps.holdable[3]==(cases[c].damagesTarget?(bullet?20:10):0));
        CHECK(target->client->ps.holdable[4]==(cases[c].damagesTarget&&!bullet?20:0));
        CHECK(cases[c].punishesAttacker?attacker->health<=0:attacker->health==100);
        CHECK(attacker->client->tceLastAppliedDamage==(cases[c].punishesAttacker?1000:0));
        CHECK(attacker->client->ps.holdable[2]==0);
        CHECK(attacker->client->ps.holdable[3]==(cases[c].punishesAttacker?500:0));
        CHECK(attacker->client->ps.holdable[4]==(cases[c].punishesAttacker?1000:0));
        CHECK(cases[c].punishesAttacker?attacker->deathType==65:attacker->deathType==0);
        CHECK(luaDamageCalls==(cases[c].damagesTarget||cases[c].punishesAttacker?1:0));
        CHECK(hitCount==(cases[c].damagesTarget||cases[c].punishesAttacker?1:0));
        if(hitCount)CHECK(hitOrder[0]==(cases[c].punishesAttacker?0:1)&&hitAmount[0]==(cases[c].punishesAttacker?1000:20));
        if(!cases[c].damagesTarget)CHECK(result==0);
        CHECK(spawnCalls==(bullet&&cases[c].damagesTarget?1:0));
    }
    /* Self/world attacks bypass the client-versus-other-client timer guard.
     * NO_PROTECTION continues to respect independent godmode immunity. */
    for(mode=0;mode<3;++mode)
    {
        gentity_t *target;
        resetWorld();actor(1,100,8,0,0);target=&g_entities[1];
        g_gametype.integer=7;target->client->tceDamageSpawnTime=level.time;
        if(mode==2)target->flags|=FL_GODMODE;
        G_Damage(target,NULL,mode==0?target:NULL,direction,point,20,
            mode==2?DAMAGE_NO_PROTECTION:0,MOD_UNKNOWN);
        CHECK(mode==2?target->health==100:target->health<100);
        CHECK(target->client->tceLastAppliedDamage==(mode==2?0:20));
        CHECK(g_entities[0].health==100&&luaDamageCalls==(mode==2?0:1));
    }
    /* Full bullet traversal must retain immunity, then hit at exactly 3000. */
    for(i=0;i<2;++i)
    {
        resetWorld();actor(1,100,8,0,0);g_gametype.integer=7;
        g_entities[1].client->tceDamageSpawnTime=level.time-(i?3000:2999);
        shoot(42,20,1000,1,0);
        CHECK(g_entities[0].health==100);
        CHECK(i?g_entities[1].health<100:g_entities[1].health==100);
        CHECK(g_entities[1].client->tceLastAppliedDamage==(i?52:0));
        CHECK(hitCount==i&&luaDamageCalls==i);
    }
    printf("spawn protection: GT7 immunity without ROE, exact 3000ms boundary, GT5 friendly ROE, GT2 and bypasses\n");
}
#include "tools/modernize/hitbox_regional_cases.h"
int main(void)
{
    ordering();penetrationAndCover();presentationAvailability();
    spawnProtectionROE();
    regionalGrid();regionalCoverAndCompatibility();regionalIntegerAndFractionBoundaries();
#ifdef HAS_TCE_PRESENTATION_DAMAGE
    livingCorpseEligibility();presentationRegionsAndClock();presentationOrderingAndCover();selectedValidationAndReentrancy();
#endif
    printf("full bullet/damage ordering: %d checks, %d failures; analytic collision services, no engine claim\n",tests,failures);return failures?1:0;
}
