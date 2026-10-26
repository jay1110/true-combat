#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static byte raw[0x57c],clients[8][0x16b0];static gentity_t ent;static gclient_t nc[8];static int side,scene,count,bad;static const char*argument,*cvar;
typedef struct{int kind,a;char x[1024],y[1024];}event_t;static event_t events[8];
static event_t*event(int k,int a,const char*s){event_t*e;if(count>=8)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=k;e->a=a;if(s)Q_strncpyz(e->x,s,sizeof(e->x));return e;}
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void argvFn(int n,char*out,int size){event(1,n,NULL);if(n!=2||size!=1024)bad=1;Q_strncpyz(out,argument,size);}
static void get(const char*name,char*out,int size){event(2,size,name);Q_strncpyz(out,cvar,size);}
static void set(const char*name,const char*value){event_t*e=event(3,0,name);Q_strncpyz(e->y,value,sizeof(e->y));}
static void ref(void*p,const char*fmt,...){event_t*e=event(4,0,fmt);va_list ap;if(p!=(scene&1?(side?(void*)&ent:(void*)raw):NULL))bad=1;if(strstr(fmt,"%d")){va_start(ap,fmt);e->a=va_arg(ap,int);va_end(ap);}}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*warmup)(void*);int(__cdecl*find)(void*,const char*);int sc,ai,i,n=0;
 const char*values[]={"","0","-1","-99","+1","000","23","23junk","junk"," 9","2147483647","-2147483647"};
 const char*names[]={"Alpha","^1Alpha","ALPHA","^2Beta","Beta","gamma","^7","","A\x01lpha","^xEcho","alpha","Missing"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;warmup=(void*)(b+0x7c120);find=(void*)(b+0x7c680);
 if(!hook(b+0x8ab70,argvFn)||!hook(trap_Argv,argvFn)||!hook(b+0x8acd0,get)||!hook(trap_Cvar_VariableStringBuffer,get)||!hook(b+0x8ac90,set)||!hook(trap_Cvar_Set,set)||!hook(b+0x7c730,ref)||!hook(G_refPrintf,ref))return 2;
 for(sc=0;sc<256;sc++)for(ai=0;ai<12;ai++){
  event_t expected[8];int ec;scene=sc;argument=values[ai];cvar=values[sc%12];side=0;bad=count=0;warmup(sc&1?raw:NULL);if(bad)return 3;ec=count;memcpy(expected,events,sizeof(expected));side=1;bad=count=0;G_refWarmup_cmd(sc&1?&ent:NULL);
  if(bad||ec!=count||memcmp(expected,events,count*sizeof(event_t))){fprintf(stderr,"ref warmup scene%d arg%d failed\n",sc,ai);return 1;}n++;
 }
 for(sc=0;sc<1024;sc++)for(ai=0;ai<12;ai++){
  event_t expected[8];int ec,x,y;scene=sc;level.clients=nc;*(void**)(b+0x7feb80)=clients;level.numConnectedClients=*(int*)(b+0x7fec0c)=sc%9;
  for(i=0;i<8;i++){level.sortedClients[i]=*(int*)(b+0x7fec18+i*4)=(i+sc)%8;Q_strncpyz(nc[i].pers.netname,names[(i+sc)%12],sizeof(nc[i].pers.netname));Q_strncpyz((char*)clients[i]+0x5f8,names[(i+sc)%12],sizeof(nc[i].pers.netname));}
  side=0;bad=count=0;x=find(sc&1?raw:NULL,names[ai]);if(bad)return 3;ec=count;memcpy(expected,events,sizeof(expected));side=1;bad=count=0;y=G_refClientnumForName(sc&1?&ent:NULL,names[ai]);
  if(bad||x!=y||ec!=count||memcmp(expected,events,count*sizeof(event_t))){fprintf(stderr,"ref name scene%d name%d result%d/%d failed\n",sc,ai,y,x);return 1;}n++;
 }
 printf("%d whole referee query comparisons passed (3072 warmup,12288 name)\n",n);return 0;
}
