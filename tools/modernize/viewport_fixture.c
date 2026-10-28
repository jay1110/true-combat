/* Exact production fit/projection and UI coordinate wrappers with controlled
 * cvar/display services. This does not simulate OS cursor capture or pixels. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>
#include "src/ui/tce_viewport.h"
#include "src/ui/tce_ui_coordinates.h"
#include "src/cgame/cg_hud_viewport.h"
typedef int qboolean;
typedef int qhandle_t;
typedef float vec4_t[4];
typedef struct { int integer; float value; } vmCvar_t;
#define qtrue 1
#define qfalse 0
#define SCREEN_HEIGHT 480
#define CVAR_ARCHIVE 1
typedef struct { struct { int vidWidth, vidHeight; } glconfig; float xscale,yscale; int cursorx,cursory,whiteShader; float (*getCVarValue)(const char *); } displayContextDef_t;
typedef struct { struct { const char *name; } window; } menuDef_t;
static menuDef_t *focusedMenu,*tceMenuViewportPaintMenu;
static menuDef_t *Menu_GetFocused(void) { return focusedMenu; }
static int Q_stricmp(const char *a,const char *b)
{ while(*a&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){++a;++b;}return tolower((unsigned char)*a)-tolower((unsigned char)*b); }
static struct { displayContextDef_t uiDC; } uiInfo;
static displayContextDef_t *DC=&uiInfo.uiDC;
static float enabled=1,menuScaleSetting=1,menuMaxSetting=1080;
static int modernEnabled;
static qboolean TCE_ModernUIEnabled(void) { return modernEnabled; }
static float hudScaleSetting=.85f,hudMaxSetting=1080;
static struct { struct { int vidWidth,vidHeight; } glconfig; float screenXScale,screenYScale;struct { int whiteShader,disconnectIcon,lagometerShader; } media;int eventHandling,cursorX,cursorY,cursorUpdate,localServer; } cgs;
static displayContextDef_t cgDC;
typedef struct { struct { int pm_type,commandTime; } ps; } testSnapshot_t;
static testSnapshot_t snapshot;
static struct { int time,showScores,demoPlayback,serverRespawning;struct { int pm_type; } predictedPlayerState;testSnapshot_t *snap; } cg;
typedef struct { int serverTime; } usercmd_t;
#define CMD_BACKUP 64
#define BIGCHAR_WIDTH 16
#define LAG_SAMPLES 128
#define MAX_LAGOMETER_PING 900
#define MAX_LAGOMETER_RANGE 300
#define SNAPFLAG_RATE_DELAYED 1
static vmCvar_t cg_timescale,cg_lagometer,cg_nopredict;
static struct { int frameCount,snapshotCount;float frameSamples[LAG_SAMPLES],snapshotSamples[LAG_SAMPLES];int snapshotFlags[LAG_SAMPLES]; } lagometer;
static vec4_t colorYellow={1,1,0,1},colorBlue={0,0,1,1},colorGreen={0,1,0,1},colorRed={1,0,0,1};
static float labelX,labelY,labelW,labelH;
static char labelText[64];
static int commandTime;
enum { CGAME_EVENT_NONE,CGAME_EVENT_SPEAKEREDITOR,CGAME_EVENT_GAMEVIEW,CGAME_EVENT_CAMPAIGNBREIFING,CGAME_EVENT_FIRETEAMMSG,CGAME_EVENT_DEMO };
enum { PM_NORMAL,PM_SPECTATOR,PM_INTERMISSION };
#define KEYCATCH_CGAME 4
static int speakerCalls,debriefCalls,catcher=KEYCATCH_CGAME;
static qboolean tce_uiCoordinates;
static struct { float x,y,w,h; } drawnRects[16];
static int drawnCount;
static vmCvar_t cg_aspectMode,*bound[5];
static int checks,failures,registrations,updates,mouseX,mouseY,mouseCalls;
#define CHECK(c) do { ++checks;if(!(c)){++failures;printf("FAIL %i: %s\n",__LINE__,#c);} } while(0)
static int near(float a,float b) { return fabsf(a-b)<.003f; }
typedef struct { float advance; } fontInfo_t;
static fontInfo_t regularFont={16},strongFont={18};
static char paintedText[2048];
static float paintedScale,paintedX,paintedY;
static int textPaintCalls,imageMode;
static fontInfo_t *TCE_ModernFont(int strong) { return strong?&strongFont:&regularFont; }
static int Text_Width_Ext(const char *text,float scale,int limit,fontInfo_t *font)
{
    int count=0;(void)limit;
    while(*text){if(text[0]=='^'&&text[1])text+=2;else{++count;++text;}}
    return (int)(count*scale*font->advance);
}
static int CG_Text_Width_Ext(const char *text,float scale,int limit,fontInfo_t *font)
{ return Text_Width_Ext(text,scale,limit,font); }
static void Q_strncpyz(char *dest,const char *source,int length)
{ if(length>0){strncpy(dest,source,length-1);dest[length-1]=0;} }
static void TCE_ModernThemeColor(const float *source,float *out,int surface)
{ (void)surface;memcpy(out,source,sizeof(vec4_t)); }
static void Text_Paint_Ext(float x,float y,float sx,float sy,const float *color,const char *text,float adjust,int limit,int style,fontInfo_t *font)
{
    (void)color;(void)adjust;(void)limit;(void)style;(void)font;CHECK(sx==sy);
    paintedX=x;paintedY=y;paintedScale=sx;Q_strncpyz(paintedText,text,sizeof(paintedText));++textPaintCalls;
}
static void CG_Text_Paint_Ext(float x,float y,float sx,float sy,const float *color,const char *text,float adjust,int limit,int style,fontInfo_t *font)
{ Text_Paint_Ext(x,y,sx,sy,color,text,adjust,limit,style,font); }
static void TCE_ModernImage(const char *name,float x,float y,float w,float h);
static float trap_Cvar_VariableValue(const char *name)
{ if(!strcmp(name,"cg_wideScreen"))return enabled;if(!strcmp(name,"ui_menuScale"))return menuScaleSetting;if(!strcmp(name,"ui_menuMaxHeight"))return menuMaxSetting;return 0; }
#if defined(_WIN32) && !defined(Q3_VM)
static double CG_Cvar_Get(const char *name) { return trap_Cvar_VariableValue(name); }
#else
static float CG_Cvar_Get(const char *name) { return trap_Cvar_VariableValue(name); }
#endif
static void trap_Cvar_Update(vmCvar_t *var)
{
    if(var==bound[0])var->value=enabled;
    else if(var==bound[1])var->value=hudScaleSetting;
    else if(var==bound[2])var->value=hudMaxSetting;
    else if(var==bound[3])var->value=menuScaleSetting;
    else if(var==bound[4])var->value=menuMaxSetting;
    else CHECK(0);
    var->integer=(int)var->value;++updates;
}
static void trap_Cvar_Register(vmCvar_t *var,const char *name,const char *value,int flags)
{
    int n=!strcmp(name,"cg_wideScreen")?0:!strcmp(name,"cg_hudScale")?1:!strcmp(name,"cg_hudMaxHeight")?2:!strcmp(name,"ui_menuScale")?3:!strcmp(name,"ui_menuMaxHeight")?4:-1;
    CHECK(n>=0&&flags==CVAR_ARCHIVE);if(n<0)return;
    CHECK(!strcmp(value,n==0||n==3?"1":n==1?"0.85":"1080"));bound[n]=var;++registrations;trap_Cvar_Update(var);
}
static int TCE_UI_WrapAdd(int a,int b) { return (int)((unsigned)a+(unsigned)b); }
static int TCE_AddInt32(int a,int b) { return TCE_UI_WrapAdd(a,b); }
static void CG_SpeakerEditorMouseMove_Handling(int x,int y) { (void)x;(void)y;++speakerCalls; }
static void CG_Debriefing_MouseEvent(int x,int y) { (void)x;(void)y;++debriefCalls; }
static int trap_Key_GetCatcher(void) { return catcher; }
static void trap_Key_SetCatcher(int value) { catcher=value; }
static int trap_GetCurrentCmdNumber(void) { return 100; }
static void trap_GetUserCmd(int number,usercmd_t *cmd) { (void)number;cmd->serverTime=commandTime; }
static const char *CG_TranslateString(const char *s) { return s; }
static int CG_DrawStrlen(const char *s) { return (int)strlen(s); }
static int Menu_Count(void) { return 1; }
static void Display_MouseMove(void *p,int x,int y) { (void)p;mouseX=x;mouseY=y;++mouseCalls; }
static void trap_R_SetColor(const float *color) { (void)color; }
static void trap_R_DrawStretchPic(float x,float y,float w,float h,float s1,float t1,float s2,float t2,int shader)
{ (void)s1;(void)t1;(void)s2;(void)t2;(void)shader;CHECK(w>=0&&h>=0);if(drawnCount<16){drawnRects[drawnCount].x=x;drawnRects[drawnCount].y=y;drawnRects[drawnCount].w=w;drawnRects[drawnCount++].h=h;} }
void CG_AdjustFrom640(float *x,float *y,float *w,float *h);
static void CG_DrawBigString(int x,int y,const char *s,float alpha)
{ (void)alpha;labelX=(float)x;labelY=(float)y;labelW=(float)strlen(s)*BIGCHAR_WIDTH;labelH=24;strcpy(labelText,s);CG_AdjustFrom640(&labelX,&labelY,&labelW,&labelH); }

/* ACTUAL_HUD_CONTEXT */

