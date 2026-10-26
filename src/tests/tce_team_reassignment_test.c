#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>
extern int QDECL G_SortPlayersByXP(const void*,const void*);
static byte*base;static byte originalClients[64][0x16b0];static gclient_t clients[64];static int side,count,bad;
typedef struct{int k,a,b,c,team;char text[64];}event;
static event calls[1024];
typedef struct{int count,teams[64];event calls[1024];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int team(int n){return side?clients[n].sess.sessionTeam:*(int*)(originalClients[n]+0xb30);}
static int entityIndex(void*p){return side?(gentity_t*)p-g_entities:((byte*)p-base-0x226e2a0)/0x57c;}
static void note(int k,int a,int b,int c,const char*s){event*e;if(count>=1024)exit(3);e=&calls[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;e->b=b;e->c=c;e->team=a>=0&&a<64?team(a):-1;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static void reset(int t,int full){note(1,t,full,0,NULL);}
static void character(void*p){int n=side?(gclient_t*)p-clients:((byte*)p-(byte*)originalClients)/0x16b0;note(2,n,0,0,NULL);}
static void userinfo(int n){note(3,n,0,0,NULL);}
static void begin(int n){note(4,n,0,0,NULL);}
static void tank(void*p,int v){note(5,entityIndex(p),v,0,NULL);}
static void fireteam(int n,int a,int b){note(6,n,a,b,NULL);}
static void mines(void*p){note(7,entityIndex(p),0,0,NULL);}
static void fade(void*p,int mod){note(8,entityIndex(p),mod,0,NULL);}
static void send(int n,const char*s){note(9,n,0,0,s);}
/* Library-boundary phase: same stable qsort substitute invokes the real comparator
 * on both sides. The initial phase also exercises each actual CRT on unique XP. */
static void sortBoundary(void*p,size_t n,size_t size,int(__cdecl*cmp)(const void*,const void*)){int*i=p,j,k,v;note(10,(int)n,(int)size,0,NULL);if(size!=4||n>64||cmp!=(side?G_SortPlayersByXP:(void*)(base+0x8f690))){bad=1;return;}for(k=1;k<(int)n;k++){v=i[k];j=k;while(j>0&&cmp(&v,&i[j-1])<0){i[j]=i[j-1];j--;}i[j]=v;}}
static void capture(result*r){int i;memset(r,0,sizeof(*r));r->count=count;memcpy(r->calls,calls,sizeof(calls));for(i=0;i<64;i++)r->teams[i]=team(i);}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*swap)(void),(__cdecl*shuffle)(void);int(__cdecl*compare)(const void*,const void*);int phase,v,i,j,n,total=0,comparisons=0;static result a,b;int scores[]={INT_MIN,-16777217,-1,0,1,16777217,INT_MAX};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;swap=(void*)(base+0x8f5f0);shuffle=(void*)(base+0x8f6f0);compare=(void*)(base+0x8f690);level.clients=clients;*(void**)(base+0x7feb80)=originalClients;
 for(i=0;i<7;i++)for(j=0;j<7;j++){int x=3,y=57;clients[x].ps.persistant[PERS_SCORE]=*(int*)(originalClients[x]+0x110)=scores[i];clients[y].ps.persistant[PERS_SCORE]=*(int*)(originalClients[y]+0x110)=scores[j];if(compare(&x,&y)!=G_SortPlayersByXP(&x,&y)){printf("XP comparator mismatch %d/%d\n",i,j);return 1;}comparisons++;}
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x8f590,G_teamReset,reset);H(0x4a870,G_UpdateCharacter,character);H(0x4bf70,ClientUserinfoChanged,userinfo);H(0x4ca70,ClientBegin,begin);H(0x535f0,G_LeaveTank,tank);H(0x5c230,G_RemoveClientFromFireteams,fireteam);H(0x6c580,G_ExplodeMines,mines);H(0x6c370,G_FadeItems,fade);H(0x8ad60,trap_SendServerCommand,send);
 for(phase=0;phase<3;phase++){
  if(phase==2){H(0xa2407,qsort,sortBoundary);}
  for(v=0;v<4096;v++)for(side=0;side<2;side++){
   n=v%65;count=bad=0;memset(calls,0,sizeof(calls));memset(clients,0,sizeof(clients));memset(originalClients,0,sizeof(originalClients));level.numConnectedClients=*(int*)(base+0x7fec0c)=n;g_landminetimeout.integer=*(int*)(base+0x6d856c)=(v/65)%3-1;
   for(i=0;i<64;i++){int t=(i+(v/195))%4,score=phase==2?scores[(i+v)%7]:((i*37+v)%64)*100-3200;level.sortedClients[i]=*(int*)(base+0x7fec18+4*i)=(i*17+v)%64;clients[i].sess.sessionTeam=*(int*)(originalClients[i]+0xb30)=t;clients[i].ps.persistant[PERS_SCORE]=*(int*)(originalClients[i]+0x110)=score;}
   if(!phase){if(side)G_swapTeams();else swap();}else{if(side)G_shuffleTeams();else shuffle();}capture(side?&b:&a);
   if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Team reassignment mismatch phase%d case%d bad%d calls%d/%d\n",phase,v,bad,a.count,b.count);for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.calls[i],&b.calls[i],sizeof(event)))printf("call%d kind%d/%d args%d,%d,%d/%d,%d,%d team%d/%d text[%s]/[%s]\n",i,a.calls[i].k,b.calls[i].k,a.calls[i].a,a.calls[i].b,a.calls[i].c,b.calls[i].a,b.calls[i].b,b.calls[i].c,a.calls[i].team,b.calls[i].team,a.calls[i].text,b.calls[i].text);return 1;}total++;}
  }
 }
 printf("PASS whole team reassignment %d cases plus %d XP comparator pairs; actual CRT unique-XP and controlled tie-order phases\n",total,comparisons);return 0;
}
