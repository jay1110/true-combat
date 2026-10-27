/* Actual diagnostic transport/cache, with explicit engine/asset services.
 * This proves budgets, timestamps and field mapping, not BSP/render parity. */
#ifdef HIT_DEBUG_CLIENT
#include "src/cgame/cg_local.h"
#include "src/cgame/cg_tce_hit_debug.h"
#else
#include "src/game/g_local.h"
#include "src/game/g_tce_hit_debug.h"
#include "src/game/g_tce_pose.h"
#include "src/game/g_tce_pose_presentation.h"
#endif
#include "src/game/tce_hit_debug.h"
#include <stdio.h>
#include <math.h>
#include <float.h>
#ifdef vsnprintf
#undef vsnprintf
#endif
static int checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
vec_t VectorNormalize(vec3_t v) {float n=(float)sqrt(DotProduct(v,v));if(n)VectorScale(v,1.f/n,v);return n;}
#ifdef HIT_DEBUG_CLIENT
cg_t cg;cgs_t cgs;
static int polys,labels,viewportDepth;static char lastLabel[256];static polyVert_t lastPoly[4];
void trap_R_AddPolyToScene(qhandle_t shader,int count,const polyVert_t *verts)
{int i,j;(void)shader;CHECK(count==4);++polys;memcpy(lastPoly,verts,sizeof(lastPoly));for(i=0;i<4;++i)for(j=0;j<3;++j)CHECK(isfinite(verts[i].xyz[j]));}
void CG_DrawSmallString(int x,int y,const char *text,float alpha)
{(void)x;(void)y;(void)alpha;++labels;snprintf(lastLabel,sizeof(lastLabel),"%s",text);}
void CG_HudViewportBegin(float x,float y) {CHECK(x==0&&y==0&&viewportDepth==0);++viewportDepth;}
void CG_HudViewportEnd(void) {CHECK(viewportDepth==1);--viewportDepth;}
int CG_Text_Width_Ext(const char *text,float scale,int limit,fontInfo_t *font)
{(void)limit;(void)font;return (int)(strlen(text)*30*scale);}
void CG_Text_Paint_Ext(float x,float y,float sx,float sy,vec4_t color,const char *text,float adjust,int limit,int style,fontInfo_t *font)
{(void)y;(void)color;(void)adjust;(void)limit;(void)style;(void)font;CHECK(viewportDepth==1&&x==8&&sx>0&&sx<=.14f&&sx==sy);
 CHECK(CG_Text_Width_Ext(text,sx,0,font)<=360);++labels;snprintf(lastLabel,sizeof(lastLabel),"%s",text);}
