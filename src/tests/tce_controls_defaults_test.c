#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern displayContextDef_t *DC;
extern char *BindingFromName(const char *name);
static char events[65536];
static int first,second,style;
static void keys(const char *name,int *a,int *b){*a=first;*b=second;sprintf(events+strlen(events),"get:%s;",name);}
static void binding(int key,const char *name){sprintf(events+strlen(events),"bind:%d:%s;",key,name);}
static void execute(int mode,const char *text){sprintf(events+strlen(events),"exec:%d:%s;",mode,text);}
static void keyname(int key,char *out,int size){
 char text[128];
 sprintf(text,style==0?"key%d":style==1?"mixed Key %d":style==2?"abcdefghijklmnopqrstuvwxyz%d":"",key);
 Q_strncpyz(out,text,size);sprintf(events+strlen(events),"name:%d:%d;",key,size);
}
static char *translate(const char *text){sprintf(events+strlen(events),"translate:%s;",text);return style==1?" oder ":style==2?" exceptionally long separator ":" or ";}
int main(int argc,char **argv){
 HMODULE h;unsigned char *b;static unsigned char raw[0x1000];displayContextDef_t dc;
 int left,restart,i,j,cases=0;int values[]={-1,0,1,127,300};char trace[65536],result[64];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 memset(&dc,0,sizeof(dc));DC=&dc;dc.getKeysForBinding=keys;dc.setBinding=binding;dc.executeText=execute;dc.keynumToStringBuf=keyname;dc.translateString=translate;
 *(void **)(raw+0xc0)=keys;*(void **)(raw+0xc8)=binding;*(void **)(raw+0xcc)=execute;*(void **)(raw+0xb8)=keyname;*(void **)(raw+0xac)=translate;*(void **)(b+0x245fa4)=raw;
 for(left=-2;left<=2;++left)for(restart=0;restart<=1;++restart){
  events[0]=0;((void(__cdecl *)(int))(b+0x1bc10))(left);((void(__cdecl *)(int))(b+0x1bb90))(restart);strcpy(trace,events);
  events[0]=0;Controls_SetDefaults(left);Controls_SetConfig(restart);
  if(strcmp(trace,events)){printf("defaults mismatch left%d restart%d\noriginal:%s\nsource:%s\n",left,restart,trace,events);return 3;}++cases;
 }
 for(style=0;style<4;++style)for(i=0;i<5;++i)for(j=0;j<5;++j){
  first=values[i];second=values[j];events[0]=0;strcpy(result,((char *(__cdecl *)(const char *))(b+0x1bca0))("+firemode"));strcpy(trace,events);
  events[0]=0;if(strcmp(result,BindingFromName("+firemode"))||strcmp(trace,events)){printf("label mismatch style%d keys%d,%d\n",style,first,second);return 4;}++cases;
 }
 printf("%d complete defaults/apply and binding-label comparisons passed\n",cases);return 0;
}
