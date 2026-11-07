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
static const char *missingImage;
static int missingAliases, missingAll, clobberVA;
static int fadeCalls;
static int backgroundContext;
static qboolean debugMode;
static displayContextDef_t services, *DC=&services;
static menuDef_t *tceMenuViewportPaintMenu;
static int itemPaintCalls,scriptCalls;
static vec4_t color;
static struct { qhandle_t shader; vec4_t color; int reset; float x,y,w,h; } draws[4];
vec4_t tceModernWhite={.93f,.96f,.98f,1}, tceModernMuted={.55f,.64f,.72f,1}, tceModernAmber={.96f,.73f,.27f,1};

static void check(int condition, const char *label)
{ ++checks; if(!condition) { ++failures; fprintf(stderr,"FAIL: %s\n",label); } }
static int close_float(float a,float b) { return fabsf(a-b)<.00001f; }
static float bits(unsigned value) { float f; memcpy(&f,&value,sizeof(f)); return f; }
static int equal(const float *a,const float *b)
{ int i; for(i=0;i<4;++i)if(!close_float(a[i],b[i]))return 0;return 1; }
qboolean TCE_ModernUIEnabled(void) { return enabled; }
int CG_MenuViewportActive(void) { return menuActive; }
int TCE_MenuViewportBackground(void) { return backgroundContext; }
int trap_Milliseconds(void) { return milliseconds; }
int Q_strlenInt(const char *text) { return (int)strlen(text); }
void QDECL Com_Error(int level,const char *format,...) { (void)level;(void)format;abort(); }
/* ACTUAL_SDK_VA */
/* EXPECTED_SHADER_NAMES */
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
    int i,result;++imageCalls;
    if(!strncmp(name,"tce2_ui/",8) || !strncmp(name,"ui/assets/",10)) {
        int known=0;
        for(i=0;i<(int)(sizeof(expectedShaderNames)/sizeof(expectedShaderNames[0]));++i)
            if(!strcmp(name,expectedShaderNames[i])) { known=1;break; }
        check(known,"Actual palette request names an existing packaged shader/image alias");
        if(!known) return 0;
    }
    if(missingAll || (missingAliases && !strncmp(name,"ui/assets/",10)) ||
       (missingImage && !strcmp(name,missingImage))) return 0;
    result=0;for(i=0;i<imageCount;++i)if(!strcmp(images[i].name,name)){result=i+1;break;}
    if(!result){check(imageCount<256,"Shader service bound");strncpy(images[imageCount].name,name,127);result=++imageCount;}
    /* An engine callback/formatter may recycle every SDK va allocation. */
    if(clobberVA){va("%031800d",0);va("%031800d",0);}
    return result;
}
void trap_R_SetColor(const float *value)
{ int i;nullColor=value==NULL;for(i=0;i<4;++i)color[i]=value?value[i]:1; }
void UI_DrawHandlePic(float x,float y,float w,float h,qhandle_t shader)
{
    if(drawCount<4){draws[drawCount].shader=shader;draws[drawCount].reset=nullColor;memcpy(draws[drawCount].color,color,sizeof(color));draws[drawCount].x=x;draws[drawCount].y=y;draws[drawCount].w=w;draws[drawCount].h=h;}
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

static int controlEnabled=1;
static qboolean Item_EnableShowViaCvar(itemDef_t *item,int flag)
{ (void)item;check(flag==CVAR_ENABLE,"Focus uses the native enable/disable cvar gate");return controlEnabled; }
/* ACTUAL_FOCUS_PAINT */

static float relativeLuminance(const float *rgb)
{
    int i;double result=0;const double weights[]={.2126,.7152,.0722};
    for(i=0;i<3;++i)result+=weights[i]*(rgb[i]<=.04045 ? rgb[i]/12.92 : pow((rgb[i]+.055)/1.055,2.4));
    return (float)result;
}
static float contrast(const float *a,const float *b)
{ float x=relativeLuminance(a),y=relativeLuminance(b);return x>y?(x+.05f)/(y+.05f):(y+.05f)/(x+.05f); }

static float noTooltips(const char *name){(void)name;return 0;}
static int TCE_FloatIsZeroOrNaN(float value){return value==0 || isnan(value);}
static int TCE_AddInt32(int a,int b){return (int)((unsigned)a+(unsigned)b);}
static void Item_Paint(itemDef_t *item){(void)item;++itemPaintCalls;}
static void Item_RunScript(itemDef_t *item,qboolean *abort,const char *script)
{(void)item;(void)abort;(void)script;++scriptCalls;}

/* ACTUAL_MENU_PAINT */

static void fresh(void)
{
    TCE_ModernThemeReset();memset(cvars,0,sizeof(cvars));memset(images,0,sizeof(images));
    cvarCount=registrations=updates=imageCount=imageCalls=drawCount=0;enabled=menuActive=1;milliseconds+=32;
    missingImage=NULL;missingAliases=missingAll=clobberVA=0;
    backgroundContext=0;controlEnabled=1;
}
static void refresh(void) { milliseconds+=16;TCE_ModernThemeRefresh(); }

int main(void)
{
    int i,j,previous;vec4_t input={.1f,.2f,.3f,.8f},output,red={.9f,.1f,.1f,1},white={1,1,1,1};
    qhandle_t known,unknown;
    const float expected[10][3]={{.96f,.73f,.27f},{.27f,.76f,.94f},{.98f,.38f,.36f},{.32f,.85f,.66f},
        {.96f,.73f,.27f},{.72f,.76f,.80f},{.10f,.12f,.14f},{.46f,.64f,.82f},{.87f,.53f,.33f},{.71f,.58f,.95f}};
    float unusual[]={bits(0x7fc00000u),bits(0x7f800000u),bits(0xff800000u),-100.f,100.f,0.f,.5f,1.f};
    fresh();enabled=0;TCE_ModernThemeRefresh();check(registrations==0,"No opt-in marker means no new cvars");
    TCE_ModernThemeColor(input,output,1);check(equal(input,output),"No marker preserves legacy surface RGBA");
    unknown=trap_R_RegisterShaderNoMip("models/teams/specops");previous=imageCalls;
    TCE_ModernThemePicture(0,0,8,8,unknown,input);
    check(drawCount==1 && draws[0].shader==unknown && equal(draws[0].color,input),"No marker preserves image rendering");
    check(imageCalls==previous,"No marker avoids optional shader registration");
    drawCount=0;TCE_ModernThemePicture(0,0,8,8,0,white);
    check(drawCount==1 && draws[0].shader==0,"No marker leaves the legacy default-shader path unchanged");
    fresh();TCE_ModernThemeRefresh();check(cvarCount==9 && registrations==9,"Exactly nine archived appearance cvars");
    for(i=0;i<cvarCount;++i)check(cvars[i].flags==CVAR_ARCHIVE,"Every appearance cvar persists locally");
    previous=updates;TCE_ModernThemeRefresh();check(updates==previous,"Refresh runs once in the current 16ms bucket");
    set("ui_themePreset",1);milliseconds=(milliseconds/16+1)*16;TCE_ModernThemeRefresh();
    check(close_float(tceModernAmber[0],.27f),"Palette update visible by next 16ms bucket");
    for(i=0;i<10;++i){set("ui_themePreset",(float)i);refresh();for(j=0;j<3;++j)check(close_float(tceModernAmber[j],expected[i][j]),"Every preset accent including stable Custom4 defaults");}
    set("ui_themePreset",6);refresh();
    check(tceModernWhite[0]<.1f && tceModernMuted[0]<.2f,"Grey/Black preset selects readable dark text");
    { vec4_t mapped;qhandle_t panel=trap_R_RegisterShaderNoMip("tce2_ui/panel");
      TCE_ModernThemeColor(white,mapped,0);drawCount=0;
      TCE_ModernThemePicture(0,0,20,20,panel,mapped);
      check(drawCount==2,"Grey/Black panel has two valid masks");
      for(j=0;j<3;++j)check(close_float(draws[0].color[j],modernTheme.base[j]),"Mapped dark text color must not darken a light panel a second time"); }
    set("ui_themePreset",5);refresh();
    check(tceModernWhite[0]>.9f && tceModernMuted[0]>.5f,"Black/Grey preset selects readable light text");
    set("ui_themePreset",999);refresh();
    for(j=0;j<3;++j)check(close_float(tceModernAmber[j],expected[9][j]),"Out-of-range palette safely clamps to the last preset");
    set("ui_themePreset",4);refresh();
    for(j=0;j<3;++j)check(close_float(tceModernAmber[j],expected[4][j]),"Custom4 is preserved after selecting added presets");
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
    /* Controls are foreground affordances, not another dark panel surface.
     * Exercise their actual masks and the original native image aliases. */
    { static const struct { const char *name;int track; } controls[]={
        {"tce2_ui/slider_track",1},{"tce2_ui/slider_thumb",0},
        {"tce2_ui/scroll_track",1},{"tce2_ui/scroll_thumb",0},
        {"tce2_ui/check_off",0},{"tce2_ui/check_on",0},
        {"tce2_ui/radio_off",0},{"tce2_ui/radio_on",0},
        {"ui/assets/slider2.tga",1},{"ui/assets/sliderbutt_1.tga",0},
        {"ui/assets/scrollbar.tga",1},{"ui/assets/scrollbar_thumb.tga",0},
        {"ui/assets/check_no.tga",0},{"ui/assets/check.tga",0}};
      fresh();TCE_ModernThemeRefresh();set("ui_themeOpacityPreset",3);set("ui_themeOpacity",.25f);
      for(i=1;i<10;++i){
        set("ui_themePreset",(float)i);refresh();
        for(j=0;j<(int)(sizeof(controls)/sizeof(controls[0]));++j){
          int channel;const float *ink=controls[j].track?tceModernMuted:tceModernWhite;
          known=trap_R_RegisterShaderNoMip(controls[j].name);drawCount=0;
          TCE_ModernThemePicture(0,0,20,20,known,white);
          check(drawCount==2,"Control has valid neutral body and accent masks");
          for(channel=0;channel<3;++channel)
            check(close_float(draws[0].color[channel],ink[channel]),"Control body uses contrasting ink instead of panel surface RGB");
          check(close_float(draws[0].color[3],.25f)&&close_float(draws[1].color[3],.25f),"Control low opacity is applied once to each disjoint layer");
        }
      }
      set("ui_themePreset",4);set("ui_themeBackgroundR",1);set("ui_themeBackgroundG",1);set("ui_themeBackgroundB",1);refresh();
      known=trap_R_RegisterShaderNoMip("tce2_ui/slider_thumb");drawCount=0;TCE_ModernThemePicture(0,0,20,20,known,white);
      check(draws[0].color[0]<.1f,"White custom surfaces retain a dark visible slider handle");
      set("ui_themeBackgroundR",0);set("ui_themeBackgroundG",0);set("ui_themeBackgroundB",0);refresh();
      drawCount=0;TCE_ModernThemePicture(0,0,20,20,known,white);
      check(draws[0].color[0]>.9f,"Black custom surfaces retain a light visible slider handle");
    }
    /* Real engines return handle0 for missing shaders. It must never match a
     * cached alias or be rendered as the default checkerboard texture. */
    fresh();missingAliases=1;TCE_ModernThemeRefresh();set("ui_themePreset",4);
    set("ui_themeOpacityPreset",3);set("ui_themeOpacity",.25f);refresh();
    known=trap_R_RegisterShaderNoMip("tce2_ui/button");drawCount=0;
    TCE_ModernThemePicture(0,0,852,480,known,white);
    check(drawCount==2 && draws[0].shader>0 && draws[1].shader>0,"Missing native aliases do not break valid modern masks");
    check(modernTheme.imageCount==51,"Failed aliases are absent from the usable image cache");
    previous=imageCalls;drawCount=0;TCE_ModernThemePicture(0,0,852,480,0,white);
    check(drawCount==0,"Missing original handle0 never draws a fullscreen replacement/default shader");
    check(imageCalls==previous,"Invalid original does not retry failed registrations");
    { Window window;memset(&window,0,sizeof(window));
      services.setColor=trap_R_SetColor;services.drawHandlePic=UI_DrawHandlePic;
      services.fillRect=fill;services.drawRect=border;
      window.rect.w=852;window.rect.h=480;window.style=WINDOW_STYLE_SHADER;
      window.flags=WINDOW_VISIBLE|WINDOW_FORECOLORSET;memcpy(window.foreColor,white,sizeof(white));
      drawCount=0;Window_Paint(&window,.075f,.55f,20);
      check(drawCount==0,"Actual shader-style window with background0 cannot paint a missing-image overlay");
      window.style=WINDOW_STYLE_FILLED;memcpy(window.backColor,input,sizeof(input));
      drawCount=0;Window_Paint(&window,.075f,.55f,20);
      check(drawCount==1 && close_float(draws[0].color[3],input[3]*.25f),"Actual untextured filled window still paints with exactly one opacity factor"); }
    for(j=0;j<2;++j){
        fresh();missingImage=j ? "tce2_ui/theme/button_accent" : "tce2_ui/theme/button_body";
        TCE_ModernThemeRefresh();set("ui_themePreset",4);set("ui_themeOpacityPreset",3);set("ui_themeOpacity",.25f);refresh();
        known=trap_R_RegisterShaderNoMip("tce2_ui/button");drawCount=0;
        TCE_ModernThemePicture(0,0,8,8,known,white);
        check(drawCount==1 && draws[0].shader==known,"Either missing mask falls back to the valid original artwork");
        check(close_float(draws[0].color[3],.25f),"Missing-mask fallback applies low custom opacity exactly once");
        previous=imageCalls;drawCount=0;TCE_ModernThemePicture(0,0,8,8,known,white);
        check(imageCalls==previous && drawCount==1,"Missing-mask result is cached without repeated shader registration");
    }
    fresh();missingImage="tce2_ui/button";TCE_ModernThemeRefresh();set("ui_themePreset",4);refresh();drawCount=0;
    TCE_ModernThemePicture(0,0,852,480,0,white);
    check(drawCount==0,"Failed modern original0 is not replaced by unrelated palette layers");
    fresh();missingAll=1;TCE_ModernThemeRefresh();set("ui_themePreset",4);refresh();drawCount=0;
    TCE_ModernThemePicture(0,0,8,8,77,white);previous=imageCalls;
    TCE_ModernThemePicture(0,0,8,8,77,white);
    check(drawCount==2 && draws[0].shader==77 && draws[1].shader==77,"All optional shader failures preserve valid unknown original");
    check(imageCalls==previous,"Empty optional image cache is initialized only once");
    modernTheme.imageCount=(int)(sizeof(modernThemeImages)/sizeof(modernThemeImages[0]));previous=imageCalls;
    TCE_ThemeRegisterImage("tce2_ui/unexpected_extra","unexpected_extra");
    check(imageCalls==previous && modernTheme.imageCount==80,"Future extra image aliases cannot overflow the fixed cache");
    fresh();clobberVA=1;
    { const char *name=va("tce2_ui/icons/%s","play"),*target=va("icons/%s","play");
      TCE_ThemeRegisterImage(name,target);
      check(modernTheme.imageCount==1 && modernThemeImages[0].body>0 && modernThemeImages[0].accent>0,"Image paths survive real SDK va reuse during renderer callbacks");
      check(!strcmp(images[modernThemeImages[0].body-1].name,"tce2_ui/theme/icons/play_body"),"Body path is frozen before callbacks");
      check(!strcmp(images[modernThemeImages[0].accent-1].name,"tce2_ui/theme/icons/play_accent"),"Accent path is frozen before callbacks"); }
    /* Normal startup activates this nonvisual music menu. Unlike the shader
     * Window path, its direct fullscreen draw bypassed the palette guard. */
    { menuDef_t music,previous;itemDef_t item;
      fresh();TCE_ModernThemeRefresh();set("ui_themePreset",6);set("ui_themeOpacityPreset",3);set("ui_themeOpacity",.25f);refresh();
      memset(&music,0,sizeof(music));memset(&previous,0,sizeof(previous));memset(&item,0,sizeof(item));
      services.drawHandlePic=UI_DrawHandlePic;services.getCVarValue=noTooltips;services.realTime=10;
      music.window.name="backgroundmusic";music.fullScreen=qtrue;music.window.flags=WINDOW_VISIBLE;
      music.onOpen="playlooped sound/music/loopMenu045tuned.wav";
      tceMenuViewportPaintMenu=&previous;drawCount=0;Menu_Paint(&music,qfalse);
      check(drawCount==0,"Grey/Black at opacity.25 cannot expose fullscreen music shader0");
      check(music.fullScreen && (music.window.flags&WINDOW_VISIBLE) && music.onOpen && music.openTime==10,"Guard preserves music menu flags, script and normal paint lifecycle");
      check(tceMenuViewportPaintMenu==&previous,"Music menu paint scope restores its caller");
      music.itemCount=1;music.items[0]=&item;music.timeout=1;music.onTimeout="test_timeout";music.openTime=5;
      itemPaintCalls=scriptCalls=0;Menu_Paint(&music,qfalse);
      check(itemPaintCalls==1&&scriptCalls==1,"Untextured fullscreen menu still paints its items and executes timeout");
      music.itemCount=music.timeout=0;music.window.background=77;drawCount=0;Menu_Paint(&music,qfalse);
      check(drawCount==1&&draws[0].shader==77,"Valid fullscreen backgrounds retain their original direct draw");
      enabled=0;music.window.background=0;drawCount=0;Menu_Paint(&music,qfalse);
      check(drawCount==1&&draws[0].shader==0,"Marker-disabled fullscreen shader0 retains the exact legacy path");
      tceMenuViewportPaintMenu=NULL;
    }
    { Window background;vec4_t black={0,0,0,1};
      fresh();TCE_ModernThemeRefresh();set("ui_themePreset",6);set("ui_themeOpacityPreset",3);set("ui_themeOpacity",.25f);refresh();
      backgroundContext=1;memset(&background,0,sizeof(background));background.rect.w=852;background.rect.h=480;
      background.style=WINDOW_STYLE_FILLED;memcpy(background.backColor,black,sizeof(black));
      drawCount=0;Window_Paint(&background,.075f,.55f,20);
      check(drawCount==1&&equal(draws[0].color,black),"Background menu black fill is independent of palette and menu opacity");
      background.style=WINDOW_STYLE_SHADER;background.flags=WINDOW_VISIBLE|WINDOW_FORECOLORSET;
      memcpy(background.foreColor,white,sizeof(white));background.background=trap_R_RegisterShaderNoMip("tce2_ui/background");
      drawCount=0;Window_Paint(&background,.075f,.55f,20);
      check(drawCount==1&&draws[0].shader==background.background&&equal(draws[0].color,white),"Background photo keeps white tint and alpha1 under Grey/Black opacity.25");
      backgroundContext=0;
    }
    /* Execute the production item focus painter, not synthetic menu scripts.
     * The startup Main's transparent text-only button needs an affordance even
     * though normal/hover neutral text map to the same palette foreground. */
    { itemDef_t item,saved;int preset,opacity,channel;
      static const float alphas[]={.25f,.60f,.85f,1.f};
      fresh();TCE_ModernThemeRefresh();services.fillRect=fill;services.drawRect=border;
      memset(&item,0,sizeof(item));item.type=ITEM_TYPE_BUTTON;
      item.window.flags=WINDOW_VISIBLE|WINDOW_MOUSEOVER;
      item.window.rect.x=64;item.window.rect.y=130;item.window.rect.w=128;item.window.rect.h=27;
      item.window.style=WINDOW_STYLE_FILLED;item.window.backColor[3]=0;
      for(preset=0;preset<10;++preset)for(opacity=0;opacity<4;++opacity){
        vec4_t oldAccent;
        set("ui_themePreset",(float)preset);set("ui_themeOpacityPreset",3);set("ui_themeOpacity",alphas[opacity]);refresh();
        memcpy(oldAccent,tceModernAmber,sizeof(oldAccent));saved=item;drawCount=0;TCE_ModernItemFocusPaint(&item);
        check(drawCount==4,"Every palette/opacity gives transparent Main buttons a visible focus treatment");
        check(!memcmp(&item,&saved,sizeof(item)),"Focus paint never changes native layout, scripts or parsed item state");
        check(close_float(draws[0].color[3],.14f),"Focus wash is subtle and fixed, not multiplied by global menu opacity");
        check(draws[1].color[3]==1 && draws[2].color[3]==1,"Both narrow focus rims stay opaque at glass/custom opacity");
        check(contrast(draws[2].color,modernTheme.base)>=4.5f,"Inner focus rim has at least4.5:1 contrast against the selected palette base");
        check(contrast(draws[1].color,draws[2].color)>20.f,"Opposing rims remain distinguishable across bright/dark scene details");
        check(equal(oldAccent,tceModernAmber),"Hover does not replace or mutate the selected accent");
        for(channel=0;channel<3;++channel)check(close_float(draws[3].color[channel],oldAccent[channel]),"Focus indicator retains each original accent component");
        for(channel=0;channel<4;++channel){
          check(draws[channel].x>=item.window.rect.x && draws[channel].y>=item.window.rect.y,"Focus treatment stays inside its existing hitbox");
          check(draws[channel].x+draws[channel].w<=item.window.rect.x+item.window.rect.w && draws[channel].y+draws[channel].h<=item.window.rect.y+item.window.rect.h,"Focus does not cover neighboring controls");
        }
      }
      /* Saturated colors need real sRGB luminance, not an average-RGB guess.
       * Exercise all combinations of five custom surface levels and eight
       * accent extremes without changing either user's configured color. */
      set("ui_themePreset",4);
      for(i=0;i<125;++i)for(j=0;j<8;++j){
        vec4_t wash,edge,rim,accent,scene;
        set("ui_themeBackgroundR",(float)(i%5)/4);set("ui_themeBackgroundG",(float)((i/5)%5)/4);set("ui_themeBackgroundB",(float)(i/25)/4);
        set("ui_themeAccentR",(float)(j&1));set("ui_themeAccentG",(float)((j>>1)&1));set("ui_themeAccentB",(float)((j>>2)&1));refresh();
        check(TCE_ModernThemeFocusColors(wash,edge,rim,accent),"Custom palette focus colors are available in menu context");
        check(contrast(edge,modernTheme.base)>=4.5f,"All custom palette bases retain a high-contrast inner rim");
        for(channel=0;channel<3;++channel)check(accent[channel]==tceModernAmber[channel],"Custom RGB accent remains exact, including black/white extremes");
        for(channel=0;channel<11;++channel){
          float a,b;scene[0]=scene[1]=scene[2]=(float)channel/10;scene[3]=1;
          a=contrast(scene,edge);b=contrast(scene,rim);
          check((a>b?a:b)>=4.5f,"At least one opaque rim remains visible over every scene brightness, independent of menu transparency");
        }
      }
      { static const int types[]={ITEM_TYPE_BUTTON,ITEM_TYPE_RADIOBUTTON,ITEM_TYPE_CHECKBOX,ITEM_TYPE_TRICHECKBOX,
          ITEM_TYPE_EDITFIELD,ITEM_TYPE_NUMERICFIELD,ITEM_TYPE_YESNO,ITEM_TYPE_MULTI,ITEM_TYPE_BIND,
          ITEM_TYPE_SLIDER};int type,focus,style;
        for(type=0;type<(int)(sizeof(types)/sizeof(types[0]));++type)for(focus=0;focus<2;++focus)for(style=1;style<=3;++style){
          item.type=types[type];item.window.flags=WINDOW_VISIBLE|(focus?WINDOW_HASFOCUS:WINDOW_MOUSEOVER);item.window.style=style;
          drawCount=0;TCE_ModernItemFocusPaint(&item);check(drawCount==4,"All interactive control styles expose both mouse and keyboard focus");
        }
      }
      item.type=ITEM_TYPE_BUTTON;item.window.flags=WINDOW_VISIBLE|WINDOW_MOUSEOVER;item.cvarFlags=CVAR_ENABLE;
      controlEnabled=0;drawCount=0;TCE_ModernItemFocusPaint(&item);check(!drawCount,"Disabled cvar-driven controls never look clickable");
      controlEnabled=1;item.cvarFlags=0;
      for(i=0;i<8;++i){
        item.type=ITEM_TYPE_BUTTON;item.window.flags=WINDOW_VISIBLE|WINDOW_MOUSEOVER;
        if(i==0)item.window.flags|=WINDOW_DECORATION;
        if(i==1)item.window.flags|=WINDOW_GREY;
        if(i==2)item.window.flags|=WINDOW_FADINGOUT;
        if(i==3)item.window.flags&=~WINDOW_VISIBLE;
        if(i==4)item.type=ITEM_TYPE_OWNERDRAW;
        if(i==5)item.type=ITEM_TYPE_LISTBOX;
        if(i==6)item.type=ITEM_TYPE_COMBO;
        if(i==7)item.type=ITEM_TYPE_TEXT;
        drawCount=0;TCE_ModernItemFocusPaint(&item);check(!drawCount,"Decorative, disabled, hidden, fading and non-control surfaces stay unchanged");
      }
      item.type=ITEM_TYPE_BUTTON;item.window.flags=WINDOW_VISIBLE|WINDOW_MOUSEOVER;
      enabled=0;drawCount=0;TCE_ModernItemFocusPaint(&item);check(!drawCount,"Marker-disabled legacy menu behavior is untouched");
      enabled=1;backgroundContext=1;TCE_ModernItemFocusPaint(&item);check(!drawCount,"Fullscreen scene artwork never gets control focus paint");backgroundContext=0;
#ifdef CGAMEDLL
      menuActive=0;TCE_ModernItemFocusPaint(&item);check(!drawCount,"Gameplay HUD outside a menu scope never receives focus paint");menuActive=1;
#endif
    }
    printf("checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
