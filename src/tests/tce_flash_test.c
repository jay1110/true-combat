#include "../cgame/cg_local.h"
#include "../cgame/tce_flash.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
cg_t cg;cgs_t cgs;int tceSmokeNewBBox;
static snapshot_t snap;
static unsigned char *base,rawSnap[4096];
static int traceCount,clearTrace,puffs,draws,volume,cases;
static float alpha;
static void patch(void *at,void *fn){DWORD o;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);}
static void require(int value,const char *what){if(!value){fprintf(stderr,"flash case%d %s\n",cases,what);exit(1);}}
void QDECL Com_Error(int code,const char *fmt,...){exit(3);}
void QDECL Com_Printf(const char *fmt,...){(void)fmt;}
void CG_Trace(trace_t *tr,const vec3_t a,const vec3_t b,const vec3_t c,const vec3_t d,int skip,int mask){memset(tr,0,sizeof(*tr));tr->fraction=(traceCount++>=clearTrace)?1:0.5f;require(mask==0x2000001,"mask");}
qhandle_t trap_R_RegisterShader(const char *name){return 11;}
sfxHandle_t trap_S_RegisterSound(const char *name,qboolean compressed){return 12;}
localEntity_t *CG_SmokePuff(const vec3_t p,const vec3_t v,float radius,float r,float g,float b,float a,float life,int now,int fade,int flags,qhandle_t shader){++puffs;require(radius==256&&life==250&&flags==0x101,"puff");return NULL;}
void CG_FillRect(float x,float y,float w,float h,const float *color){++draws;alpha=color[3];require(x==-5&&y==-85&&w==862&&h==650,"draw rectangle");}
void trap_S_AddLoopingSound(const vec3_t p,const vec3_t v,sfxHandle_t snd,int vol,int time){volume=vol;}
#define I(a) (*(int *)(base+(a)-0x30000000))
#define F(a) (*(float *)(base+(a)-0x30000000))
int main(int argc,char **argv){HMODULE h;int d,dir,large,aim,blocked,stack,only;vec3_t pos;void(__cdecl *original)(const vec3_t,int);void(__cdecl *draw)(void),(__cdecl *ring)(void);
 require(argc==2,"args");h=LoadLibraryA(argv[1]);require(h!=NULL,"dll");base=(unsigned char *)h;
 original=(void *)((char *)h+0x774f0);draw=(void *)((char *)h+0x27c40);ring=(void *)((char *)h+0x275f0);
 patch(base+0x58420,CG_Trace);patch(base+0x2b570,CG_SmokePuff);patch(base+0x29850,CG_FillRect);patch(base+0x650c0,trap_S_AddLoopingSound);
 cg.snap=&snap;*(void **)(base+0x40352c4)=rawSnap;
 I(0x32588280)=11;I(0x325888f8)=12;
 for(d=0;d<10;++d)for(dir=0;dir<5;++dir)for(large=0;large<2;++large)for(aim=0;aim<2;++aim)for(blocked=0;blocked<9;++blocked)for(stack=0;stack<3;++stack)for(only=0;only<2;++only){
  int expected[3],expectedPuffs,expectedTraces;float distances[]={0,1,100,250,299,400,499,625,999,1500};
  memset(&cg,0,sizeof(cg));memset(&cgs,0,sizeof(cgs));memset(rawSnap,0,sizeof(rawSnap));cg.snap=&snap;cg.time=I(0x3407dea4)=100000;
  cg.refdef.viewaxis[0][0]=F(0x3407f028)=1;F(0x3407f02c)=F(0x3407f030)=0;
  cg.refdef.fov_x=F(0x3407f014)=aim?20:90;cg.tceAimActive=I(0x340a496c)=aim;
  tceSmokeNewBBox=I(0x33fdf380)=large;I(0x3257e100)=0;
  F(0x3407f01c)=F(0x3407f020)=F(0x3407f024)=0;
  TCE_CG_ResetFlash();tceFlash.blindUntil=I(0x340a49c0)=cg.time+stack*4000;
  tceFlash.blindActive=I(0x340a49c4)=stack?1:0;tceFlash.deafUntil=I(0x340a49c8)=cg.time+stack*5000;
  VectorSet(pos,dir==0?distances[d]:dir==1?-distances[d]:0,dir==2?distances[d]:0,-2);
  if(dir==3){pos[0]=distances[d]*.6f;pos[1]=distances[d]*.8f;}
  if(dir==4){pos[0]=distances[d]*.95f;pos[1]=distances[d]*.1f;pos[2]=distances[d]*.2f-2;}
  traceCount=puffs=0;clearTrace=blocked;original(pos,only);
  expected[0]=I(0x340a49c0);expected[1]=I(0x340a49c4);expected[2]=I(0x340a49c8);expectedPuffs=puffs;expectedTraces=traceCount;
  traceCount=puffs=0;TCE_CG_FlashBang(pos,only);
  if(tceFlash.blindUntil!=expected[0]||tceFlash.blindActive!=expected[1]||tceFlash.deafUntil!=expected[2]){
    fprintf(stderr,"d%d dir%d large%d aim%d block%d stack%d only%d original%d/%d/%d source%d/%d/%d\n",d,dir,large,aim,blocked,stack,only,expected[0],expected[1],expected[2],tceFlash.blindUntil,tceFlash.blindActive,tceFlash.deafUntil);return 1;}
  require(puffs==expectedPuffs&&traceCount==expectedTraces,"callback parity");++cases;
 }
 for(d=0;d<2;++d)for(blocked=0;blocked<2;++blocked){
  int b0,b1,b2;
  TCE_CG_ResetFlash();I(0x340a49c0)=I(0x340a49c4)=I(0x340a49c8)=0;
  cgs.gametype=I(0x3257e100)=d?5:7;
  snap.ps.powerups[PW_INVULNERABLE]=*(int *)(rawSnap+0x180)=blocked?1000:0;
  VectorSet(pos,100,0,-2);clearTrace=0;traceCount=0;original(pos,0);
  b0=I(0x340a49c0);b1=I(0x340a49c4);b2=I(0x340a49c8);
  traceCount=0;TCE_CG_FlashBang(pos,0);
  require(b0==tceFlash.blindUntil&&b1==tceFlash.blindActive&&b2==tceFlash.deafUntil,"objective spawn protection");++cases;
 }
 for(d=0;d<9;++d)for(blocked=0;blocked<6;++blocked){int ends[]={-1,0,1,100,2999,3000,3001,6000,12000},ed,ev;float ea,ef;
  cg.time=I(0x3407dea4)=10000;cg.snap=&snap;I(0x340352c4)=(int)rawSnap;
  cg.renderingThirdPerson=I(0x3407deb8)=blocked==1;cgs.gamestate=I(0x33fb7810)=blocked==2?1:0;
  cg.predictedPlayerState.pm_type=I(0x3407dec4)=blocked==3?2:0;cg.predictedPlayerState.pm_flags=I(0x3407decc)=blocked==4?4096:0;
  cg.predictedPlayerState.stats[STAT_HEALTH]=I(0x3407df90)=blocked==5?0:100;
  tceFlash.blindUntil=I(0x340a49c0)=10000+ends[d];tceFlash.deafUntil=I(0x340a49c8)=10000+ends[d];
  draws=0;alpha=-1;volume=-1;draw();ring();ed=draws;ea=alpha;ev=volume;ef=F(0x340a49cc);
  draws=0;alpha=-1;volume=-1;TCE_CG_DrawFlashBang();TCE_CG_UpdateFlashRinging();
  require(ed==draws&&fabs(ea-alpha)<.00001f&&ev==volume&&fabs(ef-tceFlash.deafness)<.00001f,"draw/ringing parity");++cases;
 }
 printf("TC flashbang PASS %d complete controller/draw/ringing cases\n",cases);return 0;
}
