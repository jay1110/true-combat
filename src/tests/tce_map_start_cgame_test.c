#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../ui/ui_shared.h"
#include <windows.h>
#include <stdio.h>
extern displayContextDef_t *DC;
static char events[4096];static int present;
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static const char *campaign(void){return present?"Test Campaign":NULL;}
static int width(const char *s,float scale,int limit,fontInfo_t *font){char data[256];sprintf(data,"width:%s,%g,%d,%p;",s,scale,limit,font);strcat(events,data);return (int)(strlen(s)*scale*23);}
static void paint(float x,float y,float sx,float sy,vec4_t color,const char *s,float adj,int limit,int style,fontInfo_t *font){char data[512];sprintf(data,"paint:%g,%g,%g,%g,%g,%g,%g,%g,%s,%g,%d,%d,%p;",x,y,sx,sy,color[0],color[1],color[2],color[3],s,adj,limit,style,font);strcat(events,data);}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;char expected[4096],context[512]={0};int type,n,k,cases=0;
 panel_button_t button={0};panel_button_text_t font={0};displayContextDef_t dc={0};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
 patch(b+0x20910,width);patch(CG_Text_Width_Ext,width);patch(b+0x20ba0,paint);patch(CG_Text_Paint_Ext,paint);
 *(void **)(context+0x100)=campaign;*(void **)(b+0x181915c)=context;dc.nameForCampaign=campaign;DC=&dc;
 button.font=&font;font.font=(fontInfo_t *)0x12345678;
 for(type=-1;type<10;type++)for(n=0;n<8;n++)for(present=0;present<2;present++){
  cgs.gametype=*(int *)(b+0x257e100)=type;cgs.arenaInfoLoaded=*(int *)(b+0x3fc3164)=n&1;
  cgs.currentCampaignMap=*(int *)(b+0x3fb7818)=n-2;cgs.campaignData.mapCount=*(int *)(b+0x3fc314c)=n+1;
  strcpy(cgs.arenaData.longname,"Northport (Liverpool)");strcpy((char *)b+0x3fc4168,cgs.arenaData.longname);
  strcpy(cgs.arenaData.authors,n&2?"Author One & Two":"");strcpy((char *)b+0x3fc41e8,cgs.arenaData.authors);
  button.rect.x=n*13.25f;button.rect.y=110-n*.5f;button.rect.w=750-n*23.0f;
  font.scalex=.1f+n*.03125f;font.scaley=.2f+n*.0625f;font.style=n;for(k=0;k<4;k++)font.colour[k]=(n+k)*.125f;
  events[0]=0;((void (__cdecl *)(panel_button_t *))(b+0x44210))(&button);strcpy(expected,events);events[0]=0;CG_LoadPanel_RenderCampaignTypeText(&button);
  if(strcmp(expected,events)){fprintf(stderr,"type mismatch %d/%d\n%s\n%s\n",type,n,expected,events);return 1;}++cases;
  events[0]=0;((void (__cdecl *)(panel_button_t *))(b+0x44270))(&button);strcpy(expected,events);events[0]=0;CG_LoadPanel_RenderCampaignNameText(&button);
  if(strcmp(expected,events)){fprintf(stderr,"name mismatch %d/%d\n%s\n%s\n",type,n,expected,events);return 1;}++cases;
 }
 printf("%d complete loading text callback comparisons passed\n",cases);return 0;
}
