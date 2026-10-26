#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static int n,probe,scenario,run,recover;static byte *base,*rawps;static int events[32][20];static char output[256];
static void capture(int id,int arg){int*p=events[n++];p[0]=id;p[1]=arg;p[2]=run?*(int*)(rawps+0x50):pm->ps->groundEntityNum;p[3]=run?*(int*)(rawps+0xc):pm->ps->pm_flags;}
static void capsule(trace_t*t,const vec3_t s,const vec3_t mi,const vec3_t ma,const vec3_t e,int pass,int mask){int*p=events[n++];float norms[]={.69f,.7f,.71f,1.f};p[0]=1;memcpy(p+1,s,12);memcpy(p+4,e,12);memcpy(p+7,mi,12);memcpy(p+10,ma,12);p[13]=pass;p[14]=mask;memset(t,0,sizeof(*t));t->allsolid=probe==0&&(scenario&16);t->fraction=probe++==0?((scenario&1)?1.f:.37f):((scenario&2)?1.f:.51f);t->plane.normal[0]=.13f;t->plane.normal[1]=-.17f;t->plane.normal[2]=norms[(scenario/4)%4];t->entityNum=(scenario&32)?17:1022;t->surfaceFlags=8;VectorCopy(e,t->endpos);}
static int correct(trace_t*t){capture(2,0);t->allsolid=0;if(recover){t->fraction=.37f;if(run)memcpy(base+0x4ba9e74,t,56);else pml.groundTrace=*t;}return recover;}
static void missed(void){capture(3,0);}
static void crash(int steep){capture(4,steep);}
static void touch(int ent){capture(5,ent);}
static int body(playerState_t*p,animModelInfo_t*m,scriptAnimEventTypes_t anim,qboolean c,qboolean f){capture(6,anim);events[n-1][4]=c;events[n-1][5]=f;return 0;}
static int oldbody(void*p,void*m,int anim,int c,int f){return body(p,m,anim==4?ANIM_ET_JUMP:ANIM_ET_JUMPBK,c,f);}
static void print(const char*fmt,...){va_list ap;va_start(ap,fmt);Q_vsnprintf(output+strlen(output),sizeof(output)-strlen(output),fmt,ap);va_end(ap);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte raw[0x700],rpm[0x200],rch[0x900],ext[112];playerState_t ps;pmove_t move;pmoveExt_t pmext;bg_character_t character;void(__cdecl*original)(void);int k,j,count,copy[32][20];char text[256];float speeds[]={-201.f,-200.f,0,9.99f,10.f,80.f,81.f,201.f};
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;rawps=raw;original=(void*)(base+0xe490);if(sizeof(trace_t)!=56)return 3;
 hook(base+0xecb0,correct);hook(PM_CorrectAllSolid,correct);hook(base+0xee40,missed);hook(PM_GroundTraceMissed,missed);hook(base+0xe9c0,crash);hook(PM_CrashLand,crash);hook(base+0x8960,touch);hook(PM_AddTouchEnt,touch);hook(base+0x2520,oldbody);hook(BG_AnimScriptEvent,body);hook(base+0x47c70,print);hook(Com_Printf,print);
 for(k=0;k<32768;k++){
  memset(raw,0,sizeof(raw));memset(rpm,0,sizeof(rpm));memset(rch,0,sizeof(rch));memset(ext,0,sizeof(ext));memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&character,0,sizeof(character));memset(&pmext,0,sizeof(pmext));memset(&pml,0,sizeof(pml));pm=&move;move.ps=&ps;move.pmext=&pmext;move.character=&character;move.trace=capsule;
  *(void**)(base+0x4ba9edc)=rpm;*(void**)rpm=raw;*(void**)(rpm+4)=ext;*(void**)(rpm+8)=rch;*(void**)(rpm+0x124)=capsule;
  for(j=0;j<3;j++){ps.origin[j]=(k%37-18)*.37f+j*.13f;move.mins[j]=-7.3f-j;move.maxs[j]=8.7f+j;}memcpy(raw+0x14,ps.origin,12);memcpy(rpm+0xf4,move.mins,12);memcpy(rpm+0x100,move.maxs,12);
  scenario=k%64;recover=(k/64)%2;move.debugLevel=*(int*)(rpm+0x48)=k&1;c_pmove=*(int*)(base+0x166f80)=k-99;move.tracemask=*(int*)(rpm+0x44)=17;ps.clientNum=*(int*)(raw+0xa0)=k%64;move.cmd.forwardmove=*(signed char*)(rpm+0x20)=(k%3-1)*127;
  ps.groundEntityNum=*(int*)(raw+0x50)=k%3==0?-1:k%3==1?1023:22;ps.pm_flags=*(int*)(raw+0xc)=(k&128)?0x120:8;ps.pm_time=*(int*)(raw+0x10)=77;ps.velocity[2]=*(float*)(raw+0x28)=speeds[(k/128)%8];ps.eFlags=*(int*)(raw+0x68)=((k&1024)?0x80000:0)|((k&2048)?0x8000:0)|((k&4096)?0x20:0)|((k&8192)?0x400000:0);
  ps.stats[STAT_TCE_FLAGS]=*(int*)(raw+0xf4)=(k&256)?4:0;ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(raw+0xf0)=(k&512)?0x1000:0;move.cmd.serverTime=*(int*)(rpm+0xc)=k*17-1000;
  pml.walking=*(int*)(base+0x4ba9e6c)=3;pml.groundPlane=*(int*)(base+0x4ba9e70)=4;pml.tceContentRestriction=*(int*)(base+0x4ba9ed0)=(k/3)%2;pml.tceGroundExtension=*(int*)(base+0x4ba9ed4)=(k/17)%2;pml.previous_velocity[2]=*(float*)(base+0x4ba9ec4)=speeds[k%8];
  run=1;n=probe=0;memset(events,0,sizeof(events));output[0]=0;original();count=n;memcpy(copy,events,sizeof(copy));strcpy(text,output);
  run=0;n=probe=0;memset(events,0,sizeof(events));output[0]=0;PM_GroundTrace();
  if(count!=n||memcmp(copy,events,sizeof(copy))||strcmp(text,output)||memcmp(base+0x4ba9e74,&pml.groundTrace,56)||ps.groundEntityNum!=*(int*)(raw+0x50)||ps.pm_flags!=*(int*)(raw+0xc)||ps.pm_time!=*(int*)(raw+0x10)||ps.jumpTime!=*(int*)(raw+0x47c)||pmext.jumpTime!=*(int*)(ext+4)||pmext.proneTime!=*(int*)(ext+0x34)||ps.stats[STAT_TCE_FLAGS]!=*(int*)(raw+0xf4)||ps.stats[STAT_TCE_WEAPON_FLAGS]!=*(int*)(raw+0xf0)||pml.walking!=*(int*)(base+0x4ba9e6c)||pml.groundPlane!=*(int*)(base+0x4ba9e70)||pml.tceGroundExtension!=*(int*)(base+0x4ba9ed4)){printf("ground controller case%d calls%d/%d ground%d/%d time%d/%d\n",k,count,n,*(int*)(raw+0x50),ps.groundEntityNum,*(int*)(raw+0x10),ps.pm_time);return 1;}
 }puts("32768 whole GroundTrace controller comparisons passed");return 0;}

