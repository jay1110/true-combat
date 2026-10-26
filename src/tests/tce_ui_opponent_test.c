#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static const char *own,*opponent;static char trace[512];
static void get(const char*n,char*out,int size){sprintf(trace+strlen(trace),"G%s;",n);Q_strncpyz(out,!strcmp(n,"ui_teamName")?own:opponent,size);}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int n,a,o,k,i,mode,er,ar,cases=0;const char*names[]={"first","SECOND","third","fourth","missing",""};int keys[]={K_MOUSE1,K_MOUSE2,K_ENTER,K_KP_ENTER,K_ESCAPE,0};char expected[512];
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
 patch(b+0x20570,get);patch(trap_Cvar_VariableStringBuffer,get);patch(b+0x20530,set);patch(trap_Cvar_Set,set);
 for(n=1;n<=4;++n)for(a=0;a<6;++a)for(o=0;o<6;++o)for(k=0;k<6;++k)for(mode=0;mode<3;++mode){
  uiInfo.teamCount=*(int*)(b+0x2b277c)=n;
  for(i=0;i<n;++i)uiInfo.teamList[i].teamName=*(const char**)(b+0x2b2780+i*0x2c)=names[i];own=names[a];opponent=names[o];
  trace[0]=0;er=0;if(mode==0)er=((int(__cdecl*)(int,float*,int))(b+0x112d0))(0,NULL,keys[k]);else ((void(__cdecl*)(void))(b+(mode==1?0x11310:0x11370)))();strcpy(expected,trace);
  trace[0]=0;ar=0;if(mode==0)ar=UI_OpponentName_HandleKey(0,NULL,keys[k]);else if(mode==1)UI_NextOpponent();else UI_PriorOpponent();
  assert(er==ar&&!strcmp(expected,trace));++cases;
 }
 printf("%d complete original opponent controller comparisons passed\n",cases);FreeLibrary(h);return 0;}
