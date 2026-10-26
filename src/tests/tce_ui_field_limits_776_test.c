#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static byte pools[2][1024];
static int original, allocations, parses, size, parseOK, value, failAlloc;
static void *allocate(int n) { ++allocations; size=n; assert(n>0&&n<=1024); return failAlloc?NULL:pools[original]; }
static qboolean parse(int h,int*out) { assert(h==71); ++parses; if(!parseOK)return qfalse; *out=value; return qtrue; }
static struct {byte*p,saved[5];} hooks[4]; static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;itemDef_t items[2],normalized[2];int mode,type,existing,k,v,rep,er,sr,ea,ep,es,cases=0;int values[]={-1,0,1,255,256,1024,2147483647,(-2147483647-1)};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
 assert(sizeof(itemDef_t)==0x274&&offsetof(itemDef_t,typeData)==0x250);
 assert(sizeof(editFieldDef_t)==0x1c&&sizeof(listBoxDef_t)==0xec&&sizeof(multiDef_t)==0x18c&&sizeof(modelDef_t)==0x40);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x12d0,parse);patch(PC_Int_Parse,parse);
 for(mode=0;mode<3;++mode)for(type=-1;type<=18;++type)for(existing=0;existing<2;++existing)for(parseOK=0;parseOK<2;++parseOK)for(k=0;k<3;++k)for(v=0;v<8;++v)for(failAlloc=0;failAlloc<2;++failAlloc){
  /* Original zero-initializing branches dereference NULL on OOM; undefined execution is excluded. */
  if(!existing&&failAlloc&&(type==6||type==4||type==9||type==11||type==13||type==10||type==0))continue;
  value=values[v];memset(items,0,sizeof(items));memset(pools,k*67+11,sizeof(pools));items[0].type=items[1].type=type;
  if(existing){items[0].typeData=pools[0];items[1].typeData=pools[1];}
  for(rep=0;rep<2;++rep){original=1;allocations=parses=size=0;er=0;
   if(mode==0)((void(__cdecl*)(itemDef_t*))(b+0x1def0))(&items[1]);else er=((int(__cdecl*)(itemDef_t*,int))(b+(mode==1?0x1eb90:0x1ebf0)))(&items[1],71);
   ea=allocations;ep=parses;es=size;original=0;allocations=parses=size=0;sr=0;
   if(mode==0)Item_ValidateTypeData(&items[0]);else sr=mode==1?ItemParse_maxChars(&items[0],71):ItemParse_maxPaintChars(&items[0],71);
   assert(er==sr&&ea==allocations&&ep==parses&&es==size);assert((items[0].typeData==NULL)==(items[1].typeData==NULL));
   assert(!memcmp(pools[0],pools[1],sizeof(pools[0])));normalized[0]=items[0];normalized[1]=items[1];normalized[0].typeData=normalized[1].typeData=NULL;assert(!memcmp(normalized,normalized+1,sizeof(itemDef_t)));++cases;
  }
 }
 restore();assert(FreeLibrary(h));printf("%d original field-limit/type allocation comparisons passed\n",cases);return 0;}
