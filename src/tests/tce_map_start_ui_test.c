#define WIN32_LEAN_AND_MEAN
#define NOGDI
/* Include the production file to exercise its static mapinfo parser. */
#include "../ui/ui_gameinfo.c"
#include <windows.h>
#include <stdio.h>
extern void UI_LoadPanel_RenderHeaderText(panel_button_t *);
static char tokens[256][1024],events[8192],fileData[8192];static int count,at,missing,state,download,freed;
static void patch(void *p0,void *fn){DWORD old,tmp;unsigned char *p=p0;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void tokenize(const char *s){char *p=fileData,*t;strcpy(fileData,s);count=0;while(*(t=COM_Parse(&p))){strcpy(tokens[count++],t);}at=0;}
static int load(const char *path){at=0;strcat(events,path);strcat(events,";");return missing?0:23;}
static int readToken(int h,pc_token_t *t){if(at>=count)return 0;memset(t,0,sizeof(*t));strcpy(t->string,tokens[at++]);return 1;}
static int freeSource(int h){++freed;return 0;}
static qboolean stringParse(int h,const char **s){if(at>=count)return qfalse;*s=tokens[at++];return qtrue;}
static qboolean intParse(int h,int *v){if(at>=count)return qfalse;*v=atoi(tokens[at++]);return qtrue;}
static qboolean floatParse(int h,float *v){if(at>=count)return qfalse;*v=(float)atof(tokens[at++]);return qtrue;}
static void print(const char *s){strcat(events,s);}
static int list(const char *path,const char *ext,char *out,int size){if(strcmp(path,"scripts")||strcmp(ext,".mapinfo")||size!=1024)exit(8);memcpy(out,"a.mapinfo\0b.mapinfo\0",20);return 2;}
static void getState(uiClientState_t *s){memset(s,0,sizeof(*s));s->connState=state;}
static void var(const char *name,char *out,int size){if(strcmp(name,"cl_downloadName"))exit(9);Q_strncpyz(out,download?"pak3.pk3":"",size);}
static void panel(panel_button_t *b){strcat(events,b->text);}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;char expected[8192],body[2048];mapInfo maps[128];int i,j,n=0,ec,ef,official;panel_button_t button={0};
 const char *names[]={"obj_stadtrand","OBJ_DELTA","obj_railhouse","obj_northport","obj_village","obj_snow","obj_hideout","obj_custom","radar",""};
 const char *tails[]={"type \"obj bc robj\" }","type obj }","type bc }","type robj }","}","timelimit", "} unexpected", "} { map obj_custom longname Custom type bc }"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;if(sizeof(mapInfo)!=0x88)return 3;
 patch(b+0x20d90,load);patch(trap_PC_LoadSource,load);patch(b+0x20dd0,readToken);patch(trap_PC_ReadToken,readToken);
 patch(b+0x20db0,freeSource);patch(trap_PC_FreeSource,freeSource);patch(b+0x1370,stringParse);patch(PC_String_Parse,stringParse);
 patch(b+0x12d0,intParse);patch(PC_Int_Parse,intParse);patch(b+0x11e0,floatParse);patch(PC_Float_Parse,floatParse);
 patch(b+0x204a0,print);patch(trap_Print,print);patch(b+0x20690,list);patch(trap_FS_GetFileList,list);
 for(i=0;i<10;i++){
  official=((int (__cdecl *)(const char *))(b+0x16b0))(names[i]);if(official!=BG_MapIsOfficial(names[i]))return 4;++n;
  for(j=0;j<8;j++)for(missing=0;missing<2;missing++){
   sprintf(body,"{ map \"%s\" longname Name briefing Brief lmsbriefing LMS timelimit 21 axisrespawntime 5 alliedrespawntime 7 mapposition_x 2.25 mapposition_y -3.5 %s",names[i],tails[j]);
   tokenize(body);*(int *)(b+0x2b52a8)=0;memset(b+0x2b52ac,0,sizeof(maps));events[0]=0;freed=0;
   ((void (__cdecl *)(char *))(b+0x28f0))("scripts/test.mapinfo");strcpy(expected,events);ec=*(int *)(b+0x2b52a8);ef=freed;memcpy(maps,b+0x2b52ac,sizeof(maps));
   uiInfo.mapCount=0;memset(uiInfo.mapList,0,sizeof(maps));events[0]=0;freed=0;UI_LoadArenasFromFile("scripts/test.mapinfo");
   if(strcmp(expected,events)||ec!=uiInfo.mapCount||ef!=freed||memcmp(maps,uiInfo.mapList,sizeof(maps))){fprintf(stderr,"map parser mismatch %s/%d missing%d count%d/%d\n%s\n%s\n",names[i],j,missing,ec,uiInfo.mapCount,expected,events);return 1;}++n;
  }
 }
 missing=0;tokenize("{ map obj_delta longname Delta type bc }");
 memset(b+0x2b52ac,0,sizeof(maps));events[0]=0;freed=0;((void (__cdecl *)(void))(b+0x2820))();strcpy(expected,events);ec=*(int *)(b+0x2b52a8);ef=freed;memcpy(maps,b+0x2b52ac,sizeof(maps));
 memset(uiInfo.mapList,0,sizeof(maps));uiInfo.mapCount=42;events[0]=0;freed=0;UI_LoadArenas();
 if(strcmp(expected,events)||ec!=uiInfo.mapCount||ef!=freed||memcmp(maps,uiInfo.mapList,sizeof(maps)))return 5;++n;
 patch(b+0x20ab0,getState);patch(trap_GetClientState,getState);patch(b+0x20570,var);patch(trap_Cvar_VariableStringBuffer,var);patch(b+0x20280,panel);patch(BG_PanelButtonsRender_Text,panel);
 for(state=-1;state<12;state++)for(download=0;download<2;download++){
  events[0]=0;((void (__cdecl *)(panel_button_t *))(b+0x38c0))(&button);strcpy(expected,events);
  events[0]=0;UI_LoadPanel_RenderHeaderText(&button);if(strcmp(expected,events))return 6;++n;
 }
 printf("%d complete UI map/parser/loading comparisons passed\n",n);return 0;
}