/* ACTUAL_DRAW_ADAPTERS */

/* ACTUAL_FUNCTIONS */

static void TCE_ModernImage(const char *name,float x,float y,float w,float h)
{
    CHECK(name && !strcmp(name,"tce2_ui/background"));imageMode=TCE_ViewportDrawMode();
#ifdef CGAMEDLL
    CG_AdjustFrom640(&x,&y,&w,&h);
#else
    UI_AdjustFrom640(&x,&y,&w,&h);
#endif
    trap_R_DrawStretchPic(x,y,w,h,0,0,1,1,123);
}

static void wrappers(int width,int height)
{
    float x,y,w,h,sx,sy,sw,sh;
    tce_viewport_t view;
    void (*draws[4])(float*,float*,float*,float*)={UI_AdjustFrom640,SharedWindowsAdjust,SharedLinuxAdjust,SharedGenericAdjust};
    int n;
    uiInfo.uiDC.glconfig.vidWidth=width;uiInfo.uiDC.glconfig.vidHeight=height;
    uiInfo.uiDC.xscale=width/640.f;uiInfo.uiDC.yscale=height/480.f;DC->getCVarValue=trap_Cvar_VariableValue;
    CHECK(TCE_ViewportFit(&view,width,height,TCE_VIEWPORT_CANVAS_WIDTH,480,menuMaxSetting,menuScaleSetting,.5f,.5f));
    for(n=0;n<4;++n){
        x=123;y=87;w=72;h=42;sx=x;sy=y;sw=w;sh=h;
        TCE_ViewportProject(&view,&sx,&sy,&sw,&sh);draws[n](&x,&y,&w,&h);
        CHECK(near(x,sx)&&near(y,sy)&&near(w,sw)&&near(h,sh));
        CHECK(near(w/72,h/42));
        /* A button and the rendered cursor share the same affine mapping.
         * The unchanged mouse dispatcher/hit rectangles stay logical. */
        uiInfo.uiDC.cursorx=0;uiInfo.uiDC.cursory=0;mouseCalls=0;
        _UI_MouseEvent(150,100);CHECK(mouseCalls==1&&mouseX==150&&mouseY==100);
        sx=(float)mouseX;sy=(float)mouseY;sw=sh=0;draws[n](&sx,&sy,&sw,&sh);
        CHECK(sx>=x&&sx<=x+w&&sy>=y&&sy<=y+h);
    }
}
static void hudContext(void)
{
    const int sizes[][2]={{800,600},{1280,800},{1280,720},{1920,1080},{3440,1440},{3840,2160}};
    const float anchors[]={0,.5f,1},scales[]={.5f,.85f,1.f,1.5f};
    int size,anchor,tc,scale,cap;float x,y,w,h,left,top,canvasWidth,fullW,fullH,marginX,marginY;
    enabled=1;hudScaleSetting=.85f;hudMaxSetting=1080;registrations=updates=0;
    CG_HudViewportInit();CHECK(registrations==5&&updates==5&&CG_WideScreenEnabled());
    CHECK(near(hudScale.value,.85f)&&near(hudMaxHeight.value,1080));
    cg_aspectMode.integer=2;CHECK(CG_HudAspectMode()==0);
    x=1;y=2;w=3;h=4;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,1)==0&&x==1&&y==2&&w==3&&h==4);
    for(size=0;size<6;++size)for(scale=0;scale<4;++scale)for(cap=0;cap<2;++cap)for(tc=0;tc<2;++tc)for(anchor=0;anchor<3;++anchor){
        cgs.glconfig.vidWidth=sizes[size][0];cgs.glconfig.vidHeight=sizes[size][1];
        hudScaleSetting=scales[scale];hudMaxSetting=cap?1080:0;CG_HudViewportUpdate();
        canvasWidth=tc?TCE_VIEWPORT_CANVAS_WIDTH:640.f;
        CG_HudViewportBegin(anchors[anchor],anchors[anchor]);
        x=y=0;w=canvasWidth;h=480;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,tc)==1);
        left=x;top=y;fullW=w;fullH=h;marginX=sizes[size][0]-w;marginY=sizes[size][1]-h;
        CHECK(near(left,marginX*anchors[anchor])&&near(top,marginY*anchors[anchor]));
        CHECK(near(w/canvasWidth,h/480));
        CHECK(left>=-.003f&&top>=-.003f&&left+w<=sizes[size][0]+.003f&&top+h<=sizes[size][1]+.003f);
        if(cap)CHECK(h<=1080*scales[scale]+.01f);
        x=y=0;w=h=20;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,tc)==1);
        CHECK(near(w,h)&&near(w/20,fullH/480)); /* physical circle remains round */
        CHECK(near(x,left)&&near(y,top));
        CG_HudViewportFull();x=y=0;w=canvasWidth;h=480;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,tc)==1);
        CHECK(x==0&&y==0&&near(w,(float)sizes[size][0])&&near(h,(float)sizes[size][1]));
        CG_HudViewportAnchor(anchors[anchor],anchors[anchor]);x=y=0;w=canvasWidth;h=480;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,tc)==1);
        CHECK(near(x,left)&&near(y,top)&&near(w,fullW)&&near(h,fullH));
        CG_HudViewportEnd();x=1;y=2;w=3;h=4;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,tc)==0&&x==1&&y==2&&w==3&&h==4);
    }
    /* Independent known size contract: 4K capped HUD has .85*1080px height,
     * while a fresh Begin clears a prior fullscreen override/anchor. */
    cgs.glconfig.vidWidth=3840;cgs.glconfig.vidHeight=2160;hudScaleSetting=.85f;hudMaxSetting=1080;CG_HudViewportUpdate();
    CG_HudViewportBegin(1,1);CG_HudViewportFull();CG_HudViewportBegin(0,0);
    x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;CG_HudViewportAdjust(&x,&y,&w,&h,1);
    CHECK(x==0&&y==0&&near(w,1632)&&near(h,918));
    CG_HudViewportBegin(.5f,.5f);x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;CG_HudViewportAdjust(&x,&y,&w,&h,1);
    CHECK(near(x,1104)&&near(y,621)&&near(w,1632)&&near(h,918));
    CG_HudViewportFull();CG_HudViewportInit();x=1;y=2;w=3;h=4;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,1)==0&&x==1&&y==2);
    enabled=0;CG_HudViewportUpdate();CHECK(!CG_WideScreenEnabled()&&CG_HudAspectMode()==2);
    CG_HudViewportBegin(1,1);CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,1)==0&&x==1&&y==2&&w==3&&h==4);
    CG_HudViewportEnd();enabled=1;CG_HudViewportUpdate();
}
static void worldOverlayAndBorders(void)
{
    const int sizes[][2]={{800,600},{1280,800},{1280,720},{1920,1080},{3440,1440},{3840,2160}};
    const float scales[]={.5f,.85f,1.f,1.5f};
    int size,scale,tc,row,col,i,covered;float x,y,w,h,reference[4],left,top,right,bottom,px,py;
    enabled=1;CG_HudViewportInit();
    for(size=0;size<6;++size)for(scale=0;scale<4;++scale)for(tc=0;tc<2;++tc){
        cgs.glconfig.vidWidth=sizes[size][0];cgs.glconfig.vidHeight=sizes[size][1];
        cgs.screenXScale=sizes[size][0]/640.f;cgs.screenYScale=sizes[size][1]/480.f;
        hudScaleSetting=scales[scale];hudMaxSetting=1080;CG_HudViewportUpdate();
        /* This is exactly the pre-HUD portal-scene conversion. The world
         * reticle must retain all four float words at every HUD setting. */
        tce_uiCoordinates=qtrue;CG_HudViewportEnd();
        reference[0]=276.25f;reference[1]=90.5f;reference[2]=reference[3]=300;
        CG_AdjustFrom640(&reference[0],&reference[1],&reference[2],&reference[3]);
        CG_HudViewportBegin(1,1);CG_HudViewportWorld();
        x=276.25f;y=90.5f;w=h=300;CG_AdjustFrom640(&x,&y,&w,&h);
        CHECK(x==reference[0]&&y==reference[1]&&w==reference[2]&&h==reference[3]);
        CHECK(near(w,h));
        tce_uiCoordinates=tc;x=y=100;w=h=20;CG_AdjustFrom640(&x,&y,&w,&h);CHECK(near(w,h));
        if(!tc){x=320;y=240;w=h=0;CG_AdjustFrom640(&x,&y,&w,&h);CHECK(near(x,sizes[size][0]*.5f)&&near(y,sizes[size][1]*.5f));}
        x=y=0;w=tc?TCE_VIEWPORT_CANVAS_WIDTH:640;h=480;CG_AdjustFrom640(&x,&y,&w,&h);
        left=x;top=y;right=x+w;bottom=y+h;drawnCount=0;CG_HudViewportMaskMargins(tc);
        CHECK(drawnCount<=4&&tce_uiCoordinates==tc);
        /* Samples outside the shader's logical rectangle must be masked;
         * no outside world image may leak around a classic scope/binocular. */
        for(row=0;row<9;++row)for(col=0;col<9;++col){
            px=(col+.25f)*sizes[size][0]/9.f;py=(row+.25f)*sizes[size][1]/9.f;
            if(px>=left&&px<=right&&py>=top&&py<=bottom)continue;
            covered=0;for(i=0;i<drawnCount;++i)if(px>=drawnRects[i].x-.001f&&px<=drawnRects[i].x+drawnRects[i].w+.001f&&py>=drawnRects[i].y-.001f&&py<=drawnRects[i].y+drawnRects[i].h+.001f)covered=1;
            CHECK(covered);
        }
    }
    /* 4K uses a capped1.9125px SDK HUD unit, not raw6/4.5px border widths. */
    cgs.glconfig.vidWidth=3840;cgs.glconfig.vidHeight=2160;cgs.screenXScale=6;cgs.screenYScale=4.5f;
    hudScaleSetting=.85f;hudMaxSetting=1080;CG_HudViewportUpdate();CG_HudViewportBegin(.5f,.5f);tce_uiCoordinates=qfalse;
    drawnCount=0;CG_DrawSides(0,0,100,50,1);CHECK(drawnCount==2&&near(drawnRects[0].w,1.9125f));
    drawnCount=0;CG_DrawTopBottom(0,0,100,50,1);CHECK(drawnCount==2&&near(drawnRects[0].h,1.9125f));
    CG_HudViewportWorld();tce_uiCoordinates=qtrue;drawnCount=0;CG_DrawSides(0,0,100,50,1);CHECK(near(drawnRects[0].w,4.5f));
    enabled=0;CG_HudViewportUpdate();CG_HudViewportBegin(0,0);tce_uiCoordinates=qfalse;
    drawnCount=0;CG_DrawSides(0,0,100,50,1);CHECK(drawnRects[0].w==6);
    drawnCount=0;CG_DrawTopBottom(0,0,100,50,1);CHECK(drawnRects[0].h==4.5f);
    CG_HudViewportWorld();drawnCount=0;CG_HudViewportMaskMargins(1);CHECK(drawnCount==0);
    /* The warmup center anchor must differ from the preceding popup anchor. */
    enabled=1;cgs.glconfig.vidWidth=1280;cgs.glconfig.vidHeight=720;CG_HudViewportUpdate();CG_HudViewportBegin(.5f,.5f);tce_uiCoordinates=qtrue;
    x=426;y=194;w=h=0;CG_AdjustFrom640(&x,&y,&w,&h);CHECK(near(x,639.15f)&&near(y,301.35f));
    CG_HudViewportEnd();tce_uiCoordinates=qfalse;
}

