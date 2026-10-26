#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static itemDef_t item;static byte pool[1024];static int calls,allocs,failAt,number,mode;static float fv;
static void*allocate(int n){assert(n==sizeof(listBoxDef_t));++allocs;return pool;}
static qboolean integer(int h,int*out){assert(h==71);++calls;if(calls==failAt)return qfalse;*out=calls==1?number:calls*37-811;return qtrue;}
static qboolean floating(int h,float*out){assert(h==71);++calls;if(calls==failAt)return qfalse;*out=fv;return qtrue;}
static struct{byte*p,saved[5];}hooks[6];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b,ep[1024];itemDef_t before,expected;int existing,unsupported,k,er,sr,ea,ec,cases=0;unsigned addresses[]={0x1e660,0x1e6a0,0x1e640};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(itemDef_t)==0x274&&sizeof(listBoxDef_t)==0xec);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x12d0,integer);patch(PC_Int_Parse,integer);patch(b+0x11e0,floating);patch(PC_Float_Parse,floating);
 for(mode=0;mode<3;++mode)for(number=-1;number<=18;++number)for(existing=0;existing<2;++existing)for(unsupported=0;unsupported<2;++unsupported)for(k=0;k<2;++k)for(failAt=0;failAt<=(mode==1?50:1);++failAt){memset(&item,0,sizeof(item));item.type=unsupported?ITEM_TYPE_BUTTON:ITEM_TYPE_LISTBOX;if(existing)item.typeData=pool;item.special=-77.713f;before=item;fv=(float)(number*.713-3.113);memset(pool,k*73+19,sizeof(pool));calls=allocs=0;er=((int(__cdecl*)(itemDef_t*,int))(b+addresses[mode]))(&item,71);expected=item;memcpy(ep,pool,sizeof(pool));ea=allocs;ec=calls;
  item=before;memset(pool,k*73+19,sizeof(pool));calls=allocs=0;sr=mode==0?ItemParse_elementtype(&item,71):mode==1?ItemParse_columns(&item,71):ItemParse_feeder(&item,71);assert(er==sr&&ea==allocs&&ec==calls&&!memcmp(&item,&expected,sizeof(item))&&!memcmp(pool,ep,sizeof(pool)));++cases;
 }
 restore();assert(FreeLibrary(h));printf("%d original list definition comparisons passed\n",cases);return 0;}
