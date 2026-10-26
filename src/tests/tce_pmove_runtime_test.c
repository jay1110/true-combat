/* Full native DLL Pmove replay. No movement helper is replaced or patched.
 * Run through tools/runtime_parity/compare_pmove.py: it authenticates the original
 * binaries before these private RVAs are used. Engine callbacks provide only
 * deterministic time, vector snapping and four synthetic collision worlds.
 * Empty animation tables isolate movement; real animation assets require their
 * separate parser/runtime comparisons and an actual in-game smoke test. */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#ifdef CGAMEDLL
#include "../cgame/cg_local.h"
#define TEST_PRINT CG_PRINT
#define TEST_ERROR CG_ERROR
#define TEST_MILLISECONDS CG_MILLISECONDS
#define TEST_SNAPVECTOR CG_SNAPVECTOR
#define TEST_CVAR_STRING CG_CVAR_VARIABLESTRINGBUFFER
#define TEST_MODULE "cgame"
#else
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include "../game/g_public.h"
#define TEST_PRINT G_PRINT
#define TEST_ERROR G_ERROR
#define TEST_MILLISECONDS G_MILLISECONDS
#define TEST_SNAPVECTOR G_SNAPVECTOR
#define TEST_CVAR_STRING G_CVAR_VARIABLE_STRING_BUFFER
#define TEST_MODULE "qagame"
#endif
#include "../game/tce_bg.h"
#ifdef _WIN32
#include <windows.h>
#define TEST_TRY __try
#define TEST_EXCEPT __except(EXCEPTION_EXECUTE_HANDLER)
#else
#include <dlfcn.h>
#include <fenv.h>
#include <signal.h>
#define __cdecl
typedef void *HMODULE;
#define LoadLibraryA(path) dlopen(path,RTLD_NOW|RTLD_LOCAL)
#define GetProcAddress(module,name) dlsym(module,name)
#define ExitProcess(code) exit(code)
#define GetLastError() 0ul
#define TEST_TRY
#define TEST_EXCEPT if(0)
#define GetExceptionCode() 0ul
#endif
#include <float.h>

typedef void (__cdecl *moveFn)(pmove_t *);
typedef void (__cdecl *parseFn)(animModelInfo_t *, animScriptData_t *, const char *, char *);
typedef void (__cdecl *entryFn)(intptr_t (__cdecl *)(intptr_t, ...));
typedef struct { HMODULE dll; byte *base; moveFn move; parseFn parse;
                 tce_weaponDef_t *weapons; vmCvar_t *gametype, *movespeed; int original; } module_t;
typedef struct { playerState_t ps; pmoveExt_t ext; pmove_t move; bg_character_t character;
                 animScriptData_t *scripts; int skills[SK_NUM_SKILLS]; } fixture_t;
static int traceCount, snapCount, unexpectedCalls, nowTime, worldKind;
static unsigned stateMismatches,movementMismatches,extensionMismatches;
static float maxPositionDelta,maxVelocityDelta;
static unsigned byteDifferences[sizeof(playerState_t)];static byte firstA[sizeof(playerState_t)],firstB[sizeof(playerState_t)];
static const char *stage;

static intptr_t __cdecl engineCall(intptr_t op, ...)
{
 va_list ap; intptr_t a; va_start(ap,op);
 switch(op) {
 case TEST_PRINT: a=va_arg(ap,intptr_t); fprintf(stderr,"engine: %s",(char*)a); break;
 case TEST_ERROR: a=va_arg(ap,intptr_t); fprintf(stderr,"ENGINE FATAL at %s: %s\n",stage,(char*)a); ExitProcess(41); break;
 case TEST_MILLISECONDS: va_end(ap); return nowTime;
 case TEST_SNAPVECTOR: {
   float *v=(float*)va_arg(ap,intptr_t); int i; ++snapCount;
   for(i=0;i<3;i++)v[i]=(float)nearbyint((double)v[i]); break;
 }
 case TEST_CVAR_STRING: {
  const char *name=(const char*)va_arg(ap,intptr_t);char *value=(char*)va_arg(ap,intptr_t);int size=va_arg(ap,int);
  (void)name;if(size>0)value[0]=0;break;
 }
 default: fprintf(stderr,"UNEXPECTED syscall %d at %s\n",(int)op,stage); ++unexpectedCalls; break;
 }
 va_end(ap); return 0;
}

