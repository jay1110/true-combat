#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static byte raw[0x57c],clients[4][0x16b0],*base;static gentity_t ent;static gclient_t nc[4];static int side,scene,count,bad;
typedef struct{int kind,a,b;char text[1024],arg[128];}event_t;static event_t events[16];
static event_t*event(int k,int a,int b,const char*s){event_t*e;if(count>=16)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=k;e->a=a;e->b=b;if(s)Q_strncpyz(e->text,s,sizeof(e->text));return e;}
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static int countTeam(int ignore,int team){event(1,ignore,team,NULL);if(ignore!=-1||team<1||team>2)bad=1;return (scene>>(team-1))&1;}
static void checkEnt(void*p){if(p!=(scene&4?(side?(void*)&ent:(void*)raw):NULL))bad=1;}
static void printFull(char*s,void*p){checkEnt(p);event(2,0,0,s);}
static void refPrint(void*p,const char*fmt,...){event_t*e;va_list ap;checkEnt(p);e=event(3,0,0,fmt);if(strstr(fmt,"%s")){va_start(ap,fmt);Q_strncpyz(e->arg,va_arg(ap,char*),sizeof(e->arg));va_end(ap);}}
static void config(int n,const char*s){event(4,n,0,s);}
static void specLock(int team,int lock){event(5,team,lock,NULL);if(side)teamInfo[team].spec_lock=lock;else *(int*)(base+0x66fde0+team*40)=lock;}
static int ready(void){int i;event(6,side?level.ref_allready:*(int*)(base+0x221bb00),0,NULL);for(i=0;i<4;i++)event(7,i,side?nc[i].pers.ready:*(int*)(clients[i]+0xafc),NULL);return scene&1;}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*fn[2])(void*,int);void(__cdecl*allready)(void*);int mode,sc,lock,i,j,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;fn[0]=(void*)(base+0x7bbe0);fn[1]=(void*)(base+0x7c090);allready=(void*)(base+0x7bb60);
 if(!hook(base+0x4b8a0,countTeam)||!hook(TeamCount,countTeam)||!hook(base+0x65120,printFull)||!hook(G_printFull,printFull)||!hook(base+0x7c730,refPrint)||!hook(G_refPrintf,refPrint)||!hook(base+0x8adc0,config)||!hook(trap_SetConfigstring,config)||!hook(base+0x8fc40,specLock)||!hook(G_updateSpecLock,specLock)||!hook(base+0x8f9a0,ready)||!hook(G_readyMatchState,ready))return 2;
 for(mode=0;mode<3;mode++)for(lock=0;lock<3;lock++)for(sc=0;sc<2048;sc++){
  event_t expected[16];int expectedCount;void*p;gentity_t*np;scene=sc;p=sc&4?raw:NULL;np=sc&4?&ent:NULL;
  for(j=0;j<160;j++)((byte*)teamInfo)[j]=base[0x66fde0+j]=(byte)(sc+j*17);
  level.server_settings=*(int*)(base+0x221bb04)=sc*31;level.ref_allready=*(int*)(base+0x221bb00)=sc%3-1;g_gamestate.integer=*(int*)(base+0x784f6c)=(sc>>3)&3;
  level.clients=nc;*(void**)(base+0x7feb80)=clients;level.numConnectedClients=*(int*)(base+0x7fec0c)=(sc&32)?0:4;
  for(i=0;i<4;i++){level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=(i+sc)%4;nc[i].pers.ready=*(int*)(clients[i]+0xafc)=(sc+i)%3-1;nc[i].sess.sessionTeam=*(int*)(clients[i]+0xb30)=(sc+i)&3;}
  side=0;bad=count=0;if(mode<2)fn[mode](p,lock);else allready(p);if(bad)return 3;expectedCount=count;memcpy(expected,events,sizeof(expected));
  side=1;bad=count=0;if(mode==0)G_refLockTeams_cmd(np,lock);else if(mode==1)G_refSpeclockTeams_cmd(np,lock);else G_refAllReady_cmd(np);
  if(bad||count!=expectedCount||memcmp(expected,events,count*sizeof(event_t))||memcmp(teamInfo,base+0x66fde0,160)||level.server_settings!=*(int*)(base+0x221bb04)||level.ref_allready!=*(int*)(base+0x221bb00)){fprintf(stderr,"ref team mode%d scene%d lock%d events%d/%d failed\n",mode,sc,lock,count,expectedCount);return 1;}
  for(i=0;i<4;i++)if(nc[i].pers.ready!=*(int*)(clients[i]+0xafc)){fprintf(stderr,"ref ready scene%d client%d failed\n",sc,i);return 1;}n++;
 }
 printf("%d whole referee team-controller comparisons passed (6144 each)\n",n);return 0;
}