static void nativeMenusAndInput(void)
{
    const int sizes[][2]={{640,480},{1024,768},{1280,720},{1920,1080},{2560,1080},{3440,1440},{3840,2160}};
    const float scales[]={.5f,.65f,.85f,1.f,1.25f,1.5f};
    const int events[]={CGAME_EVENT_SPEAKEREDITOR,CGAME_EVENT_GAMEVIEW,CGAME_EVENT_CAMPAIGNBREIFING,CGAME_EVENT_FIRETEAMMSG,CGAME_EVENT_DEMO};
    int size,scale,wide,event;float x,y,w,h,hud[4],expected[4],unitX,unitY;
    tce_viewport_t view;
    modernEnabled=1;
    for(size=0;size<7;++size)for(scale=0;scale<6;++scale)for(wide=0;wide<2;++wide){
        cgs.glconfig.vidWidth=uiInfo.uiDC.glconfig.vidWidth=sizes[size][0];
        cgs.glconfig.vidHeight=uiInfo.uiDC.glconfig.vidHeight=sizes[size][1];
        cgs.screenXScale=uiInfo.uiDC.xscale=sizes[size][0]/640.f;
        cgs.screenYScale=uiInfo.uiDC.yscale=sizes[size][1]/480.f;
        enabled=(float)wide;hudScaleSetting=.5f;hudMaxSetting=1080;
        menuScaleSetting=scales[scale];menuMaxSetting=1080;CG_HudViewportInit();
        tce_uiCoordinates=qtrue;CG_HudViewportBegin(1,1);
        hud[0]=7;hud[1]=8;hud[2]=23;hud[3]=29;CG_AdjustFrom640(&hud[0],&hud[1],&hud[2],&hud[3]);
        CG_MenuViewportBegin();CG_MenuViewportBegin();
        CHECK(TCE_ViewportFit(&view,sizes[size][0],sizes[size][1],TCE_VIEWPORT_CANVAS_WIDTH,480,1080,scales[scale],.5f,.5f));
        x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;
        CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,1));
        CHECK(x>=-.003f&&y>=-.003f&&x+w<=sizes[size][0]+.003f&&y+h<=sizes[size][1]+.003f);
        CHECK(near(x,view.offsetX)&&near(y,view.offsetY)&&near(w,TCE_VIEWPORT_CANVAS_WIDTH*view.scale)&&near(h,480*view.scale));
        /* Menu dimensions are independent of changes to the surrounding HUD,
         * including a fullscreen HUD override. Its old state survives End. */
        expected[0]=x;expected[1]=y;expected[2]=w;expected[3]=h;
        CG_HudViewportFull();hudScaleSetting=1.25f;CG_HudViewportUpdate();
        x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;CG_AdjustFrom640(&x,&y,&w,&h);
        CHECK(near(x,expected[0])&&near(y,expected[1])&&near(w,expected[2])&&near(h,expected[3]));
        CG_MenuViewportEnd();x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;CG_AdjustFrom640(&x,&y,&w,&h);
        CHECK(near(x,expected[0])&&near(y,expected[1])&&near(w,expected[2])&&near(h,expected[3]));
        CG_MenuViewportEnd();
        if(wide){x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;CG_AdjustFrom640(&x,&y,&w,&h);
            CHECK(x==0&&y==0&&near(w,(float)sizes[size][0])&&near(h,(float)sizes[size][1]));}
        CG_HudViewportAnchor(1,1);hudScaleSetting=.5f;CG_HudViewportUpdate();
        x=7;y=8;w=23;h=29;CG_AdjustFrom640(&x,&y,&w,&h);
        CHECK(near(x,hud[0])&&near(y,hud[1])&&near(w,hud[2])&&near(h,hud[3]));
        CG_HudViewportEnd();
        /* Native CGame and UI render the same logical cursor and button in
         * the same physical place. An inverse of that image lands in its
         * original hit rectangle, for small and enlarged menu settings. */
        CG_MenuViewportBegin();x=151;y=109;w=h=0;CG_AdjustFrom640(&x,&y,&w,&h);
        expected[0]=151;expected[1]=109;expected[2]=expected[3]=0;UI_AdjustFrom640(&expected[0],&expected[1],&expected[2],&expected[3]);
        CHECK(near(x,expected[0])&&near(y,expected[1]));
        CHECK(near((x-view.offsetX)/view.scale,151)&&near((y-view.offsetY)/view.scale,109));
        CHECK((x-view.offsetX)/view.scale>=140&&(x-view.offsetX)/view.scale<=180);
        unitX=unitY=1;CHECK(CG_HudViewportDimensions(&unitX,&unitY,1)&&near(unitX,view.scale)&&near(unitY,view.scale));
        CG_MenuViewportEnd();CG_MenuViewportEnd(); /* unmatched End remains safe */
    }
    /* Old no-theme/widescreen-off conversion is still selected, rather than
     * inheriting the active menu preferences or changing border semantics. */
    modernEnabled=0;enabled=0;CG_HudViewportUpdate();CG_MenuViewportBegin();
    x=1;y=2;w=3;h=4;CHECK(!CG_HudViewportAdjust(&x,&y,&w,&h,1)&&x==1&&y==2&&w==3&&h==4);
    CHECK(!CG_HudViewportDimensions(&w,&h,1));CG_MenuViewportEnd();
    enabled=1;menuScaleSetting=1;menuMaxSetting=1080;
    uiInfo.uiDC.glconfig.vidWidth=3840;uiInfo.uiDC.glconfig.vidHeight=2160;uiInfo.uiDC.xscale=6;uiInfo.uiDC.yscale=4.5f;
    drawnCount=0;_UI_DrawSides(0,0,100,50,1);CHECK(drawnCount==2&&near(drawnRects[0].w,2.25f));
    drawnCount=0;_UI_DrawTopBottom(0,0,100,50,1);CHECK(drawnCount==2&&near(drawnRects[0].h,2.25f));
    /* One mouse event followed immediately by a key click must see the new
     * CGame position, not the vmMain pre-event cursor snapshot. */
    cg.snap=&snapshot;cg.time=1234;
    for(event=0;event<5;++event){
        cgs.eventHandling=events[event];cgs.cursorX=140;cgs.cursorY=100;cgDC.cursorx=140;cgDC.cursory=100;
        CG_MouseEvent(11,9);CHECK(cgs.cursorX==151&&cgs.cursorY==109&&cgDC.cursorx==151&&cgDC.cursory==109);
        CHECK(cgDC.cursorx>=150&&cgDC.cursorx<=170&&cgDC.cursory>=105&&cgDC.cursory<=125);
        CG_MouseEvent(9999,9999);CHECK(cgs.cursorX==852&&cgs.cursorY==480&&cgDC.cursorx==852&&cgDC.cursory==480);
        CG_MouseEvent(-9999,-9999);CHECK(cgs.cursorX==0&&cgs.cursorY==0&&cgDC.cursorx==0&&cgDC.cursory==0);
    }
    CHECK(speakerCalls==3&&cgs.cursorUpdate==6234);
    cgs.eventHandling=CGAME_EVENT_NONE;snapshot.ps.pm_type=PM_INTERMISSION;CG_MouseEvent(5,6);CHECK(debriefCalls==1);
    snapshot.ps.pm_type=PM_NORMAL;cg.predictedPlayerState.pm_type=PM_NORMAL;cg.showScores=0;CG_MouseEvent(5,6);CHECK(!(catcher&KEYCATCH_CGAME));
}

