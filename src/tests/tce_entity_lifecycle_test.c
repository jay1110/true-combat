#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte*raw;static gentity_t*ent;static int mode,bad,calls[8],count,freeMutation,unlinkMutation;static char initial[]="before";
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void*active(void){return mode?(void*)raw:(void*)ent;}
static void mutate(int m){if(m>=0){if(mode){*(int*)(raw+0x1ac)=m;*(int*)(raw+0x4c0)+=13;}else{ent->neverFree=m;ent->spawnCount+=13;}}}
static void freeCallback(gentity_t*e){calls[count++]=1;if(e!=active())bad=1;mutate(freeMutation);}
static void unlinkCallback(gentity_t*e){calls[count++]=2;if(e!=active())bad=1;mutate(unlinkMutation);}
static gentity_t*spawnCallback(void){calls[count++]=3;return active();}
static void linkCallback(gentity_t*e){calls[count++]=4;if(e!=active())bad=1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*init)(void*);void(__cdecl*release)(void*);void*(__cdecl*temporary)(vec3_t,int);int k,n;static gentity_t expected;static byte rawExpected[0x57c];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;init=(void*)(b+0x94da0);release=(void*)(b+0x94f30);temporary=(void*)(b+0x94fa0);
 if(!hook(b+0x8afc0,unlinkCallback)||!hook(trap_UnlinkEntity,unlinkCallback)||!hook(b+0x94e30,spawnCallback)||!hook(G_Spawn,spawnCallback)||!hook(b+0x8afa0,linkCallback)||!hook(trap_LinkEntity,linkCallback))return 2;
 for(k=0;k<1024;++k){raw=b+0x226e2a0+k*0x57c;ent=&g_entities[k];memset(raw,0x25,0x57c);memset(ent,0x25,sizeof(*ent));expected=*ent;ent->spawnCount=*(int*)(raw+0x4c0)=k-512;expected.spawnCount=ent->spawnCount;level.time=*(int*)(b+0x7febe0)=100000+k;init(raw);G_InitGentity(ent);
  expected.inuse=*(int*)(raw+0x194);expected.classname=ent->classname;expected.s.number=*(int*)raw;expected.r.ownerNum=*(int*)(raw+0x180);expected.aiInactive=*(int*)(raw+0x4e8);expected.nextthink=*(int*)(raw+0x29c);memcpy(expected.goalPriority,raw+0x4ec,sizeof(expected.goalPriority));expected.free=*(void**)(raw+0x2a0);expected.scriptStatus.scriptEventIndex=*(int*)(raw+0x3c8);expected.spawnCount=*(int*)(raw+0x4c0);expected.spawnTime=*(int*)(raw+0x4b0);
  if(strcmp(ent->classname,*(char**)(raw+0x1a4))||memcmp(ent,&expected,sizeof(expected))){fprintf(stderr,"init case%d mismatch\n",k);return 1;}}
 puts("1024 whole G_InitGentity comparisons passed");n=0;
 for(k=0;k<64;++k)for(freeMutation=-1;freeMutation<2;++freeMutation)for(unlinkMutation=-1;unlinkMutation<2;++unlinkMutation){int expectedCalls[8],expectedCount,never,spawn;raw=b+0x226e2a0;ent=&g_entities[0];memset(raw,0x25,0x57c);memset(ent,0x25,sizeof(*ent));ent->classname=*(char**)(raw+0x1a4)=initial;ent->free=k&1?freeCallback:NULL;*(void**)(raw+0x2a0)=ent->free;ent->neverFree=*(int*)(raw+0x1ac)=(k>>1)&1;ent->spawnCount=*(int*)(raw+0x4c0)=k-32;level.time=*(int*)(b+0x7febe0)=1000+k;expected=*ent;memcpy(rawExpected,raw,0x57c);
  mode=1;bad=count=0;release(raw);if(bad)return 3;expectedCount=count;memcpy(expectedCalls,calls,sizeof(calls));mode=0;bad=count=0;G_FreeEntity(ent);never=(k>>1)&1;spawn=k-32;if((k&1)&&freeMutation>=0){never=freeMutation;spawn+=13;}if(unlinkMutation>=0){never=unlinkMutation;spawn+=13;}
  if(never){expected.neverFree=never;expected.spawnCount=spawn;*(int*)(rawExpected+0x1ac)=never;*(int*)(rawExpected+0x4c0)=spawn;}else{memset(&expected,0,sizeof(expected));expected.classname=ent->classname;expected.freetime=level.time;expected.spawnCount=spawn;memset(rawExpected,0,0x57c);*(void**)(rawExpected+0x1a4)=*(void**)(raw+0x1a4);*(int*)(rawExpected+0x1bc)=level.time;*(int*)(rawExpected+0x4c0)=spawn;}
  if(bad||count!=expectedCount||memcmp(calls,expectedCalls,count*sizeof(int))||strcmp(ent->classname,*(char**)(raw+0x1a4))||memcmp(ent,&expected,sizeof(expected))||memcmp(raw,rawExpected,0x57c)){fprintf(stderr,"free case%d mutations%d/%d mismatch\n",k,freeMutation,unlinkMutation);return 1;}++n;}
 printf("%d whole G_FreeEntity comparisons passed\n",n);
 for(k=0;k<4096;++k){vec3_t origin,copy;void*r;int ev=k%256;raw=b+0x226e2a0;ent=&g_entities[0];memset(raw,0,0x57c);memset(ent,0,sizeof(*ent));level.time=*(int*)(b+0x7febe0)=k-2048;VectorSet(origin,k*.25f-512.125f,-k*.5f+.75f,k%33-16.875f);VectorCopy(origin,copy);mode=1;bad=count=0;r=temporary(origin,ev);if(bad||r!=raw||count!=2||calls[0]!=3||calls[1]!=4)return 4;mode=0;bad=count=0;r=G_TempEntity(origin,ev);
  if(bad||r!=ent||count!=2||calls[0]!=3||calls[1]!=4||memcmp(origin,copy,12)||ent->s.eType-ET_EVENTS!=*(int*)(raw+4)-0x43||strcmp(ent->classname,*(char**)(raw+0x1a4))||ent->eventTime!=*(int*)(raw+0x1c0)||ent->r.eventTime!=*(int*)(raw+0x184)||ent->freeAfterEvent!=*(int*)(raw+0x1c4)||memcmp(&ent->s.pos,raw+12,sizeof(trajectory_t))||memcmp(ent->r.currentOrigin,raw+0x168,12)){fprintf(stderr,"temp case%d mismatch\n",k);return 1;}}
 puts("4096 whole G_TempEntity comparisons passed (ET_EVENTS transport base normalized)");return 0;
}
