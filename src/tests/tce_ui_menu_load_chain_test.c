#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[16384];static int pos[3],missing,assetOK;static const char**outer,**inner;
static void debug(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static int load(const char*n){sprintf(trace+strlen(trace),"L:%s;",n);pos[2]=0;return missing?0:2;}
static int readToken(int h,pc_token_t*t){const char*s;assert(h==1||h==2);s=(h==1?outer:inner)[pos[h]];sprintf(trace+strlen(trace),"R:%d:%s;",h,s?s:"EOF");if(!s)return 0;++pos[h];memset(t,0,sizeof(*t));strcpy(t->string,s);return 1;}
static int release(int h){sprintf(trace+strlen(trace),"F:%d;",h);return 1;}
static qboolean asset(int h){sprintf(trace+strlen(trace),"A:%d;",h);return assetOK;}
static void menu(int h){sprintf(trace+strlen(trace),"M:%d;",h);}
static struct{byte*p,saved[5];}hooks[12];static int hc;
static void patch(void*a,void*f){DWORD old,tmp;byte*p=a;assert(hc<12);hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void restore(void){DWORD old,tmp;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&old));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,old,&tmp);}}
int main(int argc,char**argv){HMODULE h;byte*b;int o,i,mode,expectedResult,result,cases=0,expectedPos[3];char expected[16384];
 const char*outs[8][6]={{NULL},{"bad",NULL},{"{","}",NULL},{"{","",NULL},{"{","ui/host.menu","}",NULL},{"{","ui/host.menu","ui/main.menu","}",NULL},{"{suffix","ui/a.menu","}suffix",NULL},{"{","ui/a.menu",NULL}};
 const char*ins[8][7]={{NULL},{"}",NULL},{"menudef",NULL},{"MENUDEF","menudef",NULL},{"assetGlobalDef","menudef",NULL},{"ASSETGLOBALDEF","unknown","menudef","}",NULL},{"unknown","","menudef",NULL},{"}suffix","menudef",NULL}};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(pc_token_t)==0x418);
 patch(b+0x22e0,debug);patch(Com_DPrintf,debug);patch(b+0x20d90,load);patch(trap_PC_LoadSource,load);patch(b+0x20dd0,readToken);patch(trap_PC_ReadToken,readToken);patch(b+0x20db0,release);patch(trap_PC_FreeSource,release);patch(b+0x4eb0,asset);patch(Asset_Parse,asset);patch(b+0x1fb60,menu);patch(Menu_New,menu);
 for(mode=0;mode<2;++mode)for(o=0;o<(mode?8:1);++o)for(i=0;i<8;++i)for(missing=0;missing<2;++missing)for(assetOK=0;assetOK<2;++assetOK){outer=outs[o];inner=ins[i];memset(pos,0,sizeof(pos));trace[0]=0;
  expectedResult=mode?((int(__cdecl*)(int))(b+0x5360))(1):((int(__cdecl*)(const char*))(b+0x52a0))("ui/host.menu");strcpy(expected,trace);memcpy(expectedPos,pos,sizeof(pos));memset(pos,0,sizeof(pos));trace[0]=0;
  result=mode?Load_Menu(1):UI_ParseMenu("ui/host.menu");assert(result==expectedResult&&!strcmp(trace,expected)&&!memcmp(pos,expectedPos,sizeof(pos)));++cases;
 }
 restore();assert(FreeLibrary(h));printf("%d original nested menu-parser comparisons passed\n",cases);return 0;}