static void headerMenusAndHudComposites(void)
{
    const char *names[]={"ingame_main","options","ingame_vote","main","quit","modern_appearance","options_system"};
    menuDef_t menu,underneath;
    const int sizes[][2]={{1024,768},{1280,720},{1920,1080},{2560,1080},{3440,1440}};
    const float scales[]={.5f,.65f,.85f,1.f,1.25f};
    int i,size,scale;float x,y,w,h,expectedY,unit;
    modernEnabled=1;enabled=1;menuScaleSetting=.65f;menuMaxSetting=1080;CG_HudViewportInit();CG_HudViewportEnd();
    uiInfo.uiDC.glconfig.vidWidth=1920;uiInfo.uiDC.glconfig.vidHeight=1080;
    for(i=0;i<7;++i){menu.window.name=names[i];focusedMenu=&menu;
        CHECK(TCE_MenuViewportAnchorY()==(i<3?0.f:.5f));
        x=y=0;w=852;h=480;UI_AdjustFrom640(&x,&y,&w,&h);
        CHECK((i<3&&y==0)||(i>=3&&y>0));
        /* Paint scope controls an underlying hub independently of a focused
         * confirmation; afterward the cursor returns to its dialog mapping. */
        underneath.window.name="ingame_main";tceMenuViewportPaintMenu=&underneath;
        CHECK(TCE_MenuViewportAnchorY()==0);tceMenuViewportPaintMenu=NULL;
        CHECK(TCE_MenuViewportAnchorY()==(i<3?0.f:.5f));
    }
    modernEnabled=0;menu.window.name="ingame_main";CHECK(TCE_MenuViewportAnchorY()==.5f);
    focusedMenu=NULL;modernEnabled=1;CHECK(TCE_MenuViewportAnchorY()==.5f);
    /* Disconnect text belongs to the center group, while the icon/graph keep
     * their right-bottom anchor. Varying scale must not move the text center. */
    cg.snap=&snapshot;cg.time=4096;cg_timescale.value=1;commandTime=4000;snapshot.ps.commandTime=100;
    for(size=0;size<5;++size)for(scale=0;scale<5;++scale){
        cgs.glconfig.vidWidth=sizes[size][0];cgs.glconfig.vidHeight=sizes[size][1];
        cgs.screenXScale=sizes[size][0]/640.f;cgs.screenYScale=sizes[size][1]/480.f;
        hudScaleSetting=scales[scale];hudMaxSetting=1080;CG_HudViewportUpdate();
        tce_uiCoordinates=qtrue;CG_HudViewportBegin(.5f,.5f);
        x=426;y=100;w=h=0;CG_AdjustFrom640(&x,&y,&w,&h);expectedY=y;
        CG_HudViewportAnchor(1,1);drawnCount=0;CG_DrawDisconnect();
        unit=1;x=y=0;w=h=1;CG_HudViewportDimensions(&w,&h,1);unit=w;
        CHECK(!strcmp(labelText,"Connection Interrupted")&&near(labelX+labelW*.5f,sizes[size][0]*.5f-unit*(TCE_VIEWPORT_CANVAS_WIDTH*.5f-426))&&near(labelY,expectedY));
        /* The graph label is already in logical graph coordinates; its new
         * projection shares the graph origin, including size caps/anchors. */
        commandTime=0;cg_lagometer.integer=cg_nopredict.integer=1;cgs.localServer=0;
        x=804;y=280;w=h=48;CG_AdjustFrom640(&x,&y,&w,&h);drawnCount=0;CG_DrawLagometer();
        CHECK(!strcmp(labelText,"snc")&&near(labelX,x)&&near(labelY,y));commandTime=4000;
        CG_HudViewportEnd();
    }
    /* A reduced spectator heading and its following instructions are in the
     * same top-left group; crosshair-name drawing returns to center scope. */
    cgs.glconfig.vidWidth=1920;cgs.glconfig.vidHeight=1080;hudScaleSetting=.5f;CG_HudViewportUpdate();
    CG_HudViewportBegin(.5f,.5f);CG_HudViewportAnchor(0,0);x=8;y=118;w=h=0;CG_AdjustFrom640(&x,&y,&w,&h);
    CHECK(near(x,9)&&near(y,132.75f));CG_HudViewportAnchor(.5f,.5f);x=426;y=240;w=h=0;CG_AdjustFrom640(&x,&y,&w,&h);CHECK(near(x,959.25f)&&near(y,540));
    CG_HudViewportEnd();
}

