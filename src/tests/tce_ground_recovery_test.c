#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static int n,probe,exitProbe,kind;static float fraction;static int events[64][20];static char output[128];
static void sample(trace_t*t,const float*s,const float*e){int*p=events[n++];p[0]=1;memcpy(p+1,s,12);memcpy(p+4,e,12);memset(t,0,sizeof(*t));t->allsolid=probe++!=exitProbe;t->fraction=fraction;t->entityNum=71;t->surfaceFlags=123;t->plane.normal[2]=.73f;VectorCopy(e,t->endpos);}
static void traceall(trace_t*t,vec3_t s,vec3_t e){sample(t,s,e);}
static void capsule(trace_t*t,const vec3_t s,const vec3_t mi,const vec3_t ma,const vec3_t e,int pass,int mask){sample(t,s,e);memcpy(events[n-1]+7,mi,12);memcpy(events[n-1]+10,ma,12);events[n-1][13]=pass;events[n-1][14]=mask;}
static int body(playerState_t*p,animModelInfo_t*m,scriptAnimEventTypes_t anim,qboolean c,qboolean f){events[n][0]=2;events[n][1]=anim;events[n][2]=c;events[n++][3]=f;return 0;}
static int oldbody(void*p,void*m,int anim,int c,int f){return body(p,m,anim==4?ANIM_ET_JUMP:ANIM_ET_JUMPBK,c,f);}
static void print(const char*fmt,...){va_list ap;va_start(ap,fmt);Q_vsnprintf(output,sizeof(output),fmt,ap);va_end(ap);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b,raw[0x700],rpm[0x200],rch[0x900];playerState_t ps;pmove_t move;bg_character_t character;trace_t tr,originalTrace,ground;int(__cdecl*solid)(trace_t*);void(__cdecl*missed)(void);int k,j,r,s,count,copy[64][20];char text[128];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;solid=(void*)(b+0xecb0);missed=(void*)(b+0xee40);if(sizeof(trace_t)!=56)return 3;
 hook(b+0x8e70,traceall);hook(PM_TraceAll,traceall);hook(b+0x2520,oldbody);hook(BG_AnimScriptEvent,body);hook(b+0x47c70,print);hook(Com_Printf,print);
 for(k=0;k<4096;k++)for(kind=0;kind<2;kind++){
  memset(raw,0,sizeof(raw));memset(rpm,0,sizeof(rpm));memset(rch,0,sizeof(rch));memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&character,0,sizeof(character));pm=&move;move.ps=&ps;move.character=&character;move.trace=capsule;
  *(void**)(b+0x4ba9edc)=rpm;*(void**)rpm=raw;*(void**)(rpm+8)=rch;*(void**)(rpm+0x124)=capsule;
  for(j=0;j<3;j++){ps.origin[j]=(k%37-18)*.37f+j*.13f;move.mins[j]=-7.3f-j;move.maxs[j]=8.7f+j;}memcpy(raw+0x14,ps.origin,12);memcpy(rpm+0xf4,move.mins,12);memcpy(rpm+0x100,move.maxs,12);
  move.debugLevel=*(int*)(rpm+0x48)=k&1;c_pmove=*(int*)(b+0x166f80)=k-99;move.tracemask=*(int*)(rpm+0x44)=17+k%7;ps.clientNum=*(int*)(raw+0xa0)=k%64;move.cmd.forwardmove=*(signed char*)(rpm+0x20)=(k%3-1)*127;
  ps.groundEntityNum=*(int*)(raw+0x50)=k%3==0?-1:k%3==1?1023:22;ps.pm_flags=*(int*)(raw+0xc)=k%16;
  pml.walking=*(int*)(b+0x4ba9e6c)=3;pml.groundPlane=*(int*)(b+0x4ba9e70)=4;memset(&pml.groundTrace,0x55,56);memcpy(b+0x4ba9e74,&pml.groundTrace,56);memset(&tr,0x66,56);originalTrace=tr;exitProbe=k%29-1;fraction=k%4==0?1.f:k%4==1?.99999f:k%4==2?0:.37f;
  n=probe=0;memset(events,0,sizeof(events));output[0]=0;r=0;if(kind)missed();else r=solid(&originalTrace);count=n;memcpy(copy,events,sizeof(copy));strcpy(text,output);memcpy(&ground,b+0x4ba9e74,56);
  n=probe=0;memset(events,0,sizeof(events));output[0]=0;s=0;if(kind)PM_GroundTraceMissed();else s=PM_CorrectAllSolid(&tr);
  if(r!=s||count!=n||memcmp(copy,events,sizeof(copy))||strcmp(text,output)||memcmp(&ground,&pml.groundTrace,56)||memcmp(&tr,&originalTrace,56)||ps.groundEntityNum!=*(int*)(raw+0x50)||ps.pm_flags!=*(int*)(raw+0xc)||pml.walking!=*(int*)(b+0x4ba9e6c)||pml.groundPlane!=*(int*)(b+0x4ba9e70)){printf("ground recovery case%d kind%d result%d/%d calls%d/%d\n",k,kind,r,s,count,n);return 1;}
 }puts("8192 whole ground recovery comparisons passed (4096 each)");return 0;}
