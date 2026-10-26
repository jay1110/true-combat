#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern void UI_DrawMapPreview(rectDef_t *,float,vec4_t,qboolean);
extern int Text_Width(const char *,float,int);
extern void Text_Paint(float,float,float,vec4_t,const char *,float,int,int);
static char events[16384];static int missing,width;
static void append(const char *s){strcat(events,s);strcat(events,";");}
static void set(const char *n,const char *v){append(n);append(v);}
static int shader(const char *s){append(s);return missing && strcmp(s,"levelshots/unknownmap")?0:73;}
static void draw(float x,float y,float w,float h,int shader){char b[256];sprintf(b,"draw %.3f %.3f %.3f %.3f %d",x,y,w,h,shader);append(b);}
static void stretch(float x,float y,float w,float h,float a,float b,float c,float d,int sh){char s[256];sprintf(s,"stretch %.3f %.3f %.3f %.3f %.5f %.5f %.5f %.5f %d",x,y,w,h,a,b,c,d,sh);append(s);}
static void fill(float x,float y,float w,float h,const float *c){char s[256];sprintf(s,"fill %.3f %.3f %.3f %.3f %.2f %.2f %.2f %.2f",x,y,w,h,c[0],c[1],c[2],c[3]);append(s);}
static int textwidth(const char *s,float scale,int limit){append(s);return width;}
static void paint(float x,float y,float scale,vec4_t c,const char *t,float adjust,int limit,int style){char s[256];sprintf(s,"text %.3f %.3f %.3f %.2f %.2f %.2f %.2f %.2f %d %d",x,y,scale,c[0],c[1],c[2],c[3],adjust,limit,style);append(s);append(t);}
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){
 HMODULE h;unsigned char *b;int campaign,net,idx,pos,tw,m,res,i,count=0;char expected[16384];
 const int indices[]={-1,0,1,2,3},modes[]={2,4,5,7};const float positions[]={0,1,200,500,900,1100};
 vec4_t color={.5f,.6f,.7f,.8f};rectDef_t rect={30,40,260,180};
 if(argc!=2 || !(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20730,shader);patch(trap_R_RegisterShaderNoMip,shader);
 patch(b+0x26f0,draw);patch(UI_DrawHandlePic,draw);patch(b+0x20850,stretch);patch(trap_R_DrawStretchPic,stretch);
 patch(b+0x27b0,fill);patch(UI_FillRect,fill);patch(b+0x4270,textwidth);patch(Text_Width,textwidth);
 patch(b+0x48f0,paint);patch(Text_Paint,paint);
 uiInfo.mapCount=*(int *)(b+0x2b52a8)=2;uiInfo.campaignCount=*(int *)(b+0x2b96ac)=2;
 uiInfo.campaignMap=*(int *)(b+0x2d83ac)=123;
 for(campaign=0;campaign<4;++campaign)for(net=0;net<2;++net)for(idx=0;idx<5;++idx)
 for(pos=0;pos<6;++pos)for(tw=0;tw<2;++tw)for(m=0;m<2;++m)for(res=0;res<2;++res){
  missing=m;width=tw?400:40;
  ui_netGameType.integer=*(int *)(b+0x2dc50c)=modes[campaign];
  ui_gameType.integer=*(int *)(b+0x2d926c)=0;uiInfo.gameTypes[0].gtEnum=*(int *)(b+0x2b328c)=modes[campaign];
  ui_currentMap.integer=*(int *)(b+0x2913cc)=indices[idx];
  ui_currentNetMap.integer=*(int *)(b+0x291f0c)=indices[idx];
  uiInfo.uiDC.glconfig.vidWidth=*(int *)(b+0x2b2028)=res?1280:640;
  uiInfo.uiDC.glconfig.vidHeight=*(int *)(b+0x2b202c)=res?720:480;
  uiInfo.uiDC.xscale=*(float *)(b+0x292138)=res?2:1;
  uiInfo.uiDC.yscale=*(float *)(b+0x292134)=res?1.5f:1;
  for(i=0;i<3;++i){
   unsigned char *rm=b+0x2b52ac+i*0x88,*rc=b+0x2b96b0+i*0x74;
   uiInfo.mapList[i].mapName=*(char **)rm="Preview Test";
   uiInfo.mapList[i].mapLoadName=*(char **)(rm+4)="obj_delta";
   uiInfo.mapList[i].mappos[0]=*(float *)(rm+0x70)=positions[pos];
   uiInfo.mapList[i].mappos[1]=*(float *)(rm+0x74)=positions[(pos+2)%6];
   uiInfo.campaignList[i].mapCount=*(int *)(rc+0x14)=pos%3;
   uiInfo.campaignList[i].mapInfos[0]=&uiInfo.mapList[0];*(void **)(rc+0x18)=b+0x2b52ac;
   uiInfo.campaignList[i].mapInfos[1]=&uiInfo.mapList[1];*(void **)(rc+0x1c)=b+0x2b5334;
   uiInfo.campaignList[i].mapTC[0][0]=*(float *)(rc+0x40)=pos?100:0;
   uiInfo.campaignList[i].mapTC[0][1]=*(float *)(rc+0x44)=100;
   uiInfo.campaignList[i].mapTC[1][0]=*(float *)(rc+0x48)=750;
   uiInfo.campaignList[i].mapTC[1][1]=*(float *)(rc+0x4c)=750;
  }
  events[0]=0;((void (__cdecl *)(rectDef_t *,float,vec4_t,int))(b+0x5590))(&rect,.2f,color,net);strcpy(expected,events);
  events[0]=0;UI_DrawMapPreview(&rect,.2f,color,net);
  if(strcmp(expected,events) || ui_currentMap.integer!=*(int *)(b+0x2913cc) || ui_currentNetMap.integer!=*(int *)(b+0x291f0c)){
   printf("preview mode%d net%d idx%d pos%d width%d missing%d res%d\n%s\n%s\n",modes[campaign],net,idx,pos,tw,m,res,expected,events);return 1;}
  ++count;
 }
 printf("%d whole map-preview comparisons, including campaign/map coordinates and bounds\n",count);return 0;
}