static void backgroundAndFullscreenScopes(void)
{
    const int sizes[][2]={{1024,768},{1280,720},{1920,1080},{3440,1440},{3840,2160}};
    const float scales[]={.5f,.85f,1.f,1.25f,1.5f};
    const char *backgrounds[]={"background_1","background_black","fadebox_black"};
    void (*adjust[4])(float*,float*,float*,float*)={UI_AdjustFrom640,SharedWindowsAdjust,SharedLinuxAdjust,SharedGenericAdjust};
    menuDef_t menu,focused;
    tce_viewport_t covered,saved;
    float x,y,w,h,expectedScale,expectedX,expectedY;
    int size,scale,n,name,previous,inner;
    modernEnabled=1;enabled=1;tce_uiCoordinates=qtrue;focused.window.name="options_system";focusedMenu=&focused;
    tceMenuViewportPaintMenu=NULL;TCE_ViewportSetDrawMode(TCE_VIEWPORT_MENU);
    for(size=0;size<5;++size)for(scale=0;scale<5;++scale){
        cgs.glconfig.vidWidth=uiInfo.uiDC.glconfig.vidWidth=sizes[size][0];
        cgs.glconfig.vidHeight=uiInfo.uiDC.glconfig.vidHeight=sizes[size][1];
        cgs.screenXScale=uiInfo.uiDC.xscale=sizes[size][0]/640.f;
        cgs.screenYScale=uiInfo.uiDC.yscale=sizes[size][1]/480.f;
        menuScaleSetting=scales[scale];menuMaxSetting=480;hudScaleSetting=1.5f;CG_HudViewportInit();CG_HudViewportEnd();
        expectedScale=fmaxf(sizes[size][0]/TCE_VIEWPORT_CANVAS_WIDTH,sizes[size][1]/480.f);
        CHECK(TCE_ViewportCover(&covered,sizes[size][0],sizes[size][1],TCE_VIEWPORT_CANVAS_WIDTH,480));
        CHECK(near(covered.scale,expectedScale));saved=covered;
        CHECK(!TCE_ViewportCover(&covered,0,sizes[size][1],852,480)&&!memcmp(&covered,&saved,sizeof(saved)));
        CHECK(!TCE_ViewportCover(&covered,sizes[size][0],sizes[size][1],NAN,480)&&!memcmp(&covered,&saved,sizeof(saved)));
        CHECK(!TCE_ViewportCover(&covered,sizes[size][0],sizes[size][1],852,INFINITY)&&!memcmp(&covered,&saved,sizeof(saved)));
        CHECK(!TCE_ViewportCover(NULL,sizes[size][0],sizes[size][1],852,480));
        /* A cover scope fills every physical edge without stretching the image
         * or inheriting the user's reduced menu size/max-height cap. */
        previous=TCE_ViewportSetDrawMode(TCE_VIEWPORT_BACKDROP);CHECK(previous==TCE_VIEWPORT_MENU);
        for(n=0;n<4;++n){
            x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;adjust[n](&x,&y,&w,&h);
            CHECK(x<=.003f&&y<=.003f&&x+w>=sizes[size][0]-.003f&&y+h>=sizes[size][1]-.003f);
            CHECK(near(w/TCE_VIEWPORT_CANVAS_WIDTH,h/480)&&near(h/480,expectedScale));
        }
        x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;CHECK(CG_HudViewportAdjust(&x,&y,&w,&h,1));
        CHECK(x<=.003f&&y<=.003f&&x+w>=sizes[size][0]-.003f&&y+h>=sizes[size][1]-.003f);
        CHECK(TCE_ViewportSetDrawMode(previous)==TCE_VIEWPORT_BACKDROP);
        /* The same cover applies only while the original global background is
         * being painted. Its focused menu must not change controls to cover. */
        for(name=0;name<3;++name){
            menu.window.name=backgrounds[name];tceMenuViewportPaintMenu=&menu;CHECK(TCE_MenuViewportBackground());
            for(n=0;n<4;++n){
                /* These original .menu backdrops are authored852 units wide,
                 * not the853.333-unit native canvas used by ModernBackdrop.
                 * Using the latter as their cover denominator left a thin
                 * unpainted right edge on16:9 displays. */
                x=y=0;w=852;h=480;adjust[n](&x,&y,&w,&h);
                CHECK(near(h/480,fmaxf(sizes[size][0]/852.f,sizes[size][1]/480.f)));
                CHECK(near(w/852,h/480));
                CHECK(x<=.003f&&y<=.003f&&x+w>=sizes[size][0]-.003f&&y+h>=sizes[size][1]-.003f);
                /* Containing all four extreme pixel centers contains every
                 * pixel center in this axis-aligned viewport, without a
                 * multi-million-pixel loop or hiding subpixel edge gaps. */
                CHECK(x<=.5f&&y<=.5f&&x+w>sizes[size][0]-.5f&&y+h>sizes[size][1]-.5f);
            }
        }
        tceMenuViewportPaintMenu=NULL;focused.window.name="background_1";CHECK(!TCE_MenuViewportBackground());
        focused.window.name="options_system";
        expectedScale=fminf(sizes[size][0]/TCE_VIEWPORT_CANVAS_WIDTH,sizes[size][1]/480.f);
        expectedX=(sizes[size][0]-TCE_VIEWPORT_CANVAS_WIDTH*expectedScale)*.5f;
        expectedY=(sizes[size][1]-480*expectedScale)*.5f;
        previous=TCE_ViewportSetDrawMode(TCE_VIEWPORT_FULLSCREEN);CHECK(previous==TCE_VIEWPORT_MENU);
        for(n=0;n<4;++n){
            x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;adjust[n](&x,&y,&w,&h);
            CHECK(near(x,expectedX)&&near(y,expectedY)&&near(w,TCE_VIEWPORT_CANVAS_WIDTH*expectedScale)&&near(h,480*expectedScale));
            CHECK(x>=-.003f&&y>=-.003f&&x+w<=sizes[size][0]+.003f&&y+h<=sizes[size][1]+.003f);
        }
        /* Actual backdrop helper restores a caller's fullscreen scope. Its
         * image adapter is controlled; its scope/setter and projection are not. */
        drawnCount=0;TCE_ModernBackdrop("tce2_ui/background");
        CHECK(TCE_ViewportDrawMode()==TCE_VIEWPORT_FULLSCREEN&&imageMode==TCE_VIEWPORT_BACKDROP&&drawnCount==1);
        CHECK(drawnRects[0].x<=.003f&&drawnRects[0].y<=.003f&&drawnRects[0].x+drawnRects[0].w>=sizes[size][0]-.003f&&drawnRects[0].y+drawnRects[0].h>=sizes[size][1]-.003f);
        inner=TCE_ViewportSetDrawMode(TCE_VIEWPORT_BACKDROP);CHECK(inner==TCE_VIEWPORT_FULLSCREEN);
        CHECK(TCE_ViewportSetDrawMode(inner)==TCE_VIEWPORT_BACKDROP);
        CHECK(TCE_ViewportSetDrawMode(previous)==TCE_VIEWPORT_FULLSCREEN);
        /* Ordinary controls return to the fitted user scale after paint. */
        for(n=0;n<4;++n){
            x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;adjust[n](&x,&y,&w,&h);
            CHECK(x>=-.003f&&y>=-.003f&&x+w<=sizes[size][0]+.003f&&y+h<=sizes[size][1]+.003f);
            CHECK(h<=480*scales[scale]+.003f&&near(w/TCE_VIEWPORT_CANVAS_WIDTH,h/480));
        }
    }
    modernEnabled=0;menu.window.name="background_1";tceMenuViewportPaintMenu=&menu;CHECK(!TCE_MenuViewportBackground());
    modernEnabled=1;tceMenuViewportPaintMenu=NULL;focusedMenu=NULL;menuMaxSetting=1080;TCE_ViewportSetDrawMode(TCE_VIEWPORT_MENU);
}

