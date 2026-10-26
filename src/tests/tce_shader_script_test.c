#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean G_ScriptAction_ShaderRemap(gentity_t*,char*);
extern qboolean G_ScriptAction_ShaderRemapFlush(gentity_t*,char*);
static jmp_buf jump;static int count;static char state[1024];
typedef struct {int kind,number;float time;char a[1024],b[1024];} event_t;static event_t events[4];
static event_t*event(int kind){event_t*e;if(count>=4)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void remap(const char*a,const char*b,float time){event_t*e=event(1);e->time=time;Q_strncpyz(e->a,a,sizeof(e->a));Q_strncpyz(e->b,b,sizeof(e->b));}
static const char*build(void){event(2);return state;}
static void config(int n,const char*s){event_t*e=event(3);e->number=n;Q_strncpyz(e->a,s,sizeof(e->a));if(s!=state)exit(3);}
static void fatal(const char*s){event_t*e=event(4);Q_strncpyz(e->a,s,sizeof(e->a));longjmp(jump,1);}
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*fn[2])(void*,char*);int mode,sc,ix,j,n=0;char longInput[700];
 const char*inputs[]={NULL,""," ","old","old new","old\nnew","\"old a\" \"new b\"","/*c*/ old new","old //c\nnew","old new trailing","\"\" new",longInput};
 int times[]={-2147483647,-16777217,-1001,-1,0,1,999,1000,1001,16777215,16777216,16777217,2147483647};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;fn[0]=(void*)(b+0x7e410);fn[1]=(void*)(b+0x7e4d0);
 if(!hook(b+0x94560,remap)||!hook(AddRemap,remap)||!hook(b+0x94660,build)||!hook(BuildShaderStateConfig,build)||!hook(b+0x8adc0,config)||!hook(trap_SetConfigstring,config)||!hook(b+0x8ab30,fatal)||!hook(trap_Error,fatal))return 2;
 memset(longInput,'a',300);longInput[300]=' ';memset(longInput+301,'b',300);longInput[601]=0;
 for(mode=0;mode<2;mode++)for(sc=0;sc<260;sc++)for(ix=0;ix<12;ix++){
  event_t expected[4];int expectedCount,x=-1,y=-1;char a[1024],c[1024];
  level.time=*(int*)(b+0x7febe0)=times[sc%13];for(j=0;j<sc%900;j++)state[j]='A'+j%26;state[j]=0;
  if(inputs[ix]){Q_strncpyz(a,inputs[ix],sizeof(a));Q_strncpyz(c,inputs[ix],sizeof(c));}
  count=0;if(!setjmp(jump))x=fn[mode](NULL,inputs[ix]?a:NULL);expectedCount=count;memcpy(expected,events,sizeof(expected));
  count=0;if(!setjmp(jump))y=mode?G_ScriptAction_ShaderRemapFlush(NULL,inputs[ix]?c:NULL):G_ScriptAction_ShaderRemap(NULL,inputs[ix]?c:NULL);
  if(x!=y||count!=expectedCount||memcmp(expected,events,count*sizeof(event_t))){fprintf(stderr,"shader script mode%d scene%d input%d time%d events%d/%d failed\n",mode,sc,ix,level.time,count,expectedCount);return 1;}n++;
 }
 printf("%d whole shader-script comparisons passed (3120 each, exact float/fatal included)\n",n);return 0;
}
