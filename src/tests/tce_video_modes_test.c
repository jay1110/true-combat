#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ui/tce_video_modes.h"
#include <assert.h>
typedef struct {char name[40],value[40];} video_test_cvar_t;
static video_test_cvar_t vars[128];static int count;
static video_test_cvar_t *find(const char *name){int i;for(i=0;i<count;++i)if(!strcmp(name,vars[i].name))return &vars[i];assert(count<128);strcpy(vars[count].name,name);return &vars[count++];}
float trap_Cvar_VariableValue(const char *name){return (float)atof(find(name)->value);}
void trap_Cvar_Set(const char *name,const char *value){strcpy(find(name)->value,value);}
void trap_Cvar_SetValue(const char *name,float value){sprintf(find(name)->value,"%.9g",value);}
void trap_Cvar_VariableStringBuffer(const char *name,char *buffer,int size){snprintf(buffer,size,"%s",find(name)->value);}
static unsigned char *base;
static int parse(char **input,char **out){*out=*input;return 1;}
static void hook(int offset,void *fn){DWORD old,tmp;unsigned char *p=base+offset;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void original(const char *script){((void (__cdecl *)(const char **))(base+0x6900))(&script);}
int main(int argc,char **argv){
 const int expected[][2]={{1024,576},{1280,720},{1360,768},{1600,900},{1920,1080},{1024,640},{1280,800},{1440,900},{1600,1000},{1680,1050},{1920,1200}};
 int mode,old,savedAspect;
 HMODULE dll;const char *keys[]={"r_mode","r_customwidth","r_customheight","cg_aspectMode"};
 int k;float values[4];
 assert(argc==2);dll=LoadLibraryA(argv[1]);assert(dll);base=(unsigned char *)dll;
 hook(0x14cd0,parse);hook(0x20530,trap_Cvar_Set);hook(0x20550,trap_Cvar_VariableValue);hook(0x20570,trap_Cvar_VariableStringBuffer);hook(0x20590,trap_Cvar_SetValue);
 for(mode=3;mode<=22;++mode){
  count=0;memset(vars,0,sizeof(vars));trap_Cvar_SetValue("ui_r_mode",mode);original("systemCvarsApply");
  for(k=0;k<4;++k)values[k]=trap_Cvar_VariableValue(keys[k]);
  original("systemCvarsGet");assert(trap_Cvar_VariableValue("ui_r_mode")==mode);
  assert(TCE_UI_GetVideoMode()==mode);
  count=0;memset(vars,0,sizeof(vars));TCE_UI_ApplyVideoMode(mode);
  for(k=0;k<4;++k)assert(trap_Cvar_VariableValue(keys[k])==values[k]);
 }
 /* Original accepts every integer: <12 passes through, >22 uses mode12.
  * Compare edge inputs against the DLL, including untouched custom values. */
 {
  const int edges[]={-100,-2,-1,0,1,2,11,12,22,23,24,100};
  int e;
  for(e=0;e<(int)(sizeof(edges)/sizeof(edges[0]));++e){
   count=0;memset(vars,0,sizeof(vars));
   trap_Cvar_SetValue("r_customwidth",1234);trap_Cvar_SetValue("r_customheight",789);trap_Cvar_SetValue("cg_aspectMode",7);
   trap_Cvar_SetValue("ui_r_mode",edges[e]);original("systemCvarsApply");
   for(k=0;k<4;++k)values[k]=trap_Cvar_VariableValue(keys[k]);
   count=0;memset(vars,0,sizeof(vars));
   trap_Cvar_SetValue("r_customwidth",1234);trap_Cvar_SetValue("r_customheight",789);trap_Cvar_SetValue("cg_aspectMode",7);
   assert(TCE_UI_ApplyVideoMode(edges[e]));
   for(k=0;k<4;++k)assert(trap_Cvar_VariableValue(keys[k])==values[k]);
  }
 }
 FreeLibrary(dll);

 for(old=3;old<=22;++old)for(mode=3;mode<=22;++mode){
  assert(TCE_UI_ApplyVideoMode(old));savedAspect=(int)trap_Cvar_VariableValue("cg_aspectMode");TCE_UI_SaveVideoMode();assert(TCE_UI_ApplyVideoMode(mode));
  assert(TCE_UI_GetVideoMode()==mode);
  if(mode>=12){assert(trap_Cvar_VariableValue("r_mode")==-1);assert(trap_Cvar_VariableValue("r_customwidth")==expected[mode-12][0]);assert(trap_Cvar_VariableValue("r_customheight")==expected[mode-12][1]);}
  else assert(trap_Cvar_VariableValue("r_mode")==mode);
  TCE_UI_ResetVideoMode();assert(TCE_UI_GetVideoMode()==old);assert(trap_Cvar_VariableValue("cg_aspectMode")==savedAspect);
 }
 trap_Cvar_SetValue("r_mode",-1);trap_Cvar_SetValue("r_customwidth",1234);trap_Cvar_SetValue("r_customheight",789);trap_Cvar_SetValue("r_customaspect",1.25f);
 TCE_UI_SaveVideoMode();TCE_UI_ApplyVideoMode(20);TCE_UI_ResetVideoMode();
 assert(TCE_UI_GetVideoMode()==-1);assert(trap_Cvar_VariableValue("r_customwidth")==1234);assert(trap_Cvar_VariableValue("r_customheight")==789);assert(trap_Cvar_VariableValue("r_customaspect")==1.25f);
 assert(TCE_UI_ApplyVideoMode(23));assert(TCE_UI_GetVideoMode()==12);
 assert(trap_Cvar_VariableValue("r_customwidth")==1024);assert(trap_Cvar_VariableValue("r_customheight")==576);assert(trap_Cvar_VariableValue("cg_aspectMode")==0);
 assert(TCE_UI_ApplyVideoMode(-2));assert(TCE_UI_GetVideoMode()==-2);
 puts("20 original DLL Apply/Get and 12 original edge Apply comparisons; 400 mode transitions plus custom rollback passed.");return 0;
}