static void physicalCursorAndFittedText(void)
{
    const int sizes[][2]={{1024,768},{1280,720},{1920,1080},{3440,1440},{3840,2160}};
    const float scales[]={.5f,.65f,.85f,1.f,1.25f,1.5f};
    const char *names[]={"options","options_system"};
    const char *texts[]={"Options","Set recommended","^3This complete long menu label must remain readable","WIDE FULL LABEL 0123456789"};
    const float widths[]={15,40,112,280};
    menuDef_t menu;
    float baseSize,fitSize,expectedX,expectedY,fitScale;
    int size,scale,mode,anchor,centered,cap,i,j,strong;
    modernEnabled=enabled=1;menuMaxSetting=1080;
    for(size=0;size<5;++size){
        cgs.glconfig.vidWidth=uiInfo.uiDC.glconfig.vidWidth=sizes[size][0];
        cgs.glconfig.vidHeight=uiInfo.uiDC.glconfig.vidHeight=sizes[size][1];
        baseSize=20*fminf(sizes[size][0]/TCE_VIEWPORT_CANVAS_WIDTH,sizes[size][1]/480.f);
        CHECK(near(TCE_ViewportCursorSize(sizes[size][0],sizes[size][1]),baseSize));
        for(scale=0;scale<6;++scale)for(cap=0;cap<2;++cap)for(mode=0;mode<2;++mode)for(anchor=0;anchor<2;++anchor)for(centered=0;centered<2;++centered){
            menuScaleSetting=scales[scale];menuMaxSetting=cap?1080:480;hudScaleSetting=scale&1?.5f:1.5f;CG_HudViewportInit();
            menu.window.name=names[anchor];focusedMenu=&menu;tceMenuViewportPaintMenu=NULL;
            TCE_ViewportSetDrawMode(mode?TCE_VIEWPORT_FULLSCREEN:TCE_VIEWPORT_MENU);
            fitSize=fminf(sizes[size][0]/TCE_VIEWPORT_CANVAS_WIDTH,sizes[size][1]/480.f);
            fitScale=mode?fitSize:fminf(fitSize,fminf(fitSize,menuMaxSetting/480)*menuScaleSetting);
            expectedX=151*fitScale+(sizes[size][0]-TCE_VIEWPORT_CANVAS_WIDTH*fitScale)*.5f;
            expectedY=109*fitScale+(sizes[size][1]-480*fitScale)*(mode||anchor?.5f:0);
            if(centered){expectedX-=baseSize*(14.f/32.f);expectedY-=baseSize*(14.f/32.f);}
            drawnCount=0;TCE_ModernCursor(151,109,99,centered);
            CHECK(drawnCount==1&&near(drawnRects[0].x,expectedX)&&near(drawnRects[0].y,expectedY));
            CHECK(near(drawnRects[0].w,baseSize)&&near(drawnRects[0].h,baseSize));
            /* This single expected physical projection is checked by both
             * compiled UI and CGame implementations, independently of HUD. */
        }
    }
    drawnCount=0;TCE_ModernCursor(1,2,0,0);CHECK(drawnCount==0);
    CHECK(TCE_ViewportCursorSize(0,720)==0&&TCE_ViewportCursorSize(1280,-1)==0);
    cgs.glconfig.vidWidth=uiInfo.uiDC.glconfig.vidWidth=0;TCE_ModernCursor(1,2,99,0);CHECK(drawnCount==0);
    focusedMenu=NULL;menuScaleSetting=1;menuMaxSetting=1080;TCE_ViewportSetDrawMode(TCE_VIEWPORT_MENU);
    /* Compile the actual fit+clip helpers. Controlled fixed font advances make
     * complete-string retention and size bounds deterministic, not font parity. */
    for(i=0;i<4;++i)for(j=0;j<4;++j)for(strong=0;strong<2;++strong){
        textPaintCalls=0;TCE_ModernTextFit(31,44,.35f,widths[j],colorYellow,texts[i],strong);
        CHECK(textPaintCalls==1&&!strcmp(paintedText,texts[i]));
        CHECK(paintedX==31&&paintedY==44&&paintedScale>0&&paintedScale<=.35f);
        CHECK(TCE_ModernWidth(paintedText,paintedScale,TCE_ModernFont(strong))<=widths[j]);
    }
    textPaintCalls=0;TCE_ModernTextFit(1,2,.3f,0,colorYellow,"Options",0);
    TCE_ModernTextFit(1,2,0,100,colorYellow,"Options",0);TCE_ModernTextFit(1,2,.3f,100,colorYellow,NULL,0);
    CHECK(textPaintCalls==0);
}
int main(void)
{
    const int sizes[][2]={{800,600},{1280,800},{1280,720},{1920,1080},{3440,1440},{3840,2160}};
    const float scales[]={.5f,.85f,1.f,1.5f};
    int i,j,k;float x,y,w,h;
    tce_viewport_t view,before;
    for(i=0;i<6;++i)for(j=0;j<4;++j)for(k=0;k<2;++k){menuScaleSetting=scales[j];menuMaxSetting=k?1080:0;wrappers(sizes[i][0],sizes[i][1]);}
    for(i=0;i<6;++i){
        CHECK(TCE_ViewportFit(&view,sizes[i][0],sizes[i][1],640,480,0,1,0,0));
        CHECK(near(view.offsetX,0)&&near(view.offsetY,0));
        CHECK(view.scale<=sizes[i][0]/640.f+.001f&&view.scale<=sizes[i][1]/480.f+.001f);
        CHECK(TCE_ViewportFit(&view,sizes[i][0],sizes[i][1],TCE_VIEWPORT_CANVAS_WIDTH,480,1080,1,1,1));
        x=y=0;w=TCE_VIEWPORT_CANVAS_WIDTH;h=480;TCE_ViewportProject(&view,&x,&y,&w,&h);
        CHECK(near(x+w,(float)sizes[i][0])&&near(y+h,(float)sizes[i][1]));
        CHECK(h<=1080.001f);
    }
    CHECK(TCE_ViewportFit(&view,1920,1080,TCE_VIEWPORT_CANVAS_WIDTH,480,0,1,.5,.5));before=view;
    CHECK(!TCE_ViewportFit(&view,0,1080,853,480,0,1,.5,.5)&&!memcmp(&before,&view,sizeof(view)));
    CHECK(!TCE_ViewportFit(&view,1920,-1,853,480,0,1,.5,.5));
    CHECK(!TCE_ViewportFit(&view,1920,1080,NAN,480,0,1,.5,.5));
    CHECK(!TCE_ViewportFit(&view,1920,1080,INFINITY,480,0,1,.5,.5));
    CHECK(!TCE_ViewportFit(NULL,1920,1080,853,480,0,1,.5,.5));
    CHECK(TCE_ViewportFit(&view,1920,1080,TCE_VIEWPORT_CANVAS_WIDTH,480,NAN,NAN,NAN,NAN));
    CHECK(near(view.scale,before.scale)&&near(view.offsetX,before.offsetX)&&near(view.offsetY,before.offsetY));
    x=1;y=2;w=3;h=4;view.scale=INFINITY;TCE_ViewportProject(&view,&x,&y,&w,&h);CHECK(x==1&&y==2&&w==3&&h==4);
    view=before;x=NAN;TCE_ViewportProject(&view,&x,&y,&w,&h);CHECK(isnan(x)&&y==2&&w==3&&h==4);
    x=2;TCE_ViewportProject(&view,&x,NULL,NULL,NULL);CHECK(near(x,2*view.scale+view.offsetX));
    TCE_ViewportProject(NULL,NULL,NULL,NULL,NULL);
    uiInfo.uiDC.glconfig.vidWidth=0;x=1;y=2;w=3;h=4;UI_AdjustFrom640(&x,&y,&w,&h);CHECK(x==1&&y==2&&w==3&&h==4);
    SharedWindowsAdjust(&x,&y,&w,&h);CHECK(x==1&&y==2&&w==3&&h==4);
    /* Feature disabled retains the original native/safe legacy branches. */
    enabled=0;uiInfo.uiDC.glconfig.vidWidth=800;uiInfo.uiDC.glconfig.vidHeight=600;DC->xscale=DC->yscale=1.25f;
    x=100;y=50;w=80;h=60;UI_AdjustFrom640(&x,&y,&w,&h);CHECK(near(x,93.75f)&&near(y,121.875f)&&near(w,75)&&near(h,56.25f));
    x=100;y=50;w=80;h=60;SharedWindowsAdjust(&x,&y,&w,&h);CHECK(near(x,93.75f)&&near(y,121.875f));
    x=100;y=50;w=80;h=60;SharedLinuxAdjust(&x,&y,&w,&h);CHECK(near(x,93.75f)&&near(y,121.875f));
    x=100;y=50;w=80;h=60;SharedGenericAdjust(&x,&y,&w,&h);CHECK(near(x,93.75f)&&near(y,121.875f));
    _UI_MouseEvent(9999,9999);CHECK(mouseX==852&&mouseY==480);
    _UI_MouseEvent(-9999,-9999);CHECK(mouseX==0&&mouseY==0);
#ifdef CGAMEDLL
    enabled=1;hudScaleSetting=.85f;hudMaxSetting=1080;CG_HudViewportInit();
    cgs.glconfig.vidWidth=1920;cgs.glconfig.vidHeight=1080;CG_HudViewportBegin(0,0);
    x=y=0;w=100;h=50;SharedWindowsAdjust(&x,&y,&w,&h);
    CHECK(x==0&&y==0&&near(w,191.25f)&&near(h,95.625f));CG_HudViewportEnd();
#endif
    hudContext();
    worldOverlayAndBorders();
    nativeMenusAndInput();
    headerMenusAndHudComposites();
    backgroundAndFullscreenScopes();
    physicalCursorAndFittedText();
    printf("viewport: %i checks, %i failures\n",checks,failures);return failures?1:0;
}
