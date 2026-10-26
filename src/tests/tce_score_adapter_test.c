#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../cgame/cg_scoreboard.c"
#include "../ui/tce_ui_coordinates.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
extern qboolean tce_uiCoordinates;
static float actual[4];static int calls;
static void fill(float x,float y,float w,float h,const float*c){assert(tce_uiCoordinates);actual[0]=x;actual[1]=y;actual[2]=w;actual[3]=h;TCE_UI_AdjustCoordinates(actual,actual+1,actual+2,actual+3,cgs.glconfig.vidWidth,cgs.glconfig.vidHeight,cgs.screenXScale,cgs.screenYScale);++calls;}
static void text(float x,float y,float sx,float sy,vec4_t c,const char*s,float a,int l,int style,fontInfo_t*f){assert(tce_uiCoordinates&&x==20&&y==48&&sx==.25f&&sy==.25f);++calls;}
static void top(float x,float y,float w,float h,float size){assert(tce_uiCoordinates&&x==15&&y==88&&w==399&&h==17&&size==1);++calls;}
static int board(const tce_scoreboardContext_t*c){assert(tce_uiCoordinates);TCE_ScoreFill(15,35,817,21,colorWhite);TCE_ScoreText(20,48,.25f,colorWhite,"test");TCE_ScoreTopBottom(15,88,399,17,1);return 1;}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;static snapshot_t snap;int i,j,prior,cases=0;int sizes[][2]={{640,480},{960,720},{1280,720},{1920,1080},{1280,1024}};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;cg.snap=&snap;patch(CG_FillRect,fill);patch(CG_Text_Paint_Ext,text);patch(CG_DrawTopBottom_NoScale,top);patch(TCE_CG_DrawScoreboard,board);
for(i=0;i<5;++i)for(prior=0;prior<2;++prior){float expected[4]={15,35,817,21};cgs.glconfig.vidWidth=*(int*)(b+0x257e0c8)=sizes[i][0];cgs.glconfig.vidHeight=*(int*)(b+0x257e0cc)=sizes[i][1];cgs.screenXScale=*(float*)(b+0x257e0e8)=sizes[i][0]/640.f;cgs.screenYScale=*(float*)(b+0x257e0ec)=sizes[i][1]/480.f;((void(__cdecl*)(float*,float*,float*,float*))(b+0x29760))(expected,expected+1,expected+2,expected+3);calls=0;tce_uiCoordinates=prior;assert(CG_DrawScoreboard()==1&&tce_uiCoordinates==prior&&calls==3);for(j=0;j<4;++j){if(actual[j]!=expected[j]){fprintf(stderr,"coordinate mismatch res%d field%d %.9g != %.9g\n",i,j,actual[j],expected[j]);return 1;}}++cases;}
printf("%d native scoreboard adapter original-coordinate comparisons passed\n",cases);FreeLibrary(h);return 0;}
