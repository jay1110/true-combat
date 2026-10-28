/* Actual optional theme implementation, controlled native engine services. */
#include "src/ui/ui_shared.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

static int checks, failures, enabled = 1, menuActive = 1, milliseconds = 1000, updates, registrations;
static struct { char name[64]; float value; int flags; } cvars[32];
static int cvarCount;
static struct { char name[128]; } images[256];
static int imageCount, imageCalls, drawCount, nullColor;
static int fadeCalls;
static qboolean debugMode;
static displayContextDef_t services, *DC=&services;
static vec4_t color;
static struct { qhandle_t shader; vec4_t color; int reset; } draws[4];
vec4_t tceModernWhite={.93f,.96f,.98f,1}, tceModernMuted={.55f,.64f,.72f,1}, tceModernAmber={.96f,.73f,.27f,1};

static void check(int condition, const char *label)
{ ++checks; if(!condition) { ++failures; fprintf(stderr,"FAIL: %s\n",label); } }
static int close_float(float a,float b) { return fabsf(a-b)<.00001f; }
static float bits(unsigned value) { float f; memcpy(&f,&value,sizeof(f)); return f; }
static int equal(const float *a,const float *b)
{ int i; for(i=0;i<4;++i)if(!close_float(a[i],b[i]))return 0;return 1; }
qboolean TCE_ModernUIEnabled(void) { return enabled; }
int CG_MenuViewportActive(void) { return menuActive; }
int trap_Milliseconds(void) { return milliseconds; }
char *QDECL va(const char *format,...)
{
    static char buffers[16][256];static unsigned index;va_list args;char *out=buffers[index++&15];
    va_start(args,format);vsnprintf(out,256,format,args);va_end(args);return out;
}
static int find_cvar(const char *name)
{ int i;for(i=0;i<cvarCount;++i)if(!strcmp(cvars[i].name,name))return i;return -1; }
void trap_Cvar_Register(vmCvar_t *cvar,const char *name,const char *value,int flags)
{
    int at=find_cvar(name);++registrations;
    if(at<0){at=cvarCount++;strncpy(cvars[at].name,name,63);cvars[at].value=(float)atof(value);cvars[at].flags=flags;}
    cvar->handle=at;cvar->value=cvars[at].value;
}
void trap_Cvar_Update(vmCvar_t *cvar) { ++updates;cvar->value=cvars[cvar->handle].value; }
static void set(const char *name,float value)
{ int at=find_cvar(name);check(at>=0,"Test cvar registered");if(at>=0)cvars[at].value=value; }
qhandle_t trap_R_RegisterShaderNoMip(const char *name)
{
    int i;++imageCalls;for(i=0;i<imageCount;++i)if(!strcmp(images[i].name,name))return i+1;
    check(imageCount<256,"Shader service bound");strncpy(images[imageCount].name,name,127);return ++imageCount;
}
void trap_R_SetColor(const float *value)
{ int i;nullColor=value==NULL;for(i=0;i<4;++i)color[i]=value?value[i]:1; }
void UI_DrawHandlePic(float x,float y,float w,float h,qhandle_t shader)
{
    (void)x;(void)y;(void)w;(void)h;
    if(drawCount<4){draws[drawCount].shader=shader;draws[drawCount].reset=nullColor;memcpy(draws[drawCount].color,color,sizeof(color));}
    ++drawCount;
}
void CG_DrawPic(float x,float y,float w,float h,qhandle_t shader) { UI_DrawHandlePic(x,y,w,h,shader); }

/* ACTUAL_THEME */

/* Fade math remains the original separate SDK path. Control one actual fade
 * transition here to verify Window_Paint passes its alpha to paint exactly once
 * and does not persist the user's palette in parsed menu state. */
static void Fade(int *flags,float *alpha,float clamp,int *next,int cycle,qboolean remove,float amount)
{ (void)clamp;(void)next;(void)cycle;(void)remove;(void)amount;++fadeCalls;*alpha=.5f;*flags&=~WINDOW_FADINGIN; }
static void fill(float x,float y,float w,float h,const vec4_t value)
{ trap_R_SetColor(value);UI_DrawHandlePic(x,y,w,h,0); }
static void border(float x,float y,float w,float h,float size,const vec4_t value)
{ (void)size;fill(x,y,w,h,value); }
static void GradientBar_Paint(rectDef_t *rect,const vec4_t value)
{ fill(rect->x,rect->y,rect->w,rect->h,value); }

/* ACTUAL_WINDOW_PAINT */

static void fresh(void)
{
    TCE_ModernThemeReset();memset(cvars,0,sizeof(cvars));memset(images,0,sizeof(images));
    cvarCount=registrations=updates=imageCount=imageCalls=drawCount=0;enabled=menuActive=1;milliseconds+=32;
}
static void refresh(void) { milliseconds+=16;TCE_ModernThemeRefresh(); }

