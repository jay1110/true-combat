#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[1024];static int returnValue;
static void init(int a){sprintf(trace,"I:%d;",a);}static void shutdown(void){strcpy(trace,"SHUT;");}
static void key(int a,int b){sprintf(trace,"K:%d:%d;",a,b);}static void mouse(int a,int b){sprintf(trace,"M:%d:%d;",a,b);}
static void refresh(int a){sprintf(trace,"R:%d;",a);}static int fullscreen(void){strcpy(trace,"FULL;");return returnValue;}
static void active(int a){sprintf(trace,"ACTIVE:%d;",a);}static int console(int a){sprintf(trace,"CONSOLE:%d;",a);return returnValue;}
static void connect(int a){sprintf(trace,"CONNECT:%d;",a);}static int execKey(int a){sprintf(trace,"EXEC:%d;",a);return returnValue;}
/* Frame budget setup is a renderer-import dependency of dispatch, not its target. */
static void beginFrame(int frameTime){}
static struct{unsigned char*p,bytes[5];}hooks[24];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<24);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;int c,a,w,bind,menu,er,sr,cases=0;char expected[1024];int args[]={-37,0,1,913};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
#define PA(off,real,mock) patch(b+(off),mock);patch(real,mock)
patch(TCE_RendererBeginFrame,beginFrame);
PA(0xdb90,_UI_Init,init);PA(0x20c80,trap_LAN_SaveCachedServers,shutdown);PA(0x11c00,_UI_KeyEvent,key);PA(0x11cb0,_UI_MouseEvent,mouse);PA(0x4c70,_UI_Refresh,refresh);PA(0x1d9b0,Menus_AnyFullScreenVisible,fullscreen);PA(0x11d40,_UI_SetActiveMenu,active);PA(0x2420,UI_ConsoleCommand,console);PA(0x12460,UI_DrawConnectScreen,connect);PA(0x6620,UI_CheckExecKey,execKey);
for(c=-2;c<=16;++c)for(a=0;a<4;++a)for(returnValue=-1;returnValue<=2;++returnValue)for(w=0;w<2;++w)for(bind=0;bind<2;++bind)for(menu=0;menu<3;++menu){menutype=*(int*)(b+0x63f50)=menu*7;g_waitingForKey=*(int*)(b+0x245fa8)=w;g_bindItem=(itemDef_t*)(bind?0x123400:0);*(void**)(b+0x245fb0)=g_bindItem;
assert(_UI_GetActiveMenu()==((int(__cdecl*)(void))(b+0x11d30))());trace[0]=0;er=((int(__cdecl*)(int,int,int))(b+0x3de0))(c,args[a],args[3-a]);strcpy(expected,trace);trace[0]=0;sr=vmMain(c,args[a],args[3-a],3,4,5,6,7,8,9,10,11,12);assert(er==sr&&!strcmp(trace,expected));++cases;}
restore();assert(FreeLibrary(h));printf("%d original UI engine-dispatch comparisons passed\n",cases);return 0;}