/* Deterministic engine-style swept-box trace: floor, steps, wall, jump ledge.
 * These are test collision worlds, not replacements for any movement routine. */
static void traceBox(trace_t *tr,const vec3_t start,const vec3_t mins,const vec3_t maxs,
                     const vec3_t end,const float *bmin,const float *bmax)
{
 float enter=-1e30f,leave=1e30f;int axis,normalAxis=0,inside=1;float sign=0;
 for(axis=0;axis<3;axis++) {
  float lo=bmin[axis]-(maxs?maxs[axis]:0),hi=bmax[axis]-(mins?mins[axis]:0);
  float delta=end[axis]-start[axis],a,b,normal;
  if(start[axis]<=lo+.0001f||start[axis]>=hi-.0001f)inside=0;
  if(delta==0) {if(start[axis]<=lo||start[axis]>=hi)return;continue;}
  a=(lo-start[axis])/delta;b=(hi-start[axis])/delta;normal=-1;
  if(a>b){float temp=a;a=b;b=temp;normal=1;}
  if(a>enter){enter=a;normalAxis=axis;sign=normal;}if(b<leave)leave=b;
  if(enter>leave)return;
 }
 if(inside){tr->startsolid=1;tr->allsolid=leave>1;enter=0;}
 if(enter<0||enter>1||leave<0||enter>=tr->fraction)return;
 tr->fraction=enter;tr->entityNum=ENTITYNUM_WORLD;tr->contents=CONTENTS_SOLID;
 memset(tr->plane.normal,0,sizeof(vec3_t));tr->plane.normal[normalAxis]=sign;
}
static void flatTrace(trace_t *tr,const vec3_t start,const vec3_t mins,const vec3_t maxs,
                      const vec3_t end,int pass,int mask)
{
 float s=start[2]+(mins?mins[2]:0),e=end[2]+(mins?mins[2]:0);int i;
 (void)pass;(void)mask;++traceCount;memset(tr,0,sizeof(*tr));
 tr->fraction=1;tr->entityNum=ENTITYNUM_NONE;tr->plane.normal[2]=1;
 if(e<s&&e<0&&s>=-.001f){tr->fraction=s/(s-e);if(tr->fraction<0)tr->fraction=0;
  tr->entityNum=ENTITYNUM_WORLD;tr->contents=CONTENTS_SOLID;}
 if(s<-.01f){tr->startsolid=1;tr->allsolid=e<0;tr->fraction=0;tr->entityNum=ENTITYNUM_WORLD;}
 if(worldKind==1)for(i=0;i<8;i++){
  float lo[]={160+i*48.0f,-100,-128},hi[]={208+i*48.0f,100,(i+1)*12.0f};traceBox(tr,start,mins,maxs,end,lo,hi);
 }
 if(worldKind==2){float lo[]={256,-256,0},hi[]={280,256,256};traceBox(tr,start,mins,maxs,end,lo,hi);}
 if(worldKind==3){float lo[]={180,-128,0},hi[]={500,128,32};traceBox(tr,start,mins,maxs,end,lo,hi);}
 for(i=0;i<3;i++)tr->endpos[i]=start[i]+tr->fraction*(end[i]-start[i]);
}
static int dryContents(const vec3_t p,int pass) { (void)p;(void)pass;return 0; }