int main(void)
{
    int i,j,previous;vec4_t input={.1f,.2f,.3f,.8f},output,red={.9f,.1f,.1f,1},white={1,1,1,1};
    qhandle_t known,unknown;
    const float expected[4][3]={{.96f,.73f,.27f},{.27f,.76f,.94f},{.98f,.38f,.36f},{.32f,.85f,.66f}};
    float unusual[]={bits(0x7fc00000u),bits(0x7f800000u),bits(0xff800000u),-100.f,100.f,0.f,.5f,1.f};
    fresh();enabled=0;TCE_ModernThemeRefresh();check(registrations==0,"No opt-in marker means no new cvars");
    TCE_ModernThemeColor(input,output,1);check(equal(input,output),"No marker preserves legacy surface RGBA");
    unknown=trap_R_RegisterShaderNoMip("models/teams/specops");previous=imageCalls;
    TCE_ModernThemePicture(0,0,8,8,unknown,input);
    check(drawCount==1 && draws[0].shader==unknown && equal(draws[0].color,input),"No marker preserves image rendering");
    check(imageCalls==previous,"No marker avoids optional shader registration");
    fresh();TCE_ModernThemeRefresh();check(cvarCount==9 && registrations==9,"Exactly nine archived appearance cvars");
    for(i=0;i<cvarCount;++i)check(cvars[i].flags==CVAR_ARCHIVE,"Every appearance cvar persists locally");
    previous=updates;TCE_ModernThemeRefresh();check(updates==previous,"Refresh runs once in the current 16ms bucket");
    set("ui_themePreset",1);milliseconds=(milliseconds/16+1)*16;TCE_ModernThemeRefresh();
    check(close_float(tceModernAmber[0],.27f),"Palette update visible by next 16ms bucket");
    for(i=0;i<4;++i){set("ui_themePreset",(float)i);refresh();for(j=0;j<3;++j)check(close_float(tceModernAmber[j],expected[i][j]),"Every preset accent");}
    set("ui_themePreset",4);set("ui_themeAccentR",.12f);set("ui_themeAccentG",.34f);set("ui_themeAccentB",.56f);
    set("ui_themeBackgroundR",.8f);set("ui_themeBackgroundG",.9f);set("ui_themeBackgroundB",1);refresh();
    check(close_float(tceModernAmber[0],.12f)&&close_float(tceModernAmber[1],.34f)&&close_float(tceModernAmber[2],.56f),"Custom accent is applied");
    check(tceModernWhite[0]<.1f && tceModernMuted[0]<.2f,"Light custom background selects legible dark text");
    for(i=0;i<(int)(sizeof(unusual)/sizeof(unusual[0]));++i){
        float value=TCE_ThemeClamp(unusual[i],.3f,0,1);check(value>=0&&value<=1,"Clamp handles nonfinite and extreme custom values");
        set("ui_themePreset",unusual[i]);set("ui_themeOpacityPreset",unusual[i]);set("ui_themeOpacity",unusual[i]);
        set("ui_themeAccentR",unusual[i]);set("ui_themeBackgroundB",unusual[i]);refresh();
        for(j=0;j<3;++j){check(tceModernAmber[j]>=0&&tceModernAmber[j]<=1,"Preset/custom accent remains finite bounded");check(modernTheme.base[j]>=0&&modernTheme.base[j]<=1,"Custom background remains finite bounded");}
        check(modernTheme.alpha>=.25f&&modernTheme.alpha<=1,"Opacity remains finite bounded");
    }
    set("ui_themePreset",2);set("ui_themeOpacityPreset",2);refresh();
    TCE_ModernThemeColor(white,output,0);check(close_float(output[3],1),"Text stays opaque with glass surfaces");
    TCE_ModernThemeColor(input,output,1);check(close_float(output[3],input[3]*.6f),"Glass affects only surfaces");
    TCE_ModernThemeColor(red,output,0);check(equal(red,output),"Team-colored text is retained");
    set("ui_themeOpacityPreset",3);set("ui_themeOpacity",.1f);refresh();check(close_float(modernTheme.alpha,.25f),"Custom opacity lower bound");
    set("ui_themeOpacity",2);refresh();check(close_float(modernTheme.alpha,1),"Custom opacity upper bound");
    set("ui_themeOpacity",.42f);refresh();check(close_float(modernTheme.alpha,.42f),"Custom opacity uses selected value");
    known=trap_R_RegisterShaderNoMip("tce2_ui/button");unknown=trap_R_RegisterShaderNoMip("models/teams/specops");drawCount=0;
    TCE_ModernThemePicture(4,5,20,30,known,white);
    check(drawCount==2,"Nondefault palette draws neutral body and accent");
    check(draws[0].shader!=known && draws[1].shader!=known && draws[0].shader!=draws[1].shader,"Body/accent have distinct registered masks");
    check(close_float(draws[0].color[3],.42f)&&close_float(draws[1].color[3],.42f),"Both neutral layers share surface opacity");
    check(modernTheme.imageCount==62 && modernTheme.imageCount<(int)(sizeof(modernThemeImages)/sizeof(modernThemeImages[0])),"All 51 widgets/icons plus 11 aliases stay inside cache");
    previous=imageCalls;drawCount=0;TCE_ModernThemePicture(4,5,20,30,known,white);check(imageCalls==previous,"Theme shader registration is cached");
    drawCount=0;TCE_ModernThemePicture(4,5,20,30,unknown,red);
    check(drawCount==1&&draws[0].shader==unknown&&equal(draws[0].color,red),"Unknown/team image retains original tint and alpha");
    drawCount=0;TCE_ModernThemePicture(4,5,20,30,unknown,white);
    check(drawCount==1&&equal(draws[0].color,white),"Unknown white image remains untinted and opaque");
    drawCount=0;TCE_ModernThemePicture(4,5,20,30,unknown,input);
    check(drawCount==1&&close_float(draws[0].color[3],input[3]*.42f),"Legacy dark menu fadebox receives chosen surface opacity");
    { Window window;vec4_t saved;memset(&window,0,sizeof(window));
      services.setColor=trap_R_SetColor;services.drawHandlePic=UI_DrawHandlePic;
      services.fillRect=fill;services.drawRect=border;
      window.rect.x=20;window.rect.y=20;window.rect.w=100;window.rect.h=80;
      window.style=WINDOW_STYLE_FILLED;memcpy(window.backColor,input,sizeof(input));
      memcpy(saved,window.backColor,sizeof(saved));window.background=unknown;window.flags=WINDOW_VISIBLE|WINDOW_FADINGIN;
      drawCount=fadeCalls=0;Window_Paint(&window,.075f,.55f,20);
      check(fadeCalls==1&&!(window.flags&WINDOW_FADINGIN)&&(window.flags&WINDOW_VISIBLE),"Window paint preserves the separate Fade state transition");
      check(close_float(window.backColor[3],.5f),"Window backColor retains SDK fade alpha, not themed opacity");
      check(drawCount==1&&close_float(draws[0].color[3],.5f*.42f),"Window shader receives post-Fade alpha times one opacity factor");
      for(i=0;i<3;++i)check(close_float(window.backColor[i],saved[i]),"Window paint never persists palette in menu colors");
      window.background=0;window.backColor[3]=.8f;drawCount=fadeCalls=0;Window_Paint(&window,.075f,.55f,20);
      check(!fadeCalls&&drawCount==1&&close_float(draws[0].color[3],.8f*.42f),"Untextured window applies surface opacity once without adding Fade calls"); }
    /* Window_Paint supplies a palette-adjusted forecolor to known icon art.
     * The shader layer must not square that same accent a second time. */
    { vec4_t gold={.94f,.75f,.27f,1};qhandle_t icon=trap_R_RegisterShaderNoMip("tce2_ui/icons/play");
      TCE_ModernThemeColor(gold,output,0);drawCount=0;
      TCE_ModernThemePicture(0,0,20,20,icon,output);
      check(drawCount==2,"Known icon selects theme layers");
      for(i=0;i<3;++i)check(close_float(draws[1].color[i],tceModernAmber[i]),"Known icon uses custom accent exactly once"); }
#ifdef CGAMEDLL
    menuActive=0;
    TCE_ModernThemeColor(input,output,1);check(equal(input,output),"CGame HUD surface ignores menu palette and opacity");
    TCE_ModernThemeColor(white,output,0);check(equal(white,output),"CGame HUD text ignores menu palette");
    previous=imageCalls;drawCount=0;TCE_ModernThemePicture(0,0,8,8,known,white);
    check(drawCount==1&&draws[0].shader==known&&equal(draws[0].color,white),"CGame HUD image uses original shader and tint");
    check(imageCalls==previous,"CGame HUD never initializes palette masks");
    menuActive=1;
#endif
    set("ui_themePreset",0);refresh();drawCount=0;TCE_ModernThemePicture(0,0,8,8,known,white);
    check(drawCount==1&&draws[0].shader==known,"Default gold retains original artwork");
    TCE_ModernThemeReset();check(!modernTheme.initialized&&!modernTheme.imageCount,"Reset drops all module-local caches");
    check(close_float(tceModernAmber[0],.96f)&&close_float(tceModernWhite[0],.93f)&&close_float(tceModernMuted[0],.55f),"Reset restores native palette defaults");
    printf("checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
