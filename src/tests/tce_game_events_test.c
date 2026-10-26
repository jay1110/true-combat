#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawClient[0x2000],rawTemp[0x57c];static gentity_t ent,temp;static gclient_t client;static int mode,prints,bad,spawnCalls,linkCalls;
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void print(const char*fmt,...){va_list ap;int n;++prints;va_start(ap,fmt);n=va_arg(ap,int);va_end(ap);if(strcmp(fmt,"G_AddEvent: zero event added for entity %i\n")||n!=ent.s.number)bad=1;}
static gentity_t*spawn(void){++spawnCalls;return mode?(void*)rawTemp:&temp;}
static void link(gentity_t*e){++linkCalls;if(e!=(mode?(void*)rawTemp:&temp)||spawnCalls!=1)bad=1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*predict)(void*,int,int);void(__cdecl*add)(void*,int,int);void(__cdecl*sound)(void*,int);int which,hasClient,k,n=0;const int seqs[]={-17,-1,0,1,3,4,7,8,1023,16777217,2147483646};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;predict=(void*)(b+0x950b0);add=(void*)(b+0x950e0);sound=(void*)(b+0x95180);
 if(!hook(b+0x5ed80,print)||!hook(G_Printf,print)||!hook(b+0x94e30,spawn)||!hook(G_Spawn,spawn)||!hook(b+0x8afa0,link)||!hook(trap_LinkEntity,link))return 2;
 for(which=0;which<2;++which)for(hasClient=0;hasClient<2;++hasClient)for(k=0;k<11264;++k){int j,expectedPrint,ev=k%256,parm=k*17-9000;
  memset(raw,0,sizeof(raw));memset(rawClient,0,sizeof(rawClient));memset(&ent,0,sizeof(ent));memset(&client,0,sizeof(client));ent.client=hasClient?&client:NULL;*(void**)(raw+0x190)=hasClient?rawClient:NULL;ent.s.number=*(int*)raw=k%1024;ent.s.eventSequence=*(int*)(raw+0xc0)=seqs[k%11];client.ps.eventSequence=*(int*)(rawClient+0x6c)=seqs[(k+3)%11];
  for(j=0;j<4;++j){ent.s.events[j]=*(int*)(raw+0xc4+j*4)=-j-19;ent.s.eventParms[j]=*(int*)(raw+0xd4+j*4)=j+23;client.ps.events[j]=*(int*)(rawClient+0x70+j*4)=-j-31;client.ps.eventParms[j]=*(int*)(rawClient+0x80+j*4)=j+47;}
  ent.eventTime=*(int*)(raw+0x1c0)=-9;ent.r.eventTime=*(int*)(raw+0x184)=-11;level.time=*(int*)(b+0x7febe0)=k+100000;
  mode=1;bad=prints=0;if(which) add(raw,ev,parm);else predict(raw,ev,parm);expectedPrint=prints;if(bad)return 3;mode=0;bad=prints=0;if(which)G_AddEvent(&ent,ev,parm);else G_AddPredictableEvent(&ent,ev,parm);
  if(bad||prints!=expectedPrint||ent.s.eventSequence!=*(int*)(raw+0xc0)||client.ps.eventSequence!=*(int*)(rawClient+0x6c)||memcmp(ent.s.events,raw+0xc4,16)||memcmp(ent.s.eventParms,raw+0xd4,16)||memcmp(client.ps.events,rawClient+0x70,16)||memcmp(client.ps.eventParms,rawClient+0x80,16)||ent.eventTime!=*(int*)(raw+0x1c0)||ent.r.eventTime!=*(int*)(raw+0x184)){fprintf(stderr,"event controller%d client%d case%d mismatch\n",which,hasClient,k);return 1;}++n;
 }
 printf("%d whole event-controller comparisons passed (22528 each)\n",n);
 for(k=0;k<4096;++k){int index=k-2048;memset(rawTemp,0,sizeof(rawTemp));memset(&temp,0,sizeof(temp));VectorSet(ent.r.currentOrigin,k*.25f-100.125f,-k*.5f+.75f,k%17-8.875f);memcpy(raw+0x168,ent.r.currentOrigin,12);level.time=*(int*)(b+0x7febe0)=k+200000;mode=1;bad=spawnCalls=linkCalls=0;sound(raw,index);if(bad||spawnCalls!=1||linkCalls!=1)return 4;mode=0;bad=spawnCalls=linkCalls=0;G_Sound(&ent,index);
  if(bad||spawnCalls!=1||linkCalls!=1||*(int*)(rawTemp+4)!=67+51||temp.s.eType!=ET_EVENTS+EV_GENERAL_SOUND||temp.s.eventParm!=*(int*)(rawTemp+0xbc)||memcmp(&temp.s.pos,rawTemp+12,sizeof(trajectory_t))||memcmp(temp.r.currentOrigin,rawTemp+0x168,12)||temp.eventTime!=*(int*)(rawTemp+0x1c0)||temp.r.eventTime!=*(int*)(rawTemp+0x184)||temp.freeAfterEvent!=*(int*)(rawTemp+0x1c4)){fprintf(stderr,"sound case%d mismatch\n",k);return 1;}
 }
 puts("4096 whole G_Sound comparisons passed through real TempEntity/SetOrigin");return 0;
}
