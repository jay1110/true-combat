#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.h"
#include <windows.h>
#include <stdio.h>
extern qboolean Item_EnableShowViaCvar(itemDef_t *,int);
extern qboolean Item_SettingShow(itemDef_t *,qboolean);
extern void trap_Cvar_VariableStringBuffer(const char *,char *,int);
static displayContextDef_t dc;
static unsigned char rawDC[0x200],rawItem[0x300];
static char events[2048],value[1024];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void get(const char *key,char *out,int size){sprintf(events+strlen(events),"get:%s:%d;",key,size);Q_strncpyz(out,value,size);}
static void config(int id,char *out,int size){sprintf(events+strlen(events),"config:%d:%d;",id,size);Q_strncpyz(out,value,size);}
int main(int argc,char **argv){unsigned char *b;HMODULE h;itemDef_t item;int bits,flags,v,f,s,a,c,n,cases=0;char expected[2048];
 const char *lists[]={NULL,"","0","1 ; 2","; ; 3 ;","\"Ab C\" ; YES ; 0",";","-1 ; 0 ; 2"};
 const char *values[]={"","0","1","2","3","ab c","yes","-1","anything"};
 const char *keys[]={NULL,"","authLevel"};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 dc.getCVarString=get;dc.getConfigString=config;Init_Display(&dc);String_Init();
 *(void **)(rawDC+0x70)=get;*(void **)(rawDC+0x110)=config;
#ifdef CGAMEDLL
 *(void **)(b+0x181915c)=rawDC;patch(b+0x64c30,get);
#define ENABLE_RVA 0x825b0
#define SETTING_RVA 0x826e0
#else
 *(void **)(b+0x245fa4)=rawDC;patch(b+0x20570,get);
#define ENABLE_RVA 0x17930
#define SETTING_RVA 0x17a60
#endif
 patch(trap_Cvar_VariableStringBuffer,get);
 memset(&item,0,sizeof(item));
 for(n=0;n<8;++n)for(v=0;v<9;++v)for(s=0;s<3;++s)for(flags=0;flags<16;++flags)for(f=0;f<5;++f){
  item.enableCvar=lists[n];item.cvarTest=keys[s];item.cvarFlags=flags;
  *(const char **)(rawItem+0x11c)=lists[n];*(const char **)(rawItem+0x118)=keys[s];*(int *)(rawItem+0x120)=flags;strcpy(value,values[v]);
  events[0]=0;a=((int(__cdecl *)(void *,int))(b+ENABLE_RVA))(rawItem,1<<f)&255;strcpy(expected,events);events[0]=0;c=Item_EnableShowViaCvar(&item,1<<f);
  if(a!=c||strcmp(expected,events)){printf("enable mismatch %d %d %d %d %d: %d %d\n%s\n%s\n",n,v,s,flags,f,a,c,expected,events);return 1;}++cases;
 }
 /* Every vote bit, combined bits and negative masks; keep nonboolean return
  * values for settingFlags=2 (the original returns the actual masked bits). */
 for(bits=0;bits<20;++bits)for(v=0;v<8;++v)for(flags=0;flags<4;++flags)for(f=0;f<2;++f){
  int mask=bits<17?1<<bits:bits==17?0:bits==18?0x15555:-1;
  int vals[]={0,1,4,256,65536,131071,-1,mask};
  sprintf(value,"%d",vals[v]);item.voteFlag=item.settingTest=mask;item.settingFlags=flags;
  *(int *)(rawItem+0x264)=mask;*(int *)(rawItem+0x25c)=mask;*(int *)(rawItem+0x260)=flags;
  events[0]=0;a=((int(__cdecl *)(void *,int))(b+SETTING_RVA))(rawItem,f);strcpy(expected,events);events[0]=0;c=Item_SettingShow(&item,f);
  if(a!=c||strcmp(expected,events)){printf("setting mismatch %d %d %d %d: %d %d\n%s\n%s\n",bits,v,flags,f,a,c,expected,events);return 1;}++cases;
 }
 events[0]=0;a=((int(__cdecl *)(void *,int))(b+ENABLE_RVA))(NULL,4)&255;c=Item_EnableShowViaCvar(NULL,4);if(a!=c||events[0])return 1;
 printf("%d whole original visibility comparisons passed\n",cases+1);return 0;
}
