#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[4096];static float input;
static float get(const char*n){sprintf(trace+strlen(trace),"get:%s;",n);return input;}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"set:%s=%s;",n,v);}
static void value(const char*n,float v){sprintf(trace+strlen(trace),"value:%s=%.9g;",n,v);}
static void command(int m,const char*s){sprintf(trace+strlen(trace),"cmd%d:%s;",m,s);}
static struct{unsigned char*p,bytes[5];}hooks[8];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<8);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;byte*b;int act,v,n,capital,cases=0,j;char expected[4096],ot[256],st[256],*op,*sp;const char*actions[]={"initHostGameFeatures","toggleMaxLives","toggleWeaponRestriction","voteGame","refGame","rconGame","clientCheckVote","glCustom","voteTimelimit","refTimelimit","voteWarmupDamage","refWarmupDamage"};float values[]={-1.25f,-.75f,0,.75f,1,2,5,7,8,99.75f,100,100.75f,255};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;patch(b+0x20550,get);patch(trap_Cvar_VariableValue,get);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20590,value);patch(trap_Cvar_SetValue,value);patch(b+0x205d0,command);patch(trap_Cmd_ExecuteText,command);Init_Display(NULL);*(void**)(b+0x245fa4)=NULL;String_Init();((void(__cdecl*)(void))(b+0x14af0))();
for(act=0;act<12;++act)for(v=0;v<13;++v)for(n=0;n<3;++n)for(capital=0;capital<2;++capital){input=values[v];uiInfo.numGameTypes=*(int*)(b+0x2b3280)=n==0?0:n==1?8:10;ui_netGameType.integer=*(int*)(b+0x2dc50c)=(int)input;sprintf(ot,"%s trailing",actions[act]);if(capital)for(j=0;ot[j]&&ot[j]!=' ';++j)if(ot[j]>='a'&&ot[j]<='z')ot[j]-=32;strcpy(st,ot);op=ot;sp=st;trace[0]=0;((void(__cdecl*)(char**))(b+0x6900))(&op);strcpy(expected,trace);trace[0]=0;UI_RunMenuScript(&sp);if(strcmp(expected,trace)||(op-ot)!=(sp-st)){fprintf(stderr,"host-action mismatch %s v%.9g n%d capital%d\nO%s\nS%s\n",actions[act],input,n,capital,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original host-action branch comparisons passed (parent partial)\n",cases);return 0;}
