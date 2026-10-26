#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static itemDef_t pool;static byte rawPool[0x280];static int original,failAlloc,parseOK,allocations,parses;static const char*textValue;
static void*allocate(int n){assert(n==(original?0x274:sizeof(itemDef_t)));++allocations;return failAlloc?NULL:original?(void*)rawPool:(void*)&pool;}
static qboolean parse(int h,const char**out){assert(h==71);++parses;if(!parseOK)return qfalse;*out=textValue;return qtrue;}
static struct{byte*p,saved[5];}hooks[4];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b,parent[0x280];itemDef_t item;int mode,existing,k,v,rep,er,sr,ea,ep,cases=0;const char*texts[]={"","Host advanced setting","Line one\nLine two","^3Colored hint"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(itemDef_t)==0x274);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x1370,parse);patch(PC_String_Parse,parse);
 for(mode=0;mode<2;++mode)for(existing=0;existing<2;++existing)for(failAlloc=0;failAlloc<2;++failAlloc)for(parseOK=0;parseOK<2;++parseOK)for(k=0;k<8;++k)for(v=0;v<4;++v){textValue=texts[v];memset(&item,0,sizeof(item));memset(parent,0,sizeof(parent));memset(&pool,k*31,sizeof(pool));memset(rawPool,k*31,sizeof(rawPool));if(existing){item.toolTipData=&pool;*(void**)(parent+0x270)=rawPool;}
  for(rep=0;rep<2;++rep){original=1;allocations=parses=0;er=mode?((int(__cdecl*)(void*,int))(b+0x1f220))(parent,71):((int(__cdecl*)(void*))(b+0x1dfe0))(parent);ea=allocations;ep=parses;original=0;allocations=parses=0;sr=mode?ItemParse_tooltip(&item,71):Item_ValidateTooltipData(&item);
   assert(er==sr&&ea==allocations&&ep==parses);assert((item.toolTipData==NULL)==(*(void**)(parent+0x270)==NULL));if(item.toolTipData)assert(!memcmp(&pool,rawPool,sizeof(pool)));++cases;
  }
 }
 restore();assert(FreeLibrary(h));printf("%d original tooltip allocation/parser comparisons passed\n",cases);return 0;}
