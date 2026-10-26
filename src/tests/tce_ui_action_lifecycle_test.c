#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static itemDef_t*received;static const char*receivedScript;static qboolean*receivedAbort;static int calls;
static void script(itemDef_t*i,qboolean*a,const char*s){received=i;receivedAbort=a;receivedScript=s;++calls;}
static void capture(void*p){assert(0);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;itemDef_t item;unsigned char raw[0x268];int i,mode,cases=0;const char*scripts[]={NULL,"","open menu;","setcvar test value;"};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
/* Original Item_RunScript dispatch boundary. */
patch(b+0x17810,script);patch(Item_RunScript,script);
for(i=0;i<4;++i)for(mode=0;mode<2;++mode){int n;const char*s;memset(&item,0,sizeof(item));memset(raw,0,sizeof(raw));item.action=scripts[i];*(const char**)(raw+0x104)=scripts[i];calls=0;((void(__cdecl*)(void*))(b+0x19d80))(mode?raw:NULL);n=calls;s=receivedScript;if(mode){assert(received==(itemDef_t*)raw&&receivedAbort==NULL);}calls=0;Item_Action(mode?&item:NULL);assert(n==calls);if(mode)assert(received==&item&&receivedAbort==NULL&&s==receivedScript);++cases;}
for(i=0;i<2;++i){captureFunc=i?capture:NULL;*(void**)(b+0x245f98)=i?capture:NULL;assert(Menus_CaptureFuncActive()==((int(__cdecl*)(void))(b+0x1a0f0))());++cases;}
for(i=0;i<=128;++i){menuCount=*(int*)(b+0x245fb8)=i;Menus[0].window.flags=0x13579;*(int*)(b+0x255da0+0x48)=0x13579;Menu_Reset();((void(__cdecl*)(void))(b+0x1fd50))();assert(menuCount==0&&*(int*)(b+0x245fb8)==0);assert(Menus[0].window.flags==0x13579&&*(int*)(b+0x255da0+0x48)==0x13579);++cases;}
printf("%d original UI action/capture/reset comparisons passed\n",cases);FreeLibrary(h);return 0;}

