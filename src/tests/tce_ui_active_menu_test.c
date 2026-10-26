#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[16384],errorText[4096];static int menus,catcher,diagnose,missing;
static void add(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static int count(void){add("count;");return menus;}
static void closeall(void){add("close;");}
static menuDef_t*activate(const char*n,qboolean f){add("activate:%s,%d;",n,f);return NULL;}
static void openmenu(const char*n){add("open:%s;",n);}
static void players(void){add("players;");}
static void feeder(menuDef_t*m,int f,int i,const char*n){assert(!m&&!n);add("feeder:%d,%d;",f,i);}
static int getcatch(void){add("getcatch;");return catcher;}
static void setcatch(int c){add("catch:%d;",c);}
static void clear(void){add("clear;");}
static void set(const char*n,const char*v){add("set:%s=%s;",n,v);if(!strcmp(n,"com_errorMessage"))Q_strncpyz(errorText,v,sizeof(errorText));}
static void string(const char*n,char*b,int size){const char*v="";if(!strcmp(n,"com_errorMessage"))v=errorText;else if(!strcmp(n,"com_errorDiagnoseIP"))v=diagnose?"127.0.0.1":"";else if(!strcmp(n,"com_missingFiles"))v=missing?"missing.pk3":"";Q_strncpyz(b,v,size);}
static const char*translate(const char*s){return s;}
static void fade(float v,int t,qboolean stop){add("fade:%.9g,%d,%d;",v,t,stop);}
static void command(int m,const char*s){add("cmd:%d:%s;",m,s);}
static void print(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static struct{unsigned char*p,bytes[5];}hooks[30];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<30);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;byte*b;int mode,e,redirect,gt,cases=0;char expected[16384],finalError[4096];int mt,x,y;const char*errors[]={"",";","Invalid password for server","ET://127.0.0.1:27960","ET://","PunkBuster must be Enabled","Ordinary server error"};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
#define PAIR(offset,source,stub) patch(b+offset,stub);patch(source,stub)
PAIR(0x1fbc0,Menu_Count,count);PAIR(0x15e80,Menus_CloseAll,closeall);PAIR(0x1d9e0,Menus_ActivateByName,activate);PAIR(0x15d70,Menus_OpenByName,openmenu);PAIR(0x104d0,UI_BuildPlayerList,players);PAIR(0x1d8f0,Menu_SetFeederSelection,feeder);PAIR(0x20a80,trap_Key_GetCatcher,getcatch);PAIR(0x20a90,trap_Key_SetCatcher,setcatch);PAIR(0x20a70,trap_Key_ClearStates,clear);PAIR(0x20530,trap_Cvar_Set,set);PAIR(0x20570,trap_Cvar_VariableStringBuffer,string);PAIR(0x20f40,trap_TranslateString,translate);PAIR(0x20970,trap_S_FadeAllSound,fade);PAIR(0x205d0,trap_Cmd_ExecuteText,command);PAIR(0x2380,Com_Printf,print);
for(menus=0;menus<2;++menus)for(mode=-1;mode<=16;++mode)for(e=0;e<7;++e)for(redirect=0;redirect<2;++redirect)for(gt=0;gt<2;++gt)for(diagnose=0;diagnose<2;++diagnose)for(missing=0;missing<2;++missing){catcher=7;ui_autoredirect.integer=*(int*)(b+0x2db78c)=redirect;g_gameType.integer=*(int*)(b+0x2dd28c)=gt?5:0;menutype=*(int*)(b+0x63f50)=77;uiInfo.uiDC.cursorx=*(int*)(b+0x292148)=23;uiInfo.uiDC.cursory=*(int*)(b+0x29214c)=37;
strcpy(errorText,errors[e]);trace[0]=0;((void(__cdecl*)(int))(b+0x11d40))(mode);strcpy(expected,trace);strcpy(finalError,errorText);mt=*(int*)(b+0x63f50);x=*(int*)(b+0x292148);y=*(int*)(b+0x29214c);strcpy(errorText,errors[e]);trace[0]=0;_UI_SetActiveMenu(mode);if(strcmp(expected,trace)||strcmp(finalError,errorText)||mt!=menutype||x!=uiInfo.uiDC.cursorx||y!=uiInfo.uiDC.cursory){fprintf(stderr,"active menu mismatch menus%d mode%d err%d redirect%d gt%d diagnose%d missing%d\nO%s\nS%s\n",menus,mode,e,redirect,gt,diagnose,missing,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original active-menu comparisons passed\n",cases);return 0;}
