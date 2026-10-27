#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
typedef struct {void*p;unsigned offset;} field_t;
static field_t fields[]={{&cgs.complaintClient,0x3fb781c},{&cgs.complaintEndTime,0x3fb7820},{&cg.numbufferedSoundScripts,0x40a28ac},{&cg.centerPrintTime,0x4080948},{&cg.itemPickupTime,0x4080e3c},{&cg.cursorHintFade,0x4080dd4},{&cg.zoomedBinoc,0x407f1c0},{&cg.zoomedScope,0x407f1c4},{&cg.zoomTime,0x407f1c8},{&cg.zoomval,0x407f1d0},{&cg.tceScopeBlocked,0x40a4b0c},{&cg.tceAimRequested,0x40a4974},{&cg.tceAimComplete,0x40a4970},{&cg.tceAimActive,0x40a496c},{&cg.tceTacticalScale,0x4846e6c},{&cg.tceAimWeaponLatch,0x40a49fc},{&cg.tceCoronaBlendAlpha,0x40a49e4},{&cg.tceEyeSmooth,0x40a4be8},{&cg.tceEyeLinear,0x40a4bec},{&cg.tceScopeLightBoost,0x40a4c08},{&cg.tceEyeFlare,0x40a4bdc},{&cg.tceEyeSampleTime,0x40a4bd0},{&cg.tceEyeFlareFrame,0x40a4c1c},{&cgs.invitationEndTime,0x3fc4290},{&cgs.applicationEndTime,0x3fc4298},{&cgs.propositionEndTime,0x3fc42a4},{&cgs.autoFireteamEndTime,0x3fc42a8},{&cgs.autoFireteamCreateEndTime,0x3fc42b0},{&numSplinePaths,0x4bab708},{&numPathCorners,0x4c2ea20},{&cg.numOIDtriggers2,0x4087f58},{&cgs.fadeStartTime,0x2587ffc},{&cgs.fadeAlpha,0x2587ff4},{&cg.editingSpeakers,0x40a2444},{&cg.intermissionStarted,0x40352b8},{&cg.lightstylesInited,0x4080944},{&cg.mapRestart,0x407deb4},{&cg.timelimitWarnings,0x407deb0},{&cgs.voteTime,0x257e214},{&cgs.dumpStatsTime,0x3fc42f0},{&cg.latchAutoActions,0x4081570},{&cg.latchVictorySound,0x4081574},{&cg.v_dmg_time,0x4080f6c},{&cg.v_noFireTime,0x4080f94},{&cg.v_fireTime,0x4080f90},{&cg.filtercams,0x4080dbc}};
static byte*base;static int originalRun,count;static const char*config;
typedef struct{int id,args[8];int state[46];int banks[10];byte pmext[112];byte fx;char text[96];} event_t;
static event_t events[48];
static event_t*record(int id){int i;event_t*e=&events[count++];memset(e,0,sizeof(*e));e->id=id;for(i=0;i<46;i++)memcpy(&e->state[i],originalRun?base+fields[i].offset:fields[i].p,4);memcpy(e->banks,originalRun?base+0x4080f28:(byte*)cg.lastWeapSelInBank,40);memcpy(e->pmext,originalRun?base+0x4087ee8:(byte*)&cg.pmext,112);e->fx=originalRun?*(base+0x18531a0):cg_fxflags;return e;}
static void request(void){record(1);}
static void wolf(void){record(2);}
static void entities(void){record(3);}
static void objectives(void){record(4);}
static void localents(void){record(5);}
static void marks(void){record(6);}
static void splines(void){record(7);}
static void smoke(void){record(8);}
static void particles(void){record(9);}
static void flames(void){record(10);}
static void sound(void){record(11);}
static void music(void){record(12);}
static void loops(void){record(13);}
static void decals(void){record(14);}
static void charge(void){record(15);}
static void fireteams(void){record(16);}
static void oid(void){record(17);}
static void popups(void){record(18);}
static void spawns(void){record(19);}
static void tags(void){record(20);}
static void light(void){record(21);}
static void print(const char*s,...){Q_strncpyz(record(30)->text,s,96);}
static void fog(int a,int b,int c,float d,float e,float f,float g){event_t*v=record(31);v->args[0]=a;v->args[1]=b;v->args[2]=c;memcpy(v->args+3,&d,4);memcpy(v->args+4,&e,4);memcpy(v->args+5,&f,4);memcpy(v->args+6,&g,4);}
static void cvar(const char*k,const char*v){event_t*e=record(32);Q_strncpyz(e->text,k,96);e->args[0]=atoi(v);}
static void clears(int n){record(33)->args[0]=n;}
static const char* cfg(int n){record(34)->args[0]=n;return config;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
#define H(a,n,f) hook(base+a,f);hook(n,f)
int main(int argc,char**argv){HMODULE m;void(__cdecl*original)(void);int k,i,pass,n,accepted;static event_t expected[48];static const char*values[]={"","0","1","-1","12junk","  2","bad"};accepted=argc==3&&!strcmp(argv[2],"--accepted-extensions");if((argc!=2&&!accepted)||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;original=(void*)(base+0x5fa20);
H(0x43b40,CG_LimboPanel_RequestObjective,request);
H(0x5b6a0,CG_ParseWolfinfo,wolf);
H(0x64ac0,CG_ParseEntitiesFromString,entities);
H(0x47d70,CG_LoadObjectiveData,objectives);
H(0x44860,CG_InitLocalEntities,localents);
H(0x4b8c0,CG_InitMarkPolys,marks);
H(0x5b20,BG_BuildSplinePaths,splines);
H(0x2dbd0,InitSmokeSprites,smoke);
H(0x4f4c0,CG_ClearParticles,particles);
H(0x3d530,CG_ClearFlameChunks,flames);
H(0x60a20,CG_SoundInit,sound);
H(0x48200,CG_StartMusic,music);
H(0x65090,trap_S_ClearLoopingSounds,loops);
H(0x64fa0,trap_R_ClearDecals,decals);
H(0x5bf10,CG_ChargeTimesChanged,charge);
H(0x3b490,CG_ParseFireteams,fireteams);
H(0x5b680,CG_ParseOIDInfos,oid);
H(0x575b0,CG_InitPM,popups);
H(0x5b870,CG_ParseSpawns,spawns);
H(0x6b1c0,CG_ParseTagConnects,tags);
H(0x48990,CG_SetupEliteLighting,light);
H(0x47b10,CG_Printf,print);H(0x65380,trap_R_SetFog,fog);H(0x64c10,trap_Cvar_Set,cvar);H(0x650a0,trap_S_ClearSounds,clears);H(0x481a0,CG_ConfigString,cfg);
if(sizeof(cg.lastWeapSelInBank)!=40){puts("restart weapon-bank ABI mismatch");return 2;}
if(!accepted&&sizeof(cg.pmext)!=112){printf("STRICT_BASELINE_DIFFERENCE CG_MapRestart pmext=%u original=112; freelook tail requires separate accepted-extension test\n",(unsigned)sizeof(cg.pmext));return 1;}
if(accepted&&(sizeof(cg.pmext)!=128||offsetof(pmoveExt_t,tceFreelookRecoil)!=112||sizeof(cg.pmext.tceFreelookRecoil)!=8||offsetof(pmoveExt_t,tceReloadWeapon)!=120||offsetof(pmoveExt_t,tceReloadRequestedAt)!=124)){puts("Accepted mod extension layout contract changed");return 2;}
for(k=0;k<1120;k++){config=values[k%7];for(pass=0;pass<2;pass++){originalRun=!pass;count=0;for(i=0;i<46;i++){int seed=0x3e800000+k*17+i;memcpy(fields[i].p,&seed,4);memcpy(base+fields[i].offset,&seed,4);}
memset(&cg.pmext,0x55,sizeof(cg.pmext));memset(base+0x4087ee8,0x55,112);memset(cg.lastWeapSelInBank,0x33,40);memset(base+0x4080f28,0x33,40);cg_fxflags=*(base+0x18531a0)=71;cg_showmiss.integer=*(int*)(base+0x400faac)=(k/7)%3-1;cg_autoReload.integer=*(int*)(base+0x40143cc)=(k/21)%5-2;
if(originalRun)original();else CG_MapRestart();
if(!originalRun&&accepted){const byte zero[8]={0};if(memcmp(cg.pmext.tceFreelookRecoil,zero,8)||cg.pmext.tceReloadWeapon||cg.pmext.tceReloadRequestedAt){printf("Freelook/reload suffix not reset case%d\n",k);return 1;}}
record(40);if(!pass){n=count;memcpy(expected,events,sizeof(expected));}else if(n!=count||memcmp(expected,events,n*sizeof(event_t))){for(i=0;i<n&&i<count;i++)if(memcmp(&expected[i],&events[i],sizeof(event_t)))break;printf("maprestart case%d event%d id%d/%d\n",k,i,expected[i].id,events[i].id);return 1;}}}
puts(accepted?"ACCEPTED_EXTENSION CG_MapRestart 1120 cases: original112-byte prefix+ordered calls equal; separate16-byte freelook/reload suffix zeroed":"1120 whole CG_MapRestart state and ordered-call comparisons passed");return 0;}

