/* Exact production fit/projection and UI coordinate wrappers with controlled
 * cvar/display services. This does not simulate OS cursor capture or pixels. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "src/ui/tce_viewport.h"
#include "src/ui/tce_ui_coordinates.h"
#include "src/cgame/cg_hud_viewport.h"
typedef int qboolean;
typedef float vec4_t[4];
typedef struct { int integer; float value; } vmCvar_t;
#define qtrue 1
#define qfalse 0
#define SCREEN_HEIGHT 480
#define CVAR_ARCHIVE 1
typedef struct { struct { int vidWidth, vidHeight; } glconfig; float xscale,yscale; int cursorx,cursory; float (*getCVarValue)(const char *); } displayContextDef_t;
static struct { displayContextDef_t uiDC; } uiInfo;
static displayContextDef_t *DC=&uiInfo.uiDC;
static float enabled=1,menuScale=1,maxHeight=1080;
static float hudScaleSetting=.85f,hudMaxSetting=1080;
static struct { struct { int vidWidth,vidHeight; } glconfig; float screenXScale,screenYScale;struct { int whiteShader; } media; } cgs;
static qboolean tce_uiCoordinates;
static struct { float x,y,w,h; } drawnRects[16];
static int drawnCount;
static vmCvar_t cg_aspectMode,*bound[3];
static int checks,failures,registrations,updates,mouseX,mouseY,mouseCalls;
#define CHECK(c) do { ++checks;if(!(c)){++failures;printf("FAIL %i: %s\n",__LINE__,#c);} } while(0)
static int near(float a,float b) { return fabsf(a-b)<.003f; }
static float trap_Cvar_VariableValue(const char *name)
{ if(!strcmp(name,"cg_wideScreen"))return enabled;if(!strcmp(name,"ui_menuScale"))return menuScale;if(!strcmp(name,"ui_menuMaxHeight"))return maxHeight;return 0; }
static void trap_Cvar_Update(vmCvar_t *var)
{
    if(var==bound[0])var->value=enabled;
    else if(var==bound[1])var->value=hudScaleSetting;
    else if(var==bound[2])var->value=hudMaxSetting;
    else CHECK(0);
    var->integer=(int)var->value;++updates;
}
static void trap_Cvar_Register(vmCvar_t *var,const char *name,const char *value,int flags)
{
    int n=!strcmp(name,"cg_wideScreen")?0:!strcmp(name,"cg_hudScale")?1:!strcmp(name,"cg_hudMaxHeight")?2:-1;
    CHECK(n>=0&&flags==CVAR_ARCHIVE);if(n<0)return;
    CHECK(!strcmp(value,n==0?"1":n==1?"0.85":"1080"));bound[n]=var;++registrations;trap_Cvar_Update(var);
}
static int TCE_UI_WrapAdd(int a,int b) { return (int)((unsigned)a+(unsigned)b); }
static int Menu_Count(void) { return 1; }
static void Display_MouseMove(void *p,int x,int y) { (void)p;mouseX=x;mouseY=y;++mouseCalls; }
static void trap_R_SetColor(const float *color) { (void)color; }
static void trap_R_DrawStretchPic(float x,float y,float w,float h,float s1,float t1,float s2,float t2,int shader)
{ (void)s1;(void)t1;(void)s2;(void)t2;(void)shader;CHECK(w>=0&&h>=0);if(drawnCount<16){drawnRects[drawnCount].x=x;drawnRects[drawnCount].y=y;drawnRects[drawnCount].w=w;drawnRects[drawnCount++].h=h;} }
void CG_AdjustFrom640(float *x,float *y,float *w,float *h);

/* ACTUAL_HUD_CONTEXT */

/* ACTUAL_DRAW_ADAPTERS */

/* ACTUAL_FUNCTIONS */

static void wrappers(int width,int height)
{
    float x,y,w,h,sx,sy,sw,sh;
    tce_viewport_t view;
    void (*draws[4])(float*,float*,float*,float*)={UI_AdjustFrom640,SharedWindowsAdjust,SharedLinuxAdjust,SharedGenericAdjust};
    int n;
    uiInfo.uiDC.glconfig.vidWidth=width;uiInfo.uiDC.glconfig.vidHeight=height;
    uiInfo.uiDC.xscale=width/640.f;uiInfo.uiDC.yscale=height/480.f;DC->getCVarValue=trap_Cvar_VariableValue;
    CHECK(TCE_ViewportFit(&view,width,height,TCE_VIEWPORT_CANVAS_WIDTH,480,maxHeight,menuScale,.5f,.5f));
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
    CG_HudViewportInit();CHECK(registrations==3&&updates==3&&CG_WideScreenEnabled());
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
int main(void)
{
    const int sizes[][2]={{800,600},{1280,800},{1280,720},{1920,1080},{3440,1440},{3840,2160}};
    const float scales[]={.5f,.85f,1.f,1.5f};
    int i,j,k;float x,y,w,h;
    tce_viewport_t view,before;
    for(i=0;i<6;++i)for(j=0;j<4;++j)for(k=0;k<2;++k){menuScale=scales[j];maxHeight=k?1080:0;wrappers(sizes[i][0],sizes[i][1]);}
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
    printf("viewport: %i checks, %i failures\n",checks,failures);return failures?1:0;
}
