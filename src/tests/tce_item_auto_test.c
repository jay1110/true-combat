#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte rawItem[0x57c],rawActor[0x57c],rawClient[0x2000],rawDef[60];
static gentity_t item,actor;static gclient_t client;static gitem_t def;static trace_t tr;
static int originalMode,calls,mutation,bad;
static void touch(void*a,void*b,void*c){++calls;if(c!=&tr)bad=1;if(originalMode){if(a!=rawItem||b!=rawActor||*(int*)(rawItem+0x36c)!=1)bad=1;*(int*)(rawClient+0x61c)=mutation;}else{if(a!=&item||b!=&actor||item.active!=1)bad=1;client.pers.autoActivate=mutation;}}
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*,void*,void*);int mode,type,w,own,active,m,n=0;
 const int mutations[]={-1,0,2,3};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x5dc90);
 if(!hook(b+0x5dd20,touch)||!hook(Touch_Item,touch))return 2;
 item.item=&def;actor.client=&client;*(void**)(rawItem+0x33c)=rawDef;*(void**)(rawActor+0x190)=rawClient;
 for(mode=-1;mode<=3;++mode)for(type=0;type<9;++type)for(w=0;w<64;++w)for(own=0;own<2;++own)for(active=0;active<2;++active)for(m=0;m<4;++m){int expectedCalls;
  def.giType=*(int*)(rawDef+0x24)=type;def.giTag=*(int*)(rawDef+0x28)=w;
  client.ps.weapons[0]=client.ps.weapons[1]=0;if(own)COM_BitSet(client.ps.weapons,w);memcpy(rawClient+0x3d4,client.ps.weapons,8);
  item.active=*(int*)(rawItem+0x36c)=active;client.pers.autoActivate=*(int*)(rawClient+0x61c)=mode;
  mutation=mutations[m];calls=bad=0;originalMode=1;original(rawItem,rawActor,&tr);expectedCalls=calls;if(bad)return 1;
  calls=bad=0;originalMode=0;Touch_Item_Auto(&item,&actor,&tr);
  if(bad||calls!=expectedCalls||item.active!=*(int*)(rawItem+0x36c)||client.pers.autoActivate!=*(int*)(rawClient+0x61c)){fprintf(stderr,"auto pickup mode%d type%d w%d owns%d active%d mutation%d mismatch\n",mode,type,w,own,active,mutation);return 1;}++n;
 }
 printf("%d whole auto-pickup controller comparisons passed\n",n);return 0;
}