static uintptr_t mapRva(const char *path,const char *name)
{
 FILE *f=fopen(path,"rb"); char line[2048],symbol[256]; unsigned segment,offset;unsigned long long address,preferred=0;
 if(!f) {fprintf(stderr,"No map %s\n",path);ExitProcess(3);}
 while(fgets(line,sizeof(line),f)) {
  if(sscanf(line," Preferred load address is %llx",&address)==1)preferred=address;
  if(sscanf(line," %x:%x %255s %llx",&segment,&offset,symbol,&address)==4 &&
     (!strcmp(symbol,name)||(*name=='_'&&!strcmp(symbol,name+1)))) {
   fclose(f);return address-preferred; }
 }
 fclose(f);fprintf(stderr,"Missing map symbol %s\n",name);ExitProcess(3);return 0;
}
static void openModule(module_t *m,const char *path,const char *map,int original)
{
 entryFn entry; memset(m,0,sizeof(*m));m->original=original;
 m->dll=LoadLibraryA(path);if(!m->dll){fprintf(stderr,"Load failed %s: %lu\n",path,GetLastError());ExitProcess(4);}
 m->base=(byte*)m->dll;entry=(entryFn)GetProcAddress(m->dll,"dllEntry");entry(engineCall);
#ifdef _WIN32
#ifdef CGAMEDLL
 m->move=(moveFn)(m->base+(original?0x13060:mapRva(map,"_Pmove")));
 m->parse=(parseFn)(m->base+(original?0x1670:mapRva(map,"_BG_AnimParseAnimScript")));
 m->weapons=(tce_weaponDef_t*)(m->base+(original?0x4c1df20:mapRva(map,"_weaponDef")));
 m->gametype=(vmCvar_t*)(m->base+(original?0x488c180:mapRva(map,"_cg_gameType")));
 m->movespeed=(vmCvar_t*)(m->base+(original?0x48897e0:mapRva(map,"_cg_movespeed")));
#else
 m->move=(moveFn)(m->base+(original?0x3b3a0:mapRva(map,"_Pmove")));
 m->parse=(parseFn)(m->base+(original?0x29140:mapRva(map,"_BG_AnimParseAnimScript")));
 m->weapons=(tce_weaponDef_t*)(m->base+(original?0x24435c0:mapRva(map,"_weaponDef")));
 m->gametype=(vmCvar_t*)(m->base+(original?0x6d8d40:mapRva(map,"_g_gametype")));
 m->movespeed=(vmCvar_t*)(m->base+(original?0x226e180:mapRva(map,"_g_movespeed")));
#endif
#else
 (void)map;
 m->move=(moveFn)GetProcAddress(m->dll,"Pmove");
 m->parse=(parseFn)GetProcAddress(m->dll,"BG_AnimParseAnimScript");
 m->weapons=(tce_weaponDef_t*)GetProcAddress(m->dll,"weaponDef");
#ifdef CGAMEDLL
 m->gametype=(vmCvar_t*)GetProcAddress(m->dll,"cg_gameType");
 m->movespeed=(vmCvar_t*)GetProcAddress(m->dll,"cg_movespeed");
#else
 m->gametype=(vmCvar_t*)GetProcAddress(m->dll,"g_gametype");
 m->movespeed=(vmCvar_t*)GetProcAddress(m->dll,"g_movespeed");
#endif
 if(!m->move||!m->parse||!m->weapons||!m->gametype||!m->movespeed){fprintf(stderr,"Missing module replay symbol\n");ExitProcess(4);}
#endif
 m->gametype->integer=7;m->movespeed->integer=127;
 memset(m->weapons,0,sizeof(tce_weaponDef_t)*TCE_MAX_WEAPONS);
 m->weapons[1].loadoutWeight=4;
}
static void initFixture(fixture_t *f,module_t *m,int mode,int fixed,float runScale)
{
 char empty[]="";
 memset(f,0,sizeof(*f));f->scripts=(animScriptData_t*)calloc(1,sizeof(animScriptData_t));
 if(!f->scripts)ExitProcess(5);
 f->character.animModelInfo=&f->scripts->modelInfo[0];
 stage=m->original?"original animation initialization":"port animation initialization";
 m->parse(f->character.animModelInfo,f->scripts,"movement_fixture",empty);
 f->ps.commandTime=1000;f->ps.pm_type=mode==4?PM_NOCLIP:PM_NORMAL;
 f->ps.origin[0]=mode==8?80:0;f->ps.origin[2]=24;f->ps.stats[STAT_HEALTH]=100;f->ps.stats[STAT_MAX_HEALTH]=100;
 f->ps.speed=320;f->ps.gravity=800;f->ps.weapon=1;f->ps.weaponstate=WEAPON_READY;
 f->ps.runSpeedScale=runScale;f->ps.sprintSpeedScale=.744f;f->ps.crouchSpeedScale=.213f;
 f->ps.holdable[TCE_FREELOOK_PITCH]=f->ps.holdable[TCE_FREELOOK_YAW]=TCE_FREELOOK_CENTER;
 f->ps.holdable[9]=4;f->ps.groundEntityNum=ENTITYNUM_WORLD;f->ps.viewheight=40;
 f->ps.mins[0]=f->ps.mins[1]=-14;f->ps.mins[2]=-24;
 f->ps.maxs[0]=f->ps.maxs[1]=14;f->ps.maxs[2]=46;
 f->ps.crouchMaxZ=24;f->ps.standViewHeight=40;f->ps.crouchViewHeight=16;f->ps.deadViewHeight=16;
 f->ps.persistant[PERS_TEAM]=TEAM_AXIS;f->ext.sprintTime=20000;
 f->move.ps=&f->ps;f->move.pmext=&f->ext;f->move.character=&f->character;f->move.skill=f->skills;
 f->move.gametype=7;f->move.tracemask=MASK_PLAYERSOLID;f->move.noFootsteps=qtrue;
 f->move.pmove_fixed=fixed;f->move.pmove_msec=8;
 f->move.trace=flatTrace;f->move.pointcontents=dryContents;
}
typedef struct {playerState_t ps;pmoveExt_t ext;} record_t;
/* Sparse integer-word deltas keep the cross-architecture oracle compact while
 * retaining every playerState/pmoveExt byte at every usercmd. No pointers occur
 * in either serialized structure. All supported replay hosts are little-endian. */
