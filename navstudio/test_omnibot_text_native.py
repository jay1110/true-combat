"""Exercise the exact cgame text callbacks with a captured 2D renderer.

No game, actor state, HTTP service, or UI input is used by this regression.
"""
import os
from pathlib import Path
import shutil
import re
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import compile_run
from test_dms_startup_native import function


def program():
    root=(Path(__file__).resolve().parents[1] / "src")
    source=(root/'cgame/cg_omnibot.c').read_text()
    draw=(root/'cgame/cg_draw.c').read_text()
    shared=(root/'game/q_shared.h').read_text()
    fonts=shared[shared.index('#define GLYPH_START 0'):shared.index('#define Square(x)')]
    native_width=function(draw,'CG_Text_Width_Ext')
    native_paint=function(draw,'CG_Text_Paint_Ext').replace('void CG_Text_Paint_Ext(', 'void Actual_CG_Text_Paint_Ext(',1)
    native_char=function(draw,'CG_Text_PaintChar_Ext')
    old_wrappers=function(draw,'CG_Text_Width')+'\n'+function(draw,'CG_Text_Paint')
    callbacks=source[source.index('typedef struct { vec3_t origin;'):source.index('//////////////////////////////////////////////////////////////////////////',source.index('void DrawDebugText('))]
    return r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <vector>
