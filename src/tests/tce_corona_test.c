#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_ents.c"
#include <windows.h>
#include <stdio.h>
typedef struct{int type,shader,count;byte payload[512];} event_t;
static event_t events[64];static int ne,traceMode;
static qhandle_t shader(const char*s){const char*n[]={"flareShader","flareShader2","flareShader3","flareShader4","coneShader","coneShader2","coneShader3","coneShader4","lensflare1","lensflare2"};int i;for(i=0;i<10;i++)if(!strcmp(s,n[i]))return 100+i;return 777;}
static void poly(qhandle_t s,int n,const polyVert_t*p){event_t*e=&events[ne++];memset(e,0,sizeof(*e));e->type=1;e->shader=s;e->count=n;memcpy(e->payload,p,n*sizeof(*p));}
static void entity(const refEntity_t*r){event_t*e=&events[ne++];memset(e,0,sizeof(*e));e->type=2;memcpy(e->payload,r,0xec);}
static void trace(trace_t*t,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t z,int pass,int mask){event_t*e=&events[ne++];memset(e,0,sizeof(*e));e->type=3;e->shader=mask;e->count=pass;memcpy(e->payload,a,12);memcpy(e->payload+12,z,12);memset(t,0,sizeof(*t));t->fraction=traceMode==1?.5f:1;t->surfaceFlags=traceMode==2?SURF_SKY:0;}
static void hook(void*at,void*fn){DWORD o,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE dll;byte*b,raw[0xa94],snapRaw[0x600];centity_t cent;snapshot_t snap;void(__cdecl*original)(void*);int c,i,expectedN,failed=0;static event_t expected[64];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x321c0);hook(b+0x652b0,poly);hook(trap_R_AddPolyToScene,poly);hook(b+0x65290,entity);hook(trap_R_AddRefEntityToScene,entity);hook(b+0x58420,trace);hook(CG_Trace,trace);hook(trap_R_RegisterShader,shader);
 /* Cached shader handles must be initialized on both sides, as CG_Init does. */
 for(i=0;i<4;i++){cgs.media.tceCoronaFlare[i]=100+i;cgs.media.tceCoronaCone[i]=104+i;}
 cgs.media.tceLensFlare[0]=108;cgs.media.tceLensFlare[1]=109;
 for(i=0;i<8;i++)*(int*)(b+0x2588280+i*4)=100+i;*(int*)(b+0x2588274)=108;*(int*)(b+0x2588278)=109;
 for(c=0;c<2016;c++){memset(&cent,0,sizeof(cent));memset(raw,0,sizeof(raw));memset(&snap,0,sizeof(snap));memset(snapRaw,0,sizeof(snapRaw));cg.snap=&snap;*(void**)(b+0x40352c4)=snapRaw;
  cg.time=*(int*)(b+0x407dea4)=1000;cg.clientFrame=*(int*)(b+0x40352a0)=20;cg.cameraShakeTime=*(int*)(b+0x4081560)=-1;cg.refdef_current=&cg.refdef;memset(&cg.refdef,0,sizeof(cg.refdef));AxisClear(cg.refdef.viewaxis);memcpy(b+0x407f004,&cg.refdef,sizeof(cg.refdef));*(void**)(b+0x40863ac)=b+0x407f004;
  cg.tceSparkIntensity=*(float*)(b+0x40a4c04)=.5f;cg.tceEyeFlareFrame=*(int*)(b+0x40a4c1c)=0;cg.tceEyeSampleTime=*(int*)(b+0x40a4bd0)=0;cg.tceEyeFlare=*(float*)(b+0x40a4bdc)=0;cg.tceCoronaBlendAlpha=*(float*)(b+0x40a49e4)=0;VectorClear(cg.tceCoronaBlendColor);memset(b+0x40a49e8,0,12);cg.tceAimActive=*(int*)(b+0x40a496c)=0;cg.tceAimComplete=*(int*)(b+0x40a4970)=0;
  cent.currentState.clientNum=c%3;cent.currentState.otherEntityNum2=(c/3)%4;cent.currentState.dl_intensity=0x80c0ff;cent.currentState.density=64;cent.currentState.effect3Time=128;cent.currentState.effect2Time=(c/12)%2?8:0;cent.currentState.frame=16;cent.currentState.eventParm=0;cent.currentState.modelindex2=(c/24)%2;cent.currentState.effect1Time=(c/48)%4;cent.lerpOrigin[0]=128;cent.lerpOrigin[1]=12;cent.lerpOrigin[2]=64;traceMode=(c/192)%3;
  cent.miscTime=900;cent.tceCoronaLastVisible=900;cent.tceCoronaVisibility=.25f;  if(c>=576) {
   static const int ages[]={0,49,50,499,500,501};int k=c-576,kind=k%10;
   cent.currentState.clientNum=kind==0?1:0;cent.currentState.frame=0;
   cent.lerpOrigin[0]=kind==1?-128:128;cent.lerpOrigin[1]=0;cent.lerpOrigin[2]=kind==2?-12:kind==3?12:0;
   cent.currentState.dmgFlags=kind==2||kind==3;cent.currentState.modelindex2=0;
   cent.currentState.effect2Time=kind==4?8:0;cent.currentState.frame=kind==4?16:0;
   cg.tceAimActive=*(int*)(b+0x40a496c)=kind>=5;cg.tceAimComplete=*(int*)(b+0x40a4970)=kind>=6;
   cg.renderingThirdPerson=*(int*)(b+0x407deb8)=kind==7;
   snap.ps.weapon=*(int*)(snapRaw+0xd0)=57;weaponDef[57].scoped=*(float*)(b+0x4c1e094+57*0x1cc)=kind==8?0:1;
   cent.miscTime=1000-ages[(k/10)%6];cent.tceCoronaLastVisible=1000-ages[(k/60)%6];
   traceMode=(k/360)%3;
   if(kind==9){cg.cameraShakeTime=*(int*)(b+0x4081560)=1400;cg.cameraShakeLength=*(float*)(b+0x408156c)=1000;cg.cameraShakeScale=*(float*)(b+0x4081568)=.37f;cg.cameraShakePhase=*(float*)(b+0x4081564)=.63f;}
  } else cg.renderingThirdPerson=*(int*)(b+0x407deb8)=0;memcpy(raw,&cent.currentState,sizeof(entityState_t));memcpy(raw+0x710,cent.lerpOrigin,12);*(int*)(raw+0x260)=cent.miscTime;*(int*)(raw+0xa78)=cent.tceCoronaLastVisible;*(float*)(raw+0xa90)=.25f;
  ne=0;original(raw);expectedN=ne;memcpy(expected,events,sizeof(events));ne=0;CG_Corona(&cent);
  if(ne!=expectedN||memcmp(events,expected,ne*sizeof(event_t))||memcmp(&cg.tceCoronaBlendAlpha,b+0x40a49e4,4)||memcmp(cg.tceCoronaBlendColor,b+0x40a49e8,12)||memcmp(&cg.tceEyeFlare,b+0x40a4bdc,4)||memcmp(&cent.tceCoronaVisibility,raw+0xa90,4)||cent.tceCoronaLastVisible!=*(int*)(raw+0xa78)||cent.miscTime!=*(int*)(raw+0x260)){printf("corona case%d events%d/%d\n",c,expectedN,ne); for(i=0;i<ne*(int)sizeof(event_t);i++) if(((byte*)expected)[i]!=((byte*)events)[i]) { int k=i/sizeof(event_t),o=i%sizeof(event_t); printf("first event%d offset%d expected%02x actual%02x\n",k,o,((byte*)expected)[i],((byte*)events)[i]); if(o>=12)printf("float expected%.9g actual%.9g\n",*(float*)(expected[k].payload+((o-12)&~3)),*(float*)(events[k].payload+((o-12)&~3))); } printf("state eye %.9g/%.9g visibility %.9g/%.9g alpha %.9g/%.9g\n",*(float*)(b+0x40a4bdc),cg.tceEyeFlare,*(float*)(raw+0xa90),cent.tceCoronaVisibility,*(float*)(b+0x40a49e4),cg.tceCoronaBlendAlpha); failed++;}
 }printf("corona strict comparison: %d/2016 passed, %d failed\n",2016-failed,failed);return failed?1:0;
}
