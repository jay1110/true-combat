#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
extern int Multiline_Text_Width(const char*,float,int);
extern int Multiline_Text_Height(const char*,float,int);
extern void Text_SetActiveFont(int);
int main(int argc,char**argv){HMODULE h;byte*b,raw[0x280];itemDef_t item,tip;static displayContextDef_t dc;int k,i;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
 for(k=0;k<256;++k){memset(raw,k,sizeof(raw));memset(&tip,k,sizeof(tip));((void(__cdecl*)(void*))(b+0x14700))(raw);Tooltip_Initialize(&tip);assert(!memcmp(raw,&tip,0xec));}
 /* Reproduce TC global.menu registration: only slot0/1 populated, slot2 empty. */
 memset(&uiInfo,0,sizeof(uiInfo));uiInfo.uiDC.Assets.fonts[0].glyphScale=1;
 for(i=0;i<256;++i){uiInfo.uiDC.Assets.fonts[0].glyphs[i].xSkip=10;uiInfo.uiDC.Assets.fonts[0].glyphs[i].height=20;}
 dc.textFont=Text_SetActiveFont;dc.multiLineTextWidth=Multiline_Text_Width;dc.multiLineTextHeight=Multiline_Text_Height;Init_Display(&dc);
 memset(&item,0,sizeof(item));memset(&tip,0,sizeof(tip));Tooltip_Initialize(&tip);tip.text="Tooltip";item.toolTipData=&tip;item.window.rectClient.x=10;item.window.rectClient.y=20;item.window.rectClient.w=30;item.window.rectClient.h=12;
 Tooltip_ComputePosition(&item);assert(tip.font==0&&uiInfo.activeFont==0);assert(tip.window.rectClient.w>6&&tip.window.rectClient.h>9);assert(tip.window.rectClient.w==20&&tip.window.rectClient.h==13);
 assert(FreeLibrary(h));puts("256 original tooltip initializations and real populated-font dimensions passed");return 0;}
