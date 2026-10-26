#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static byte pool[1024];static itemDef_t item,tip;static int type,parseOK,parses,allocs,sz,tooltips;
static void*allocate(int n){assert(n>0&&n<=sizeof(pool));++allocs;sz=n;return pool;}
static qboolean integer(int h,int*out){assert(h==71&&out==&item.type);++parses;if(!parseOK)return qfalse;*out=type;return qtrue;}
static void tooltip(itemDef_t*i){assert(i==&item&&i->toolTipData==&tip);++tooltips;if(i->type==ITEM_TYPE_LISTBOX){assert(i->cursorPos==0);if(i->typeData){listBoxDef_t*l=i->typeData;assert(l->cursorPos==0&&l->startPos==0&&l->endPos==0);}}tip.window.rect.x=123.713f;}
static struct{byte*p,saved[5];}hooks[6];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b,expectedPool[1024];itemDef_t before,expected,expectedTip;int old,existing,k,er,sr,ea,es,ep,et,t,cases=0,controlCases=0;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(itemDef_t)==0x274);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x12d0,integer);patch(PC_Int_Parse,integer);patch(b+0x147a0,tooltip);patch(Tooltip_ComputePosition,tooltip);
 for(type=-1;type<=18;++type)for(old=-1;old<=18;++old)for(existing=0;existing<2;++existing)for(parseOK=0;parseOK<2;++parseOK)for(k=0;k<3;++k){memset(&item,0,sizeof(item));item.type=old;if(existing)item.typeData=pool;before=item;memset(pool,k*59+13,sizeof(pool));parses=allocs=sz=0;er=((int(__cdecl*)(itemDef_t*,int))(b+0x1e610))(&item,71);expected=item;memcpy(expectedPool,pool,sizeof(pool));ea=allocs;es=sz;ep=parses;
  item=before;memset(pool,k*59+13,sizeof(pool));parses=allocs=sz=0;sr=ItemParse_type(&item,71);assert(er==sr&&ea==allocs&&es==sz&&ep==parses&&!memcmp(&item,&expected,sizeof(item))&&!memcmp(pool,expectedPool,sizeof(pool)));++cases;
 }
 for(type=-1;type<=18;++type)for(existing=0;existing<2;++existing)for(t=0;t<2;++t)for(k=0;k<8;++k){memset(&item,0,sizeof(item));item.type=type;item.cursorPos=k*17-3;if(existing)item.typeData=pool;if(t)item.toolTipData=&tip;before=item;memset(&tip,0,sizeof(tip));memset(pool,k*29+3,sizeof(pool));tooltips=0;((void(__cdecl*)(itemDef_t*))(b+0x1f410))(&item);expected=item;expectedTip=tip;memcpy(expectedPool,pool,sizeof(pool));et=tooltips;
  item=before;memset(&tip,0,sizeof(tip));memset(pool,k*29+3,sizeof(pool));tooltips=0;Item_InitControls(&item);assert(et==tooltips&&!memcmp(&item,&expected,sizeof(item))&&!memcmp(&tip,&expectedTip,sizeof(tip))&&!memcmp(pool,expectedPool,sizeof(pool)));++controlCases;
 }
 tooltips=0;((void(__cdecl*)(itemDef_t*))(b+0x1f410))(NULL);Item_InitControls(NULL);assert(!tooltips);++controlCases;
 restore();assert(FreeLibrary(h));printf("%d original type parser and %d control initialization comparisons passed\n",cases,controlCases);return 0;}