static void transferRecords(FILE *file,record_t *records,int count,int writing)
{
 unsigned previous[sizeof(record_t)/4]={0};int frame;unsigned word;
 typedef char recordsHaveWholeWords[(sizeof(record_t)%4==0)?1:-1];
 for(frame=0;frame<count;frame++) {
  unsigned current[sizeof(record_t)/4];unsigned short index;
  if(writing) {
   memcpy(current,&records[frame],sizeof(current));
   for(word=0;word<sizeof(record_t)/4;word++)if(current[word]!=previous[word]) {
    index=(unsigned short)word;
    if(fwrite(&index,2,1,file)!=1||fwrite(&current[word],4,1,file)!=1)ExitProcess(6);
   }
   index=0xffff;if(fwrite(&index,2,1,file)!=1)ExitProcess(6);
   memcpy(previous,current,sizeof(previous));
  } else {
   memcpy(current,previous,sizeof(previous));
   for(;;) {
    if(fread(&index,2,1,file)!=1)ExitProcess(6);
    if(index==0xffff)break;
    if(index>=sizeof(record_t)/4||fread(&current[index],4,1,file)!=1)ExitProcess(6);
   }
   memcpy(previous,current,sizeof(previous));
   memcpy(&records[frame],current,sizeof(current));
  }
 }
}
static void runStream(module_t *m,fixture_t *f,int fps,int mode,FILE *out,int side,record_t *records,float *apex)
{
 int frame,elapsed;traceCount=snapCount=0;*apex=f->ps.origin[2];
 for(frame=1;frame<=fps*5;frame++) {
  elapsed=(frame*1000+fps/2)/fps;nowTime=1000+elapsed;
  f->move.oldcmd=f->move.cmd;memset(&f->move.cmd,0,sizeof(f->move.cmd));
  f->move.cmd.serverTime=nowTime;f->move.cmd.forwardmove=127;f->move.cmd.weapon=1;
  if(mode==1)f->move.cmd.buttons=BUTTON_SPRINT;
  if(mode==2){f->move.cmd.forwardmove=63;f->move.cmd.buttons=BUTTON_WALKING;}
  if(mode==3)f->move.cmd.upmove=-127;
  if(mode==5&&elapsed%1000<100)f->move.cmd.upmove=127;
  if(mode==6){f->move.cmd.forwardmove=0;if(elapsed>=500&&elapsed<600)f->move.cmd.upmove=127;}
  if(mode==7){f->move.cmd.rightmove=127;f->move.cmd.angles[YAW]=16384;}
  if(mode==8&&elapsed>=500&&elapsed<600)f->move.cmd.upmove=127;
  stage=side?"port Pmove":"original Pmove";m->move(&f->move);
  if(f->ps.origin[2]>*apex)*apex=f->ps.origin[2];
  if(!side){records[frame-1].ps=f->ps;records[frame-1].ext=f->ext;}
  else if(memcmp(&records[frame-1].ps,&f->ps,sizeof(f->ps))||memcmp(&records[frame-1].ext,&f->ext,sizeof(f->ext))) {
   if(stateMismatches<12){const byte *a=(const byte*)&records[frame-1].ps,*b=(const byte*)&f->ps;unsigned offset;
    for(offset=0;offset<sizeof(f->ps)&&a[offset]==b[offset];offset++){}
    fprintf(stderr,"state mismatch world%d fps%d mode%d fixed%d frame%d psbyte%u\n",worldKind,fps,mode,f->move.pmove_fixed,frame,offset);}
   {const byte *a=(const byte*)&records[frame-1].ps,*b=(const byte*)&f->ps;unsigned off;
    for(off=0;off<sizeof(f->ps);off++)if(a[off]!=b[off]){if(!byteDifferences[off]){firstA[off]=a[off];firstB[off]=b[off];}++byteDifferences[off];}}
   ++stateMismatches;
  }
  if(side) {
   const playerState_t *p=&records[frame-1].ps;int axis;
   for(axis=0;axis<3;axis++){
    float delta=fabsf(p->origin[axis]-f->ps.origin[axis]);if(delta>maxPositionDelta)maxPositionDelta=delta;
    delta=fabsf(p->velocity[axis]-f->ps.velocity[axis]);if(delta>maxVelocityDelta)maxVelocityDelta=delta;
   }
   if(memcmp(p->origin,f->ps.origin,sizeof(vec3_t))||memcmp(p->velocity,f->ps.velocity,sizeof(vec3_t))||
      p->commandTime!=f->ps.commandTime||p->groundEntityNum!=f->ps.groundEntityNum||p->pm_flags!=f->ps.pm_flags||p->pm_time!=f->ps.pm_time)++movementMismatches;
   if(memcmp(&records[frame-1].ext,&f->ext,sizeof(f->ext)))++extensionMismatches;
  }
  if(frame==1||frame%fps==0)fprintf(out,"sample,%d,%d,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%x,%x,%d,%d,%d\n",
    side,fps,mode,f->move.pmove_fixed,elapsed,f->ps.origin[0],f->ps.origin[1],f->ps.origin[2],
    f->ps.velocity[0],f->ps.velocity[1],f->ps.velocity[2],f->ps.commandTime,f->ps.pm_flags,
    f->ps.stats[STAT_TCE_WEAPON_FLAGS],traceCount,snapCount,worldKind);
 }
}
int main(int argc,char **argv)
{
 module_t original,port;fixture_t a,b;int fpss[]={20,30,60,76,125,250},i,mode,fixed,cases=0,reading=0;FILE *out,*reference=NULL;
 if(argc!=5&&argc!=7)return 2;out=fopen(argv[4],"wb");if(!out)return 3;
 if(argc==7){unsigned header[4]={0x504d5631,sizeof(playerState_t),sizeof(pmoveExt_t),432},actual[4];
  reading=!strcmp(argv[6],"read");reference=fopen(argv[5],reading?"rb":"wb");if(!reference)return 3;
  if(reading){if(fread(actual,sizeof(actual),1,reference)!=1||memcmp(header,actual,sizeof(header)))return 7;}
  else if(fwrite(header,sizeof(header),1,reference)!=1)return 7;
 }
 fprintf(out,"layout,%s,ps,%zu,ext,%zu,pmove,%zu,viewangles,%zu,weapon,%zu,fixed,%zu,trace,%zu,animdata,%zu\n",
   TEST_MODULE,sizeof(playerState_t),sizeof(pmoveExt_t),sizeof(pmove_t),offsetof(playerState_t,viewangles),
   offsetof(playerState_t,weapon),offsetof(pmove_t,pmove_fixed),offsetof(pmove_t,trace),sizeof(animScriptData_t));fflush(out);
#ifdef _WIN32
 _controlfp(_RC_NEAR,_MCW_RC);
#else
 fesetround(FE_TONEAREST);
#endif
 TEST_TRY {
  if(!reading){if(sizeof(void*)!=4)return 8;openModule(&original,argv[1],argv[3],1);}
  openModule(&port,argv[2],argv[3],0);
  for(worldKind=0;worldKind<4;worldKind++)for(fixed=0;fixed<2;fixed++)for(mode=0;mode<9;mode++)for(i=0;i<6;i++) {
   record_t *records=(record_t*)calloc(fpss[i]*5,sizeof(record_t));float origApex,portApex;
   if(!records)ExitProcess(5);
   if(!reading){initFixture(&a,&original,mode,fixed,.45f);runStream(&original,&a,fpss[i],mode,out,0,records,&origApex);
    if(reference)transferRecords(reference,records,fpss[i]*5,1);}
   else {int frame;transferRecords(reference,records,fpss[i]*5,0);memset(&a,0,sizeof(a));
    a.ps=records[fpss[i]*5-1].ps;a.ext=records[fpss[i]*5-1].ext;origApex=24;
    for(frame=0;frame<fpss[i]*5;frame++)if(records[frame].ps.origin[2]>origApex)origApex=records[frame].ps.origin[2];}
   initFixture(&b,&port,mode,fixed,.45f);runStream(&port,&b,fpss[i],mode,out,1,records,&portApex);
   fprintf(out,"result,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%d,%d,%d,%.9g,%.9g\n",fpss[i],mode,fixed,
     a.ps.origin[0],b.ps.origin[0],a.ps.velocity[0],b.ps.velocity[0],a.ps.commandTime,b.ps.commandTime,worldKind,origApex,portApex);
   fflush(out);if(!reading)free(a.scripts);free(b.scripts);free(records);++cases;
  }
 } TEST_EXCEPT {fprintf(stderr,"FAULT %08lx at %s\n",GetExceptionCode(),stage);fclose(out);return 10;}
 {unsigned off;for(off=0;off<sizeof(playerState_t);off++)if(byteDifferences[off])fprintf(out,"byte_mismatch,%u,%u,%u,%u\n",off,byteDifferences[off],firstA[off],firstB[off]);}
 fprintf(out,"errorbound,%.9g,%.9g\n",maxPositionDelta,maxVelocityDelta);
 fprintf(out,"summary,%d,%u,%d,%u,%u\n",cases,stateMismatches,unexpectedCalls,movementMismatches,extensionMismatches);fclose(out);if(reference)fclose(reference);
 printf("%s Pmove: %d streams, %u differing frame states, %d unexpected syscalls\n",TEST_MODULE,cases,stateMismatches,unexpectedCalls);
 return unexpectedCalls?20:stateMismatches?21:0;
}
