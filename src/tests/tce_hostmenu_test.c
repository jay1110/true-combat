#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
extern void UI_RunMenuScript(char **args);
extern void UI_DrawMapPreview(rectDef_t *,float,vec4_t,qboolean);
static char events[4096];static int selected,current,missing;
static void setSink(const char *n,const char *v){strcat(events,n);strcat(events,"=");strcat(events,v);strcat(events,";");}
static float getSink(const char *n){return (float)(!Q_stricmp(n,"ui_netGameType")?selected:current);}
static void valueSink(const char *n,float v){char b[64];sprintf(b,"%g",v);setSink(n,b);}
static void cmdSink(int mode,const char *s){char b[32];sprintf(b,"cmd%d:",mode);strcat(events,b);strcat(events,s);}
static int shaderSink(const char *s){strcat(events,s);strcat(events,";");return missing && strcmp(s,"levelshots/unknownmap")?0:73;}
static void drawSink(float x,float y,float w,float h,int shader){char b[128];sprintf(b,"draw:%g,%g,%g,%g,%d;",x,y,w,h,shader);strcat(events,b);}
static void patch(void *addr,void *fn){DWORD old,tmp;unsigned char *p=addr;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){
 unsigned char *b;HMODULE h;int i,j,k,cases=0;char expected[4096];int modes[]={2,5,7};vec4_t color={1,1,1,1};rectDef_t rect={432,80,270,180};
 if(argc!=2 || !(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x20530,setSink);patch(trap_Cvar_Set,setSink);patch(b+0x20550,getSink);patch(trap_Cvar_VariableValue,getSink);
 patch(b+0x205d0,cmdSink);patch(trap_Cmd_ExecuteText,cmdSink);
 patch(b+0x20590,valueSink);patch(trap_Cvar_SetValue,valueSink);
 for(i=0;i<3;i++)for(j=0;j<3;j++){
  char originalText[]="RunTCEMod",portText[]="RunTCEMod",*p=originalText;
  selected=modes[i];current=modes[j];events[0]=0;((void (__cdecl *)(char **))(b+0x6900))(&p);strcpy(expected,events);
  p=portText;events[0]=0;UI_RunMenuScript(&p);if(strcmp(expected,events)){printf("switch mismatch %s / %s\n",expected,events);return 1;}++cases;
 }
 patch(b+0x20730,shaderSink);patch(trap_R_RegisterShaderNoMip,shaderSink);patch(b+0x26f0,drawSink);patch(UI_DrawHandlePic,drawSink);
 uiInfo.mapCount=*(int *)(b+0x2b52a8)=6;
 for(i=0;i<6;i++){
  static char names[6][32];sprintf(names[i],"obj_test%d",i);
  uiInfo.mapList[i].mapLoadName=names[i];*(char **)(b+0x2b52b0+i*0x88)=names[i];
 }
 for(i=0;i<6;i++)for(j=0;j<3;j++)for(k=0;k<2;k++){
  missing=k;ui_netGameType.integer=*(int *)(b+0x2dc50c)=modes[j];
  ui_currentNetMap.integer=*(int *)(b+0x291f0c)=i;
  events[0]=0;((void (__cdecl *)(rectDef_t *,float,vec4_t,int))(b+0x5590))(&rect,.2f,color,1);strcpy(expected,events);
  events[0]=0;UI_DrawMapPreview(&rect,.2f,color,qtrue);
  if(strcmp(expected,events)){printf("preview mismatch %s / %s\n",expected,events);return 1;}++cases;
 }
 for(i=0;i<6;i++)for(j=0;j<3;j++){
  char text1[]="StartServer",text2[]="StartServer",*p=text1;
  current=0;ui_dedicated.integer=*(int *)(b+0x2d9fec)=0;
  ui_netGameType.integer=*(int *)(b+0x2dc50c)=modes[j];
  ui_currentNetMap.integer=*(int *)(b+0x291f0c)=i;
  ui_mapIndex.integer=*(int *)(b+0x29184c)=0;
  events[0]=0;((void (__cdecl *)(char **))(b+0x6900))(&p);strcpy(expected,events);
  events[0]=0;p=text2;UI_RunMenuScript(&p);
  if(strcmp(expected,events)){printf("host launch mismatch %s / %s\n",expected,events);return 1;}++cases;
 }
 {
  extern void Script_SetMenuItemColor(itemDef_t *,qboolean *,char **);
  char script[]="missing_menu item backcolor 0 0 0 1 ;",*p=script;
  const char *next; qboolean aborted=qfalse;
  Script_SetMenuItemColor(NULL,&aborted,&p);
  if(!String_Parse(&p,&next) || strcmp(next,";"))return 1;
 }
 printf("%d production host-menu/original comparisons and missing-menu parser regression passed\n",cases);return 0;
}
