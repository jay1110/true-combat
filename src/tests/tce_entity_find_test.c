#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte*originalEntities;static int rng,rngCalls,prints,bad;static const char*query;
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int fixedRandom(void){++rngCalls;return rng;}
static void print(const char*fmt,...){va_list ap;const char*s;++prints;va_start(ap,fmt);s=va_arg(ap,const char*);va_end(ap);if(strcmp(fmt,"G_PickTarget: target %s not found\n")||s!=query)bad=1;}
static int originalIndex(void*p){return p?((byte*)p-originalEntities)/0x57c:-1;}
static int nativeIndex(gentity_t*p){return p?(int)(p-g_entities):-1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void*(__cdecl*find)(void*,int,const char*);void*(__cdecl*byName)(void*,const char*);void*(__cdecl*fast)(void*,const char*,int);void*(__cdecl*pick)(const char*);int seed,i,start,q,f,n=0;const char*names[]={NULL,"","alpha","ALPHA","beta","missing"};const int rawOffsets[]={0x1a4,0x254,0x258};const int offsets[]={FOFS(classname),FOFS(target),FOFS(targetname)};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;originalEntities=b+0x226e2a0;find=(void*)(b+0x948e0);byName=(void*)(b+0x94960);fast=(void*)(b+0x949f0);pick=(void*)(b+0x94a70);
 if(!hook(b+0xa20e4,fixedRandom)||!hook(rand,fixedRandom)||!hook(b+0x5ed80,print)||!hook(G_Printf,print))return 2;
 for(seed=0;seed<64;++seed){level.num_entities=*(int*)(b+0x7feb8c)=seed+1;memset(originalEntities,0,64*0x57c);memset(g_entities,0,64*sizeof(gentity_t));
  for(i=0;i<64;++i){byte*r=originalEntities+i*0x57c;gentity_t*e=&g_entities[i];e->inuse=*(int*)(r+0x194)=(i+seed)%3!=0;for(f=0;f<3;++f){char*s=(char*)names[(i*(f+1)+seed)%6];*(char**)((byte*)e+offsets[f])=*(char**)(r+rawOffsets[f])=s;}e->targetnamehash=*(int*)(r+0x25c)=e->targetname?BG_StringHashValue(e->targetname):-1;if(i%7==0)e->targetnamehash=*(int*)(r+0x25c)=BG_StringHashValue("alpha");}
  for(start=-1;start<64;++start)for(q=1;q<6;++q){void*from=start<0?NULL:originalEntities+start*0x57c;gentity_t*nfrom=start<0?NULL:&g_entities[start];int h=BG_StringHashValue(names[q]);
   for(f=0;f<3;++f)if(originalIndex(find(from,rawOffsets[f],names[q]))!=nativeIndex(G_Find(nfrom,offsets[f],names[q]))){fprintf(stderr,"find seed%d start%d query%d field%d mismatch\n",seed,start,q,f);return 1;}
   if(originalIndex(byName(from,names[q]))!=nativeIndex(G_FindByTargetname(nfrom,names[q]))||originalIndex(fast(from,names[q],h))!=nativeIndex(G_FindByTargetnameFast(nfrom,names[q],h))||originalIndex(fast(from,names[q],h^17))!=nativeIndex(G_FindByTargetnameFast(nfrom,names[q],h^17))){fprintf(stderr,"target find seed%d start%d query%d mismatch\n",seed,start,q);return 1;}++n;
  }
 }
 printf("%d G_Find, %d G_FindByTargetname and %d G_FindByTargetnameFast whole comparisons passed\n",n*3,n,n*2);n=0;
 for(seed=0;seed<=40;++seed){level.num_entities=*(int*)(b+0x7feb8c)=64;for(i=0;i<64;++i){byte*r=originalEntities+i*0x57c;gentity_t*e=&g_entities[i];e->inuse=*(int*)(r+0x194)=i<seed;e->targetname=*(char**)(r+0x258)="alpha";e->targetnamehash=*(int*)(r+0x25c)=BG_StringHashValue("alpha");}
  for(rng=0;rng<128;++rng){int expected,expectedRng,expectedPrint;query="alpha";rngCalls=prints=bad=0;expected=originalIndex(pick(query));expectedRng=rngCalls;expectedPrint=prints;if(bad)return 3;rngCalls=prints=bad=0;if(nativeIndex(G_PickTarget((char*)query))!=expected||rngCalls!=expectedRng||prints!=expectedPrint||bad){fprintf(stderr,"pick choices%d rng%d mismatch\n",seed,rng);return 1;}++n;}}
 query=NULL;rngCalls=prints=bad=0;if(pick(NULL)||G_PickTarget(NULL)||rngCalls||prints||bad)return 4;
 printf("%d whole G_PickTarget comparisons plus NULL early return passed\n",n);return 0;
}