#include <string>
typedef float vec3_t[3];typedef float vec4_t[4];typedef int qboolean;
enum{qtrue=1,qfalse=0,ITEM_TEXTSTYLE_SHADOWED=1};
enum{ITEM_TEXTSTYLE_SHADOWEDMORE=2};
typedef int qhandle_t;
#define MAX_QPATH 64
''' + fonts + r'''
static struct{struct{fontInfo_t limboFont1;}media;}cgs;
static struct{struct{fontInfo_t fonts[6];}Assets;}cgDC;
static int activeFont;
static vec4_t colorBlack={0,0,0,1};static vec4_t g_color_table[32];
#define COLOR_NULL '*'
#define ColorIndex(c) (((c)-'0')&31)
static int Q_strlenInt(const char*s){return (int)strlen(s);}
static int Q_IsColorString(const char*s){return s[0]=='^'&&s[1]&&s[1]!='^';}
static int glyphDraws;
static void CG_AdjustFrom640(float*,float*,float*,float*){}
static void trap_R_SetColor(const float*){}
static void trap_R_DrawStretchPic(float,float,float w,float h,float,float,float,float,int shader){
 if(w>0&&h>0&&shader)++glyphDraws;
}
''' + native_width + '\n' + native_char + '\n' + native_paint + r'''
static const vec3_t vec3_origin={0,0,0};
#define VectorCopy(a,b) memcpy(b,a,sizeof(vec3_t))
#define VectorSet(a,x,y,z) do{a[0]=x;a[1]=y;a[2]=z;}while(0)
#define VectorSubtract(a,b,c) do{c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];}while(0)
static int VectorCompare(const float*a,const float*b){return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];}
static float DotProduct(const float*a,const float*b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static float VectorLengthSquared(const float*a){return DotProduct(a,a);}
static void Q_strncpyz(char*dst,const char*src,int size){strncpy(dst,src,size-1);dst[size-1]=0;}
struct Refdef{vec3_t vieworg,viewaxis[3];float fov_x,fov_y;};
static Refdef frame={{0,0,0},{{1,0,0},{0,1,0},{0,0,1}},90,90};
static struct{int time;Refdef*refdef_current;}cg={100,&frame};
static struct{int integer;}cg_omnibotdrawing={1};
static struct{float value;}cg_omnibot_render_distance={800};
static char InfoText[1024];static int InfoColor,InfoExpires;
struct Paint{float x,y;std::string text;};static std::vector<Paint>paints;
static void CG_Text_Paint_Ext(float x,float y,float sx,float sy,vec4_t color,const char*t,float adjust,int limit,int style,fontInfo_t*font){
 const int before=glyphDraws;Actual_CG_Text_Paint_Ext(x,y,sx,sy,color,t,adjust,limit,style,font);
 if(glyphDraws>before)paints.push_back({x,y,t});
}
''' + old_wrappers + r'''
void OmnibotClearText(void);
void ClearDebugDraw(){OmnibotClearText();}
''' + callbacks + r'''
int main(){
 // Native TCE startup registers limboFont1; the SDK HUD menu font table is
 // zero because CG_LoadMenus("ui/hud.txt") is disabled. Renderer calls above
 // use the actual production metric/paint/character functions, not a text stub.
 cgs.media.limboFont1.glyphScale=1;
 for(int i=0;i<GLYPHS_PER_FONT;++i){glyphInfo_t&g=cgs.media.limboFont1.glyphs[i];g.height=g.imageHeight=20;g.top=16;g.xSkip=g.imageWidth=24;g.glyph=17;}
 assert(cgDC.Assets.fonts[0].glyphScale==0);
 vec4_t white={1,1,1,1};
 CG_Text_Paint(10,20,.22f,white,"old font control",0,0,ITEM_TEXTSTYLE_SHADOWED);
 assert(glyphDraws==0&&paints.empty());assert(CG_Text_Width("old font control",.22f,0)==0);
 vec3_t pos={100,0,0};const char*text="Waypoint 34, Radius 25, UID 39\nName: Front Door Switch";
 for(int i=0;i<1000;i++)DrawDebugText(pos,text,2000,0xffffffff);
 assert(omniTextNext==0);assert(omniWaypointText.expires==2101);
 OmnibotRenderDebugText();assert(paints.size()==2);
 assert(paints[0].text=="Waypoint 34, Radius 25, UID 39");
 assert(paints[1].text=="Name: Front Door Switch");assert(paints[1].y-paints[0].y==14);
 assert(paints[0].x==320-((float)CG_Text_Width_Ext(paints[0].text.c_str(),.22f,0,&cgs.media.limboFont1))*.5f);
 cg.time=1500;DrawDebugText(pos,text,2000,0xffffffff);assert(omniTextNext==0);
 paints.clear();cg.time=3502;OmnibotRenderDebugText();assert(paints.empty());
 cg.time=4000;DrawDebugText(NULL,"Selection saved",2000,0xffffffff);
 paints.clear();OmnibotRenderDebugText();assert(paints.size()==1);
 paints.clear();cg.time=5900;OmnibotRenderDebugText();assert(paints.size()==1);
 paints.clear();cg.time=6002;OmnibotRenderDebugText();assert(paints.empty());
 cg.time=7000;pos[0]=1000;DrawDebugText(pos,"too distant",2000,0xffffffff);
 paints.clear();OmnibotRenderDebugText();assert(paints.empty());
 pos[0]=-100;DrawDebugText(pos,"behind camera",2000,0xffffffff);
 paints.clear();OmnibotRenderDebugText();assert(paints.empty());
 DrawDebugText(pos,NULL,2000,0xffffffff);DrawDebugText(NULL,"",2000,0xffffffff);
 cg_omnibotdrawing.integer=0;OmnibotRenderDebugText();assert(omniTextNext==0&&InfoText[0]==0&&InfoExpires==0&&omniWaypointText.text[0]==0);
 // Selection identity is a single live inspector even as native selection,
 // radius/property text, color or the authored point position changes.
 cg_omnibotdrawing.integer=1;cg.time=10000;pos[0]=100;pos[1]=0;
 DrawDebugText(pos,"Waypoint 504, Radius 35, UID 505\n\ntce_node 4313",2000,0xffffffff);
 cg.time=10050;pos[1]=10;
 DrawDebugText(pos,"Waypoint 505, Radius 150.25, UID 1000000000\n\ntce_node 4314",2000,0xffffffff);
 paints.clear();OmnibotRenderDebugText();assert(paints.size()==2);
 assert(paints[0].text=="Waypoint 505, Radius 150.25, UID 1000000000");
 assert(paints[1].text=="tce_node 4314");assert(omniTextNext==0);
 for(int i=0;i<1000;++i){pos[1]=i*.001f;DrawDebugText(pos,"Waypoint 505, Radius 150.25, UID 1000000000\nName: changed",2000,0xff0000ff);}
 assert(omniTextNext==0&&omniWaypointText.color==int(0xff0000ff));
 assert(omniWaypointText.origin[1]==pos[1]&&strstr(omniWaypointText.text,"Name: changed"));
 // Three full ring wraps of distinct world labels cannot evict selection.
 vec3_t distant={1000,0,0};char label[96];cg.time=10070;
 for(int i=0;i<768;++i){sprintf(label,"CAMP_world_%d\nActive: %d",i,i&1);DrawDebugText(distant,label,2000,0xffffffff);}
 assert(omniTextNext==0);assert(strstr(omniWaypointText.text,"UID 1000000000"));
 DrawDebugText(distant,"Waypoint incomplete generic label",2000,0xffffffff);
 assert(omniTextNext==1);assert(strstr(omniWaypointText.text,"UID 1000000000"));
 // Render at client cadence between server updates: one unchanged block,
 // with its original requested lifetime, never a timeout extension.
 for(int time=10070;time<=12050;time+=16){cg.time=time;paints.clear();OmnibotRenderDebugText();assert(paints.size()==2);assert(paints[0].text=="Waypoint 505, Radius 150.25, UID 1000000000");}
 cg.time=12052;paints.clear();OmnibotRenderDebugText();assert(paints.empty());
 cg.time=13000;DrawDebugText(pos,"Waypoint 4, Radius 35, UID 5",0,0xffffffff);
 paints.clear();OmnibotRenderDebugText();assert(paints.size()==1);
 cg.time=13002;paints.clear();OmnibotRenderDebugText();assert(paints.empty());
 cg_omnibotdrawing.integer=0;OmnibotRenderDebugText();assert(!omniWaypointText.text[0]&&!InfoText[0]);
 puts("Exact cgame callbacks and real native glyph renderer: font, selection replacement, moving/property updates, ring pressure, frame cadence, lifetime, clipping and reset passed.");
 return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class OmniTextNativeTests(TestCase):
    def test_tce_startup_uses_registered_limbo_font_without_sdk_hud_menu_fonts(self):
        source=((Path(__file__).resolve().parents[1] / "src")/'cgame/cg_main.c').read_text()
        load=function(source,'CG_LoadHudMenu')
        uncommented=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/','',load)
        self.assertNotIn('CG_LoadMenus(',uncommented)
        self.assertIn('CG_Text_SetActiveFont( 0 )',uncommented)
        self.assertIn('trap_R_RegisterFont( "ariblk", 27, &cgs.media.limboFont1 )',source)
        self.assertNotIn('Assets.fonts',function(source,'CG_AssetCache'))

    def test_real_cgame_debugtext_refresh_and_lifetime(self):
        compile_run(self,program())


if __name__=='__main__':main()
