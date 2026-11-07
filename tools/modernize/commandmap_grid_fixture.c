/* Controlled trap services; the grid and every viewport adapter below are
 * extracted from production sources. No engine or live map is simulated. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include "src/ui/tce_viewport.h"

typedef int qboolean;
typedef float vec2_t[2];
typedef float vec4_t[4];
typedef struct { int integer; float value; } vmCvar_t;
typedef struct { qboolean circular; float zoomFactor; vec2_t tl, br; } mapScissor_t;
#define qtrue 1
#define qfalse 0
#define CVAR_ARCHIVE 1
#define Square(x) ((x)*(x))
#define SQRTFAST(x) sqrtf(x)
#define Vector4Set(v,a,b,c,d) do { (v)[0]=(a); (v)[1]=(b); (v)[2]=(c); (v)[3]=(d); } while(0)
static struct { vec2_t mapcoordsMins, mapcoordsMaxs; } cg;
static struct { float screenXScale, screenYScale; struct { int vidWidth, vidHeight; } glconfig;
 struct { int whiteShader, limboFont2; } media; } cgs;
static qboolean tce_uiCoordinates;
static vmCvar_t cg_aspectMode;
static int modern, checks, failures, lineCount;
static float lines[256][4];
static const float clrBrownLine[4]={.2f,.2f,.2f,1},colorBlack[4]={0,0,0,1};
static qboolean gridInitDone;
static vec2_t gridStartCoord, gridStep;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr,"line %d: %s\n",__LINE__,#x); } } while(0)
static int near(float a,float b) { return fabsf(a-b)<.002f; }
static int TCE_ModernUIEnabled(void) { return modern; }
static void trap_Cvar_Register(vmCvar_t *c,const char *n,const char *d,int flags)
{ (void)n;(void)flags;c->value=(float)atof(d);c->integer=(int)c->value; }
static void trap_Cvar_Update(vmCvar_t *c) { (void)c; }
static void trap_R_SetColor(const float *c) { (void)c; }
static void trap_R_DrawStretchPic(float x,float y,float w,float h,float s,float t,float s2,float t2,int shader)
{ (void)s;(void)t;(void)s2;(void)t2;CHECK(shader==19 && lineCount<256);
 if(lineCount<256) {lines[lineCount][0]=x;lines[lineCount][1]=y;lines[lineCount][2]=w;lines[lineCount++][3]=h;} }
static float CG_Text_Width_Ext(const char *s,float scale,int limit,const int *font)
{ (void)limit;(void)font;return (float)strlen(s)*scale*20; }
static float CG_Text_Height_Ext(const char *s,float scale,int limit,const int *font)
{ (void)s;(void)limit;(void)font;return scale*20; }
static void CG_Text_Paint_Ext(float x,float y,float sx,float sy,const float *c,const char *s,float a,int l,int st,const int *f)
{ (void)x;(void)y;(void)sx;(void)sy;(void)c;(void)s;(void)a;(void)l;(void)st;(void)f; }
static void Com_sprintf(char *s,size_t n,const char *format,...) { va_list args;va_start(args,format);vsnprintf(s,n,format,args);va_end(args); }
void CG_AdjustFrom640(float *,float *,float *,float *);
/* ACTUAL_HUD_CONTEXT */
/* ACTUAL_FUNCTIONS */

int main(void)
{
 const int sizes[][2]={{640,480},{800,600},{1280,1024},{1366,768},{1920,1080},{3840,2160}};
 const float scales[]={.6f,.85f,1.f};
 const float rectangles[][4]={{460,80,352,352},{430,64,380,220},{510,64,220,380}};
 int resolution,scale,context,rect,clip,i;
 cg.mapcoordsMins[0]=-4000;cg.mapcoordsMaxs[0]=4000;
 cg.mapcoordsMins[1]=4000;cg.mapcoordsMaxs[1]=-4000;cgs.media.whiteShader=19;
 CG_HudViewportInit();
 for(resolution=0;resolution<6;++resolution) for(scale=0;scale<3;++scale)
 for(context=0;context<4;++context) for(rect=0;rect<3;++rect) for(clip=0;clip<3;++clip)
 {
  float x=rectangles[rect][0],y=rectangles[rect][1],w=rectangles[rect][2],h=rectangles[rect][3];
  float px=x,py=y,pw=w,ph=h;
  mapScissor_t scissor={0,1.6f,{100,80},{400,400}};
  /* Circular minimaps are square by contract; rectangular full maps need not be. */
  if(clip==2 && rect) continue;
  cgs.glconfig.vidWidth=sizes[resolution][0];cgs.glconfig.vidHeight=sizes[resolution][1];
  cgs.screenXScale=sizes[resolution][0]/640.f;cgs.screenYScale=sizes[resolution][1]/480.f;
  hudScale.value=menuScale.value=scales[scale];modern=context<2;tce_uiCoordinates=context!=3;
  wideScreen.integer=context<2;CG_HudViewportEnd();menuViewportDepth=0;
  if(context==0) CG_HudViewportBegin(.5f,.5f);
  if(context==1) CG_MenuViewportBegin();
  CG_AdjustFrom640(&px,&py,&pw,&ph);lineCount=0;gridInitDone=0;
  scissor.circular=clip==2;CG_DrawGrid(x,y,w,h,clip?&scissor:NULL);
  CHECK(lineCount>=8);
  for(i=0;i<lineCount;++i) {
   CHECK(isfinite(lines[i][0]) && isfinite(lines[i][1]) && isfinite(lines[i][2]) && isfinite(lines[i][3]));
   CHECK(lines[i][0]>=px-.002f && lines[i][1]>=py-.002f);
   CHECK(lines[i][0]+lines[i][2]<=px+pw+.002f && lines[i][1]+lines[i][3]<=py+ph+.002f);
   CHECK(lines[i][2]>0 && lines[i][3]>0 && (lines[i][2]<=1.001f || lines[i][3]<=1.001f));
   if(!clip) {
    if(lines[i][2]<=1.001f) CHECK(near(lines[i][1],py) && near(lines[i][3],ph));
    else CHECK(near(lines[i][0],px) && near(lines[i][2],pw));
   }
  }
 }
 /* Edge and wholly external strokes must never leak outside the map. */
 { vec4_t line={811.75f,80,1,352}; float x=460,y=80,w=352,h=352;
   CG_HudViewportBegin(.5f,.5f);CG_AdjustFrom640(&x,&y,&w,&h);lineCount=0;
   CG_DrawGridLine(line,460,80,352,352,qtrue);CHECK(lineCount==1);
   CHECK(lines[0][0]+lines[0][2]<=x+w+.002f);
   Vector4Set(line,900,80,1,352);CG_DrawGridLine(line,460,80,352,352,qtrue);CHECK(lineCount==1);
 }
 printf("%d checks, %d failures; actual grid + actual viewport projections\n",checks,failures);
 return failures?1:0;
}
