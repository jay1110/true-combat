#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void Team_ResetFlag(gentity_t*),Team_ReturnFlag(gentity_t*),Team_DroppedFlagThink(gentity_t*);
static byte*base;static byte itemraw[64];static gitem_t item;static int side,variant,count,bad;
typedef struct{int kind,id;char s[64];}rec;static rec records[16];
static void*entity(int i){return side?(void*)&g_entities[i]:(void*)(base+0x226e2a0+i*0x57c);}
static int indexof(void*p){int i;for(i=30;i<=34;i++)if(entity(i)==p)return i;bad=1;return -1;}
static void note(int k,void*p,const char*s){rec*r;if(count>=16)exit(3);r=&records[count++];memset(r,0,sizeof(*r));r->kind=k;r->id=indexof(p);if(s)Q_strncpyz(r->s,s,sizeof(r->s));}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void respawn(void*p){int i;note(1,p,NULL);i=indexof(p);if(variant&32){if(side)g_entities[i].s.density+=7;else*(int*)((byte*)p+0xf4)+=7;}}
static void freeent(void*p){note(2,p,NULL);if(side)((gentity_t*)p)->inuse=0;else*(int*)((byte*)p+0x194)=0;}
static void script(void*p,const char*s,const char*v){note(3,p,s);note(4,p,v);if(variant&64){if(side)level.gameManager=&g_entities[34];else*(void**)(base+0x221b71c)=base+0x226e2a0+34*0x57c;}}
int main(int argc,char**argv){HMODULE dll;int mode,v,j,n=0;rec expected[16];int ec;unsigned offsets[]={0x8dae0,0x8db30,0x8db40};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;
 if(!hook(base+0x5dbf0,respawn)||!hook(RespawnItem,respawn)||!hook(base+0x94f30,freeent)||!hook(G_FreeEntity,freeent)||!hook(base+0x7d4b0,script)||!hook(G_Script_ScriptEvent,script))return 2;
 for(mode=0;mode<3;mode++)for(v=0;v<4096;v++){
  int depth=v%4;variant=v;memset(&item,0,sizeof(item));memset(itemraw,0,sizeof(itemraw));item.giTag=*(int*)(itemraw+0x28)=(v>>2)%10;
  for(j=30;j<=34;j++){byte*r=base+0x226e2a0+j*0x57c;gentity_t*e=&g_entities[j];memset(r,0,0x57c);memset(e,0,sizeof(*e));e->flags=*(int*)(r+0x1b0)=(j<30+depth)?FL_DROPPED_ITEM:0;e->s.otherEntityNum=*(int*)(r+0x8c)=j==34?34:j+1;e->s.density=*(int*)(r+0xf4)=(v>>4)%5-2;e->inuse=*(int*)(r+0x194)=1;e->item=&item;*(void**)(r+0x33c)=itemraw;}
  level.gameManager=(v&128)?&g_entities[34]:NULL;*(void**)(base+0x221b71c)=(v&128)?base+0x226e2a0+34*0x57c:NULL;
  side=0;count=bad=0;((void(__cdecl*)(void*))(base+offsets[mode]))(entity(30));ec=count;memcpy(expected,records,sizeof(expected));
  side=1;count=0;if(mode==0)Team_ResetFlag(entity(30));else if(mode==1)Team_ReturnFlag(entity(30));else Team_DroppedFlagThink(entity(30));
  if(bad||ec!=count||memcmp(expected,records,count*sizeof(rec))){fprintf(stderr,"flag return mode%d case%d calls%d/%d failed\n",mode,v,count,ec);return 1;}
  for(j=30;j<=34;j++){byte*r=base+0x226e2a0+j*0x57c;gentity_t*e=&g_entities[j];if(e->s.density!=*(int*)(r+0xf4)||e->flags!=*(int*)(r+0x1b0)||e->s.otherEntityNum!=*(int*)(r+0x8c)||e->inuse!=*(int*)(r+0x194)){fprintf(stderr,"flag return mode%d case%d ent%d state failed\n",mode,v,j);return 1;}}n++;
 }
 printf("%d whole flag reset/return/timeout original comparisons passed\n",n);return 0;
}
