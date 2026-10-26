#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static menuDef_t menu;static itemDef_t pool;static listBoxDef_t list;static int allocs,parses,copies,parseOK,key,type,intOK,scriptOK,value,ints,scripts;
static const char*action="setcvar ui_probe 1 ;";
static void*allocate(int n){assert(n==sizeof(itemDef_t));++allocs;return &pool;}
static qboolean itemParse(int h,itemDef_t*i){assert(h==71&&i==&pool);++parses;i->hotkey=key;i->onKey=action;i->type=type;i->cursorPos=23;if(type==ITEM_TYPE_LISTBOX)i->typeData=&list;return parseOK?qtrue:qfalse;}
static const char*copy(const char*s){assert(s==action);++copies;return action;}
static qboolean integer(int h,int*out){assert(h==71&&out==&menu.timeout);++ints;if(!intOK)return qfalse;*out=value;return qtrue;}
static qboolean script(int h,const char**out){assert(h==71&&out==&menu.onTimeout);++scripts;if(!scriptOK)return qfalse;*out=action;return qtrue;}
static struct{byte*p,saved[5];}hooks[10];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;menuDef_t before,expected;itemDef_t ep;listBoxDef_t el;int n,hm,k,t,er,sr,ea,ec,epar,ei,es,cases=0,timecases=0;int keys[]={-1,0,13,127,255},values[]={-1,0,1,6000,2147483647,(-2147483647-1)};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(menuDef_t)==0x718&&sizeof(itemDef_t)==0x274);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x1f340,itemParse);patch(Item_Parse,itemParse);patch(b+0x14910,copy);patch(String_Alloc,copy);patch(b+0x12d0,integer);patch(PC_Int_Parse,integer);patch(b+0x14d40,script);patch(PC_Script_Parse,script);
 for(n=0;n<=129;++n)for(hm=0;hm<2;++hm)for(parseOK=0;parseOK<2;++parseOK)for(k=0;k<5;++k)for(t=0;t<2;++t){key=keys[k];type=t?ITEM_TYPE_LISTBOX:ITEM_TYPE_TEXT;memset(&menu,0,sizeof(menu));menu.itemCount=n;menu.itemHotkeyMode=hm;before=menu;memset(&pool,0x39,sizeof(pool));memset(&list,0x57,sizeof(list));allocs=parses=copies=0;er=((int(__cdecl*)(itemDef_t*,int))(b+0x1f850))((itemDef_t*)&menu,71);expected=menu;ep=pool;el=list;ea=allocs;epar=parses;ec=copies;
  menu=before;memset(&pool,0x39,sizeof(pool));memset(&list,0x57,sizeof(list));allocs=parses=copies=0;sr=MenuParse_itemDef((itemDef_t*)&menu,71);assert(er==sr&&allocs==ea&&parses==epar&&copies==ec);assert(!memcmp(&menu,&expected,sizeof(menu))&&!memcmp(&pool,&ep,sizeof(pool))&&!memcmp(&list,&el,sizeof(list)));++cases;
 }
 for(intOK=0;intOK<2;++intOK)for(scriptOK=0;scriptOK<2;++scriptOK)for(k=0;k<6;++k)for(t=0;t<3;++t){value=values[k];memset(&menu,t*31+3,sizeof(menu));before=menu;ints=scripts=0;er=((int(__cdecl*)(itemDef_t*,int))(b+0x1f570))((itemDef_t*)&menu,71);expected=menu;ei=ints;es=scripts;menu=before;ints=scripts=0;sr=MenuParse_onTimeout((itemDef_t*)&menu,71);assert(er==sr&&ei==ints&&es==scripts&&!memcmp(&menu,&expected,sizeof(menu)));++timecases;}
 restore();assert(FreeLibrary(h));printf("%d original menu item lifecycle and %d timeout comparisons passed\n",cases,timecases);return 0;}
