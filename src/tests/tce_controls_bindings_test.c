#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern int BindingIDFromName(const char *name);
extern displayContextDef_t *DC;
static char events[65536];
static int scenario, ordinal;
static void keys(const char *command,int *first,int *second){
 /* The new action is an accepted extension, tested separately through the
  * real capture/save path. Preserve comparison of all 61 original commands. */
 if(!Q_stricmp(command,"+freelook")){*first=*second=-1;return;}
 int row=ordinal++;
 *first=(scenario&1)?-1:(scenario&2)?0:(scenario&4)?300:row+1;
 *second=(scenario&8)?-1:(scenario&16)?*first:127-row;
 if(scenario&32){*first=((row+scenario)%5==0)?-1:row*3;*second=((row+scenario)%3==0)?-1:200-row;}
 sprintf(events+strlen(events),"get[%s]=%d,%d;",command,*first,*second);
}
static void binding(int key,const char *command){sprintf(events+strlen(events),"bind[%d]=%s;",key,command);}
static void execute(int mode,const char *text){sprintf(events+strlen(events),"exec[%d]=%s;",mode,text);}
int main(int argc,char **argv){
 HMODULE h;unsigned char *b;static unsigned char rawDC[0x1000];displayContextDef_t dc;
 int i,j,restart,expected,cases=0;char saved[65536],query[128];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 memset(&dc,0,sizeof(dc));dc.getKeysForBinding=keys;dc.setBinding=binding;dc.executeText=execute;DC=&dc;
 *(void **)(rawDC+0xc0)=keys;*(void **)(rawDC+0xc8)=binding;*(void **)(rawDC+0xcc)=execute;*(void **)(b+0x245fa4)=rawDC;
 for(scenario=0;scenario<64;++scenario)for(restart=-1;restart<=1;++restart){
  ordinal=0;events[0]=0;((void(__cdecl *)(void))(b+0x1bb50))();if(ordinal!=61)return 3;strcpy(saved,events);
  ordinal=0;events[0]=0;Controls_GetConfig();if(ordinal!=61||strcmp(saved,events)){puts("get config mismatch");return 4;}++cases;
  events[0]=0;((void(__cdecl *)(int))(b+0x1bb90))(restart);strcpy(saved,events);
  events[0]=0;Controls_SetConfig(restart);if(strcmp(saved,events)){printf("set mismatch scenario%d restart%d\n",scenario,restart);return 5;}++cases;
 }
 for(i=0;i<61;++i)for(j=0;j<3;++j){
  const char *name=*(const char **)(b+0x37a08+i*32);Q_strncpyz(query,name,sizeof(query));
  if(j==1)Q_strupr(query);else if(j==2)Q_strcat(query,sizeof(query),"_unknown");
  expected=((int(__cdecl *)(const char *))(b+0x1bc50))(query);
  if(BindingIDFromName(query)!=expected){printf("binding lookup mismatch %s\n",query);return 6;}++cases;
 }
 if(BindingIDFromName(NULL)!=((int(__cdecl *)(const char *))(b+0x1bc50))(NULL))return 7;++cases;
 if(BindingIDFromName("")!=((int(__cdecl *)(const char *))(b+0x1bc50))(""))return 8;++cases;
 if(BindingIDFromName("+firemode")!=60 || BindingIDFromName("toggleobjectivedesc")!=59 || BindingIDFromName("+mapexpand")!=-1)return 9;
 if(BindingIDFromName("+freelook")!=61)return 10;
 printf("%d UI binding comparisons passed for 61 original commands; +freelook extension checked separately\n",cases);return 0;
}
