#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static int parseOK,found;static void *wanted,*parent;static char trace[128];static const char*opened;
static qboolean parse(char**p,const char**n){strcat(trace,"P");if(parseOK){*n="edit";++*p;}return parseOK;}
static itemDef_t*find(menuDef_t*m,const char*n){assert(m==parent&&!strcmp(n,"edit"));strcat(trace,"F");return found?wanted:NULL;}
static itemDef_t*clear(menuDef_t*m){assert(m==parent);strcat(trace,"C");return NULL;}
static void script(itemDef_t*i,qboolean*a,const char*s){assert(i==wanted&&!a&&!strcmp(s,"focus"));strcat(trace,"R");}
static void sound(int s,int c){assert(s==73&&c==6);strcat(trace,"S");}
static menuDef_t*activate(const char*n,qboolean f){assert(f==qtrue);opened=n;return NULL;}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;static unsigned char odc[0x20100];static displayContextDef_t dc;unsigned char raw[0x268],caller[0x268],data[64];itemDef_t item,call;editFieldDef_t edit;int type,mask,cases=0;assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x14cd0,parse);patch(String_Parse,parse);patch(b+0x15910,find);patch(Menu_FindItemByName,find);patch(b+0x15520,clear);patch(Menu_ClearFocus,clear);patch(b+0x17810,script);patch(Item_RunScript,script);patch(b+0x1d9e0,activate);patch(Menus_ActivateByName,activate);*(void**)(odc+0x88)=sound;*(void**)(b+0x245fa4)=odc;dc.startLocalSound=sound;DC=&dc;parent=(void*)0x123456;
for(type=-1;type<=20;++type)for(mask=0;mask<64;++mask){char input[]="name",*args=input,*originalArgs;char expected[128];int flags,cursor,offset,editing;void*ei; qboolean abort=qtrue;
parseOK=mask&1;found=!!(mask&2);memset(raw,0,sizeof(raw));memset(caller,0,sizeof(caller));memset(&item,0,sizeof(item));memset(&call,0,sizeof(call));memset(data,0,sizeof(data));memset(&edit,0,sizeof(edit));*(void**)(caller+0xec)=parent;call.parent=parent;*(int*)(raw+200)=item.type=type;*(int*)(raw+0x48)=item.window.flags=(mask&32)?0x412:0x410;*(int*)(raw+0x24c)=item.cursorPos=19;*(int*)(data+0x18)=edit.paintOffset=27;*(void**)(raw+0x250)=(mask&4)?data:NULL;item.typeData=(mask&4)?&edit:NULL;*(char**)(raw+0x10c)=(mask&8)?"focus":NULL;item.onFocus=(mask&8)?"focus":NULL;*(int*)(odc+0x1e324)=dc.Assets.itemFocusSound=(mask&16)?73:0;*(int*)(b+0x245fac)=g_editingField=0;*(void**)(b+0x245fb4)=g_editItem=NULL;
trace[0]=0;wanted=raw;((void(__cdecl*)(void*,qboolean*,char**))(b+0x17720))(caller,&abort,&args);assert(abort==qtrue);originalArgs=args;strcpy(expected,trace);flags=*(int*)(raw+0x48);cursor=*(int*)(raw+0x24c);offset=*(int*)(data+0x18);editing=*(int*)(b+0x245fac);ei=*(void**)(b+0x245fb4);assert(!ei||ei==raw);
args=input;trace[0]=0;wanted=&item;Script_SetEditFocus(&call,&abort,&args);assert(abort==qtrue&&args==originalArgs&&!strcmp(trace,expected));assert(item.window.flags==flags&&item.cursorPos==cursor&&edit.paintOffset==offset&&g_editingField==editing);assert((g_editItem!=NULL)==(ei!=NULL));if(g_editItem)assert(g_editItem==&item);++cases;}
{const char*names[]={NULL,"","main","hostgame"};for(mask=0;mask<4;++mask){opened=(void*)1;((void(__cdecl*)(const char*))(b+0x15d70))(names[mask]);assert(opened==names[mask]);opened=(void*)1;Menus_OpenByName(names[mask]);assert(opened==names[mask]);++cases;}}
printf("%d original edit-focus/open comparisons passed\n",cases);FreeLibrary(h);return 0;}
