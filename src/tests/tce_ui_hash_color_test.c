#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static int nodeIndex(keywordHash_t *p,keywordHash_t *base){return p?(int)(p-base):-1;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;keywordHash_t nodes[160],refs[160],*table[512],*original[512];char names[160][24],query[24];int n,i,j,ec,ac,count=0;float a[4],z[4],out[4],expected[4],fraction;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;assert(sizeof(keywordHash_t)==12);
 for(n=0;n<=160;n+=16){memset(nodes,0,sizeof(nodes));memset(refs,0,sizeof(refs));memset(table,0,sizeof(table));memset(original,0,sizeof(original));
  for(i=0;i<n;++i){sprintf(names[i],"key%d",i%79);nodes[i].keyword=refs[i].keyword=names[i];
   ((void(__cdecl*)(keywordHash_t**,keywordHash_t*))(b+0x1e0a0))(original,&refs[i]);KeywordHash_Add(table,&nodes[i]);
   for(j=0;j<512;++j)assert(nodeIndex(table[j],nodes)==nodeIndex(original[j],refs));
   for(j=0;j<=i;++j)assert(nodeIndex(nodes[j].next,nodes)==nodeIndex(refs[j].next,refs));++count;
  }
  for(i=0;i<180;++i){sprintf(query,i%2?"KEY%d":"key%d",i/2);ec=nodeIndex(((keywordHash_t*(__cdecl*)(keywordHash_t**,char*))(b+0x1e0c0))(original,query),refs);ac=nodeIndex(KeywordHash_Find(table,query),nodes);assert(ec==ac);++count;}
 }
 for(i=0;i<1000;++i){for(j=0;j<4;++j){a[j]=((i*(j+3))%101-50)*.05f;z[j]=((i*(j+7))%97-48)*.07f;}fraction=(i%41-20)*.1f;
  ((void(__cdecl*)(float*,float*,float*,float))(b+0x14b50))(a,z,expected,fraction);LerpColor(a,z,out,fraction);
  for(j=0;j<4;++j)assert(out[j]==expected[j]);++count;
 }
 printf("%d original hash-chain/find and color comparisons passed\n",count);FreeLibrary(h);return 0;}