void QDECL Com_sprintf(char *dest,int size,const char *format,...)
{va_list args;va_start(args,format);vsnprintf(dest,(size_t)size,format,args);va_end(args);}
/* ACTUAL_CLIENT */
static entityState_t packet(int client,int layer,int time)
{
    entityState_t e;int i;memset(&e,0,sizeof(e));e.dmgFlags=TCE_HIT_DEBUG_BOX_EVENT;
    e.otherEntityNum=client;e.effect1Time=layer;e.time=time;e.time2=time-50;
    for(i=0;i<3;++i){e.pos.trBase[i]=i+.25f;e.origin2[i]=i+6.5f;e.angles[i]=i+10.25f;e.angles2[i]=i+16.5f;e.apos.trBase[i]=i+20.25f;e.apos.trDelta[i]=i+26.5f;}
    return e;
}
int main(int argc,char **argv)
{
    entityState_t e,valid;FILE *file;int i,layer;
    cg.time=10000;for(i=0;i<3;++i)cg.refdef.viewaxis[i][i]=1;CG_TCEHitDebugReset();
    {vec3_t start={10,0,0},end={10,5,0};byte color[4]={255,0,0,255};
        DebugLine(start,end,color);CHECK(lastPoly[0].xyz[1]==5&&lastPoly[2].xyz[1]==0);
        CHECK(lastPoly[0].xyz[2]<0&&lastPoly[1].xyz[2]>0);}
    CHECK(argc==2);if(argc!=2)return 1;
    file=fopen(argv[1],"rb");CHECK(file!=NULL);if(!file)return 1;
    CHECK(fread(&e,sizeof(e),1,file)==1);CHECK(fgetc(file)==EOF);fclose(file);
    CHECK(CG_TCEHitDebugEvent(&e)==1);
    CHECK(!memcmp(debugSamples[0][e.otherEntityNum].low[0],e.pos.trBase,sizeof(vec3_t)));
    CHECK(!memcmp(debugSamples[0][e.otherEntityNum].high[2],e.apos.trDelta,sizeof(vec3_t)));
    polys=0;CG_TCEHitDebugDraw();CHECK(polys==36);CG_TCEHitDebugOverlay();CHECK(labels>=2);
    CG_TCEHitDebugReset();valid=packet(0,0,cg.time);
    for(i=0;i<9;++i){e=valid;if(i==0)e.otherEntityNum=MAX_CLIENTS;else if(i==1)e.effect1Time=3;
        else if(i==2)e.effect2Time=2;else if(i==3)e.pos.trBase[0]=NAN;
        else if(i==4)e.origin2[0]=INFINITY;else if(i==5)e.pos.trBase[0]=e.origin2[0]+1;
        else if(i==6)e.origin2[0]=e.pos.trBase[0]+1024;else if(i==7)e.time=cg.time-1101;else e.time=cg.time+201;
        CHECK(CG_TCEHitDebugEvent(&e)==1);CHECK(!debugSamples[0][0].valid);}
    e=valid;e.dmgFlags=1;CHECK(CG_TCEHitDebugEvent(&e)==0);
    for(layer=0;layer<3;++layer)for(i=0;i<MAX_CLIENTS;++i){e=packet(i,layer,cg.time);CHECK(CG_TCEHitDebugEvent(&e));}
    polys=0;CG_TCEHitDebugDraw();CHECK(debugVisible==80&&polys==2880);
    /* Old duplicate packets cannot restore an earlier pose. */
    e=packet(63,0,cg.time-1);e.pos.trBase[0]=-50;CG_TCEHitDebugEvent(&e);CHECK(debugSamples[0][63].low[0][0]==.25f);
    cg.time+=1101;polys=0;CG_TCEHitDebugDraw();CHECK(polys==0&&debugVisible==0);
    e=packet(0,0,cg.time);CG_TCEHitDebugEvent(&e);CG_TCEHitDebugReset();polys=0;CG_TCEHitDebugDraw();CHECK(polys==0);
    e=packet(0,0,cg.time);e.effect2Time=1;CG_TCEHitDebugEvent(&e);CG_TCEHitDebugDraw();CG_TCEHitDebugOverlay();CHECK(strstr(lastLabel,"BOUNDS")!=NULL);
    {snapshot_t snap;memset(&snap,0,sizeof(snap));cg.snap=&snap;cg.clientNum=0;
        CG_TCEHitDebugReset();e=packet(0,0,cg.time);CG_TCEHitDebugEvent(&e);e=packet(1,0,cg.time);CG_TCEHitDebugEvent(&e);
        cg.renderingThirdPerson=qfalse;snap.ps.clientNum=0;polys=0;CG_TCEHitDebugDraw();CHECK(debugVisible==1&&polys==36);
        cg.renderingThirdPerson=qtrue;polys=0;CG_TCEHitDebugDraw();CHECK(debugVisible==2&&polys==72);
        CG_TCEHitDebugReset();e=packet(0,0,cg.time);CG_TCEHitDebugEvent(&e);e=packet(2,0,cg.time);CG_TCEHitDebugEvent(&e);
        cg.renderingThirdPerson=qfalse;snap.ps.clientNum=2;snap.ps.pm_flags|=PMF_FOLLOW;polys=0;CG_TCEHitDebugDraw();CHECK(debugVisible==1&&polys==36);
        CHECK(debugSamples[0][0].valid&&debugSamples[0][2].valid);cg.snap=NULL;
    }
    printf("hit debug client: %d checks, %d failures; actual server packet, atomic malformed rejection, 80-sample/2880-quad cap, expiry/reset, mesh label\n",checks,failures);return failures?1:0;
}
#else
gentity_t g_entities[MAX_GENTITIES];level_locals_t level;
vmCvar_t g_debugBullets,g_antilag,g_debugBulletsClient,vote_allow_antilag;
static int voteSetting,voteCalls;
int G_voteProcessOnOff(gentity_t *ent,char *arg,char *arg2,qboolean referee,int setting,int allow,int type)
{(void)ent;(void)arg;(void)arg2;(void)referee;(void)allow;(void)type;voteSetting=setting;return G_OK;}
void G_voteSetOnOff(const char *desc,const char *cvar)
{CHECK(!strcmp(desc,"Anti-Lag")&&!strcmp(cvar,"g_antilag"));++voteCalls;}
static gclient_t clients[MAX_CLIENTS];static gentity_t events[1024];static int eventCount,policies[MAX_CLIENTS],boundsReady=1;
void QDECL G_Printf(const char *format,...) {(void)format;}
void VectorNormalizeFast(vec3_t v) {(void)VectorNormalize(v);}
void AngleVectors(const vec3_t angles,vec3_t forward,vec3_t right,vec3_t up)
{(void)angles;if(forward)VectorSet(forward,1,0,0);if(right)VectorSet(right,0,-1,0);if(up)VectorSet(up,0,0,1);}
gentity_t *G_TempEntity(vec3_t origin,int event)
{gentity_t *e;if(eventCount>=1024)abort();e=&events[eventCount++];memset(e,0,sizeof(*e));e->s.event=event;VectorCopy(origin,e->s.pos.trBase);return e;}
int G_TCEPresentationQuery(int client,uint32_t time,uint32_t ceiling,g_tcePresentationQuery_t *out)
{(void)ceiling;memset(out,0,sizeof(*out));out->clientNum=client;out->serverTime=time;return policies[client];}
int G_TCEPosePresentationBounds(const g_tcePresentationQuery_t *query,float low[3][3],float high[3][3])
{int i,j;(void)query;for(i=0;i<3;++i)for(j=0;j<3;++j){low[i][j]=i*10+j+.25f;high[i][j]=low[i][j]+6.5f;}return boundsReady;}
/* ACTUAL_SERVER */
/* ACTUAL_ANTILAG_VOTE */
static void setup(void)
{
    int i;memset(g_entities,0,sizeof(g_entities));memset(clients,0,sizeof(clients));memset(&level,0,sizeof(level));
    level.time=10000;level.startTime=123;eventCount=0;g_debugBullets.integer=0;g_antilag.integer=1;g_debugBulletsClient.integer=-1;
    debugFrame=debugStart=debugNext=debugCursor=debugLogLines=0;memset(debugShotEvents,0,sizeof(debugShotEvents));
    for(i=0;i<MAX_CLIENTS;++i){gentity_t *e=&g_entities[i];e->inuse=qtrue;e->health=100;e->s.number=i;e->client=&clients[i];
        e->client->pers.connected=CON_CONNECTED;e->client->sess.sessionTeam=TEAM_AXIS;e->client->ps.weapon=8;
        VectorSet(e->r.currentOrigin,1000+i+.375f,2000+i+.625f,17.875f);policies[i]=G_TCE_PRESENTATION_LEGACY;}
}
int main(int argc,char **argv)
{
    FILE *file;int i,seen[MAX_CLIENTS]={0};entityState_t first;
    setup();G_TCEHitDebugFrame();CHECK(eventCount==0);
    for(i=0;i<8;++i){g_antilag.integer=i;CHECK(G_AntiLag_v(&g_entities[0],0,"antilag","1",qfalse)==G_OK);CHECK(voteSetting==!!(i&1));}
    CHECK(G_AntiLag_v(&g_entities[0],0,NULL,NULL,qfalse)==G_OK&&voteCalls==1);
    g_debugBullets.integer=4;for(i=0;i<8;++i){G_TCEHitDebugFrame();level.time+=100;}
    CHECK(eventCount==64);for(i=0;i<eventCount;++i){++seen[events[i].s.otherEntityNum];CHECK(events[i].r.svFlags&SVF_BROADCAST);}
    for(i=0;i<MAX_CLIENTS;++i)CHECK(seen[i]==1);
    first=events[0].s;CHECK(first.dmgFlags==100&&first.effect1Time==0&&first.effect2Time==0);
    {tceDamagePose_t pose;tceDamageVolume_t v[3];TCE_ClientDamagePose(&g_entities[0],&pose);TCE_DamageVolumes(&pose,v);
        for(i=0;i<3;++i){CHECK(first.pos.trBase[i]==v[0].origin[i]+v[0].mins[i]);CHECK(first.apos.trDelta[i]==v[2].origin[i]+v[2].maxs[i]);}}
    CHECK(argc==2);if(argc!=2)return 1;file=fopen(argv[1],"wb");CHECK(file!=NULL);if(!file)return 1;CHECK(fwrite(&first,sizeof(first),1,file)==1);fclose(file);
    setup();g_debugBullets.integer=4;g_debugBulletsClient.integer=63;
    G_TCEHitDebugFrame();CHECK(eventCount==1&&events[0].s.otherEntityNum==63);level.time+=50;G_TCEHitDebugFrame();CHECK(eventCount==1);
    level.time+=50;g_entities[63].r.currentOrigin[1]+=13;G_TCEHitDebugFrame();CHECK(eventCount==2&&events[1].s.angles[1]>events[0].s.angles[1]);
    level.time+=100;policies[63]=G_TCE_PRESENTATION_UNKNOWN;G_TCEHitDebugFrame();CHECK(eventCount==2);
    level.time+=100;policies[63]=G_TCE_PRESENTATION_AFFECTED;G_TCEHitDebugFrame();CHECK(eventCount==3&&events[2].s.effect2Time==1);
    level.time+=100;boundsReady=0;G_TCEHitDebugFrame();CHECK(eventCount==3);boundsReady=1;
    setup();g_antilag.integer=6;
    g_entities[0].client->tceRewind.shotQueryActive=qtrue;g_entities[0].client->tceRewind.requestedTime=9850;g_entities[0].client->tceRewind.receiptCeiling=10000;
    for(i=0;i<20;++i){G_TCEHitDebugShot(&g_entities[0],2);G_TCEHitDebugShot(&g_entities[0],1);}
    CHECK(eventCount==16);CHECK(events[0].s.time2==10000&&events[8].s.time2==9850);
    setup();g_antilag.integer=6;g_debugBulletsClient.integer=63;G_TCEHitDebugShot(&g_entities[0],2);G_TCEHitDebugShot(&g_entities[0],1);CHECK(eventCount==2&&events[0].s.otherEntityNum==63&&events[1].s.otherEntityNum==63);
    g_debugBullets.integer=8;for(i=0;i<96;++i)CHECK(G_TCEHitDebugLogAllowed());CHECK(!G_TCEHitDebugLogAllowed());level.time+=100;CHECK(G_TCEHitDebugLogAllowed());
    printf("hit debug server: %d checks, %d failures; 8/100ms round-robin, selected 100ms refresh, exact boxes/times, unknown/mesh, separate shot budgets and bounded logs\n",checks,failures);return failures?1:0;
}
#endif
