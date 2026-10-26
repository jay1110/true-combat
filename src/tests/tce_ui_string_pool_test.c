#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static void reset(unsigned char*b,int start){memset(strHandle,0,sizeof(strHandle));memset(b+0x63f74,0,8192);strPoolIndex=*(int*)(b+0x245fc8)=start;UI_InitMemory();((void(__cdecl*)(void))(b+0x148f0))();}
static void compare(unsigned char*b,const char*s){const char*a=((const char*(__cdecl*)(const char*))(b+0x14910))(s);const char*c=String_Alloc(s);int k;assert((a==NULL)==(c==NULL));if(a){assert(!strcmp(a,c));if(*a)assert(a-((char*)b+0x65f78)==c-strPool);}assert(strPoolIndex==*(int*)(b+0x245fc8));assert(allocPoint==*(int*)(b+0x65f74));for(k=0;k<HASH_TABLE_SIZE;++k){stringDef_t*x=*(stringDef_t**)(b+0x63f74+k*4),*y=strHandle[k];while(x&&y){assert(!strcmp(x->str,y->str));assert((char*)x-((char*)b+0xc5f78)==(char*)y-memoryPool);x=x->next;y=y->next;}assert(!x&&!y);}}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int i,j,cases=0;char text[80];const char*edge[]={NULL,"","X","XY","MixedCase","mixedcase"};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;reset(b,0);
for(i=0;i<1200;++i){sprintf(text,"Menu_%d_Item_%d",i%151,i%73);assert(hashForString(text)==((unsigned(__cdecl*)(const char*))(b+0x14a30))(text));compare(b,text);compare(b,text);cases+=3;}
for(i=0;i<8;++i)for(j=0;j<6;++j){reset(b,STRING_POOL_SIZE-i);compare(b,edge[j]);++cases;}
printf("%d original UI string interning/hash comparisons passed\n",cases);FreeLibrary(h);return 0;}
