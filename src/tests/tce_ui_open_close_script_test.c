#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static char event[1024];static int calls;
static void openmenu(const char*n){sprintf(event,"open:%s",n);++calls;}
static void closemenu(const char*n){sprintf(event,"close:%s",n);++calls;}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,i,flag,cases=0;const char*inputs[]={""," ","main","main trailing","\"host game\" rest","\"\" rest","// comment\nmain","/* c */ main","; rest","MAIN","\n\tname\nrest","{ name"};char orig[256],src[256],expected[1024];assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x15d70,openmenu);patch(Menus_OpenByName,openmenu);patch(b+0x15d80,closemenu);patch(Menus_CloseByName,closemenu);memset(strHandle,0,sizeof(strHandle));memset(b+0x63f74,0,8192);strPoolIndex=*(int*)(b+0x245fc8)=0;UI_InitMemory();((void(__cdecl*)(void))(b+0x148f0))();
for(mode=0;mode<2;++mode)for(i=0;i<12;++i)for(flag=0;flag<3;++flag){char *a=orig,*c=src;qboolean oa=flag,ca=flag;int count;strcpy(orig,inputs[i]);strcpy(src,inputs[i]);event[0]=0;calls=0;((void(__cdecl*)(itemDef_t*,qboolean*,char**))(b+(mode?0x16ba0:0x15fc0)))(NULL,flag==2?NULL:&oa,&a);strcpy(expected,event);count=calls;calls=0;event[0]=0;if(mode)Script_Close(NULL,flag==2?NULL:&ca,&c);else Script_Open(NULL,flag==2?NULL:&ca,&c);assert(count==calls&&!strcmp(expected,event)&&oa==ca);assert((a==NULL)==(c==NULL));if(a)assert(a-orig==c-src);++cases;}
printf("%d original UI open/close script comparisons passed\n",cases);FreeLibrary(h);return 0;}
