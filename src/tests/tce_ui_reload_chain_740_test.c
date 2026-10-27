#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[16384];static const char**tokens;static int pos,clockCalls,conn,failLoads,loads,groupOK;static menuDef_t focus;static char*menuSet;
static void debug(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static int milliseconds(void){strcat(trace,"T;");return clockCalls++?119:100;}
static void state(uiClientState_t*s){memset(s,0,sizeof(*s));s->connState=conn;strcat(trace,"STATE;");}
static int define(char*s){sprintf(trace+strlen(trace),"D:%s;",s);return 1;}
static int load(const char*s){sprintf(trace+strlen(trace),"L:%s;",s);return loads++<failLoads?0:17;}
static void error(const char*s){sprintf(trace+strlen(trace),"ERR:%s;",s);}
static int readToken(int h,pc_token_t*t){const char*s=tokens[pos];sprintf(trace+strlen(trace),"R:%d:%s;",h,s?s:"EOF");if(!s)return 0;++pos;memset(t,0,sizeof(*t));strcpy(t->string,s);return 1;}
static int freeSource(int h){sprintf(trace+strlen(trace),"FREE:%d;",h);return 1;}
static qboolean group(int h){sprintf(trace+strlen(trace),"GROUP:%d;",h);return groupOK;}
static void reset(void){strcat(trace,"RESET;");}
static menuDef_t*focused(void){strcat(trace,"FOCUS;");return &focus;}
static char*cvar(const char*n){assert(!strcmp(n,"ui_menuFiles"));strcat(trace,"CVAR;");return menuSet;}
static void strings(void){strcat(trace,"STRINGS;");}
static void gameinfo(const char*n){sprintf(trace+strlen(trace),"GAMEINFO:%s;",n);}
static void arenas(void){strcat(trace,"ARENAS;");}
static void closeAll(void){strcat(trace,"CLOSE;");}
static menuDef_t*activate(const char*n,qboolean modal){sprintf(trace+strlen(trace),"ACTIVATE:%s:%d;",n,modal);return NULL;}
static struct{byte*p,saved[5];}hooks[40];static int hc;
static void patch(void*a,void*f){DWORD old,tmp;byte*p=a;assert(hc<40);hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void restore(void){DWORD old,tmp;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&old));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,old,&tmp);}}
int main(int argc,char**argv){HMODULE h;byte*b;int mode,k,r,m,i,cases=0,safetyCases=0,ep;char expected[16384];const char*streams[6][5]={{NULL},{"}",NULL},{"",NULL},{"loadmenu",NULL},{"ignored","LOADMENU","loadmenu",NULL},{"}suffix","loadmenu",NULL}};char*sets[]={NULL,"","ui/menus_gt5.txt"};const char*names[]={"hostgame","","main"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
#define PA(off,real,mock) patch(b+(off),mock);patch(real,mock)
 PA(0x204e0,trap_Milliseconds,milliseconds);PA(0x20ab0,trap_GetClientState,state);PA(0x20d60,trap_PC_AddGlobalDefine,define);PA(0x20d90,trap_PC_LoadSource,load);PA(0x204c0,trap_Error,error);PA(0x20dd0,trap_PC_ReadToken,readToken);PA(0x20db0,trap_PC_FreeSource,freeSource);PA(0x5360,Load_Menu,group);PA(0x1fd50,Menu_Reset,reset);PA(0x22e0,Com_DPrintf,debug);
 PA(0x1d8a0,Menu_GetFocused,focused);PA(0x23e0,UI_Cvar_VariableString,cvar);PA(0x14af0,String_Init,strings);PA(0xd980,UI_ParseGameInfo,gameinfo);PA(0x2820,UI_LoadArenas,arenas);PA(0x15e80,Menus_CloseAll,closeAll);PA(0x1d9e0,Menus_ActivateByName,activate);
 for(mode=0;mode<2;++mode)for(k=0;k<6;++k)for(conn=0;conn<4;++conn)for(failLoads=0;failLoads<3;++failLoads)for(groupOK=0;groupOK<2;++groupOK)for(r=0;r<2;++r)for(m=0;m<(mode?3:1);++m){tokens=streams[k];menuSet=sets[m];focus.window.name=names[m];trace[0]=0;pos=clockCalls=loads=0;*(int*)(b+0x2dc62c)=27;
  if(mode)((void(__cdecl*)(void))(b+0x54f0))();else((void(__cdecl*)(const char*,int))(b+0x53e0))("ui/host.txt",r);strcpy(expected,trace);ep=pos;assert(*(int*)(b+0x2dc62c)==1);
  /* Intentional safety divergence: a focused menu with an empty name is not
   * reactivated. All preceding original service calls must still match. */
  if(mode&&!names[m][0]){char*tail=strstr(expected,"ACTIVATE::1;");assert(tail&&!strcmp(tail,"ACTIVATE::1;"));*tail=0;++safetyCases;}
  trace[0]=0;pos=clockCalls=loads=0;ui_new.integer=27;if(mode)UI_Load();else UI_LoadMenus("ui/host.txt",r);assert(ui_new.integer==1&&ep==pos&&!strcmp(expected,trace));++cases;
 }
 restore();assert(FreeLibrary(h));printf("%d menu reload-chain checks passed (%d intentional empty-name safety cases)\n",cases,safetyCases);return 0;}
