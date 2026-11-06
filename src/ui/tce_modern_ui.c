/* Shared by the native UI and CGame, not by the engine or QAGame. */
#ifdef CGAMEDLL
#include "../cgame/cg_local.h"
#include "../cgame/cg_hud_viewport.h"
#else
#include "ui_local.h"
extern int Text_Width_Ext(const char *text, float scale, int limit, fontInfo_t *font);
#endif
#include "tce_modern_ui.h"
#include "tce_viewport.h"

static int modernChecked, modernEnabled, modernFonts;
static fontInfo_t modernRegular, modernStrong;
static struct { char name[MAX_QPATH]; qhandle_t handle; } modernImages[16];
static int modernImageCount;
vec4_t tceModernWhite = { .93f, .96f, .98f, 1 };
vec4_t tceModernMuted = { .55f, .64f, .72f, 1 };
vec4_t tceModernAmber = { .96f, .73f, .27f, 1 };

#include "tce_modern_theme.inc"

int TCE_ModernMarkerValid(const char *data, int length)
{
    static const char marker[] = "TCE2_MODERN_UI 1\n";
    return data && length == (int)sizeof(marker)-1 && !memcmp(data, marker, sizeof(marker)-1);
}

float TCE_ModernProgress(int used, int expected)
{
    if (expected <= 0) return -1;
    if (used <= 0) return 0;
    if (used >= expected) return 1;
    return (float)((double)used / (double)expected);
}

int TCE_ModernRosterPages(int count, int rows)
{
    if (rows <= 0 || count <= 0) return 1;
    if (count > MAX_CLIENTS) count = MAX_CLIENTS;
    return 1 + (count-1)/rows;
}

int TCE_ModernHiddenStats(int gameType, int killMessages, int health, int pmType)
{
    return gameType == 5 && !killMessages && health > 0 && pmType != PM_INTERMISSION;
}

void TCE_ModernUIReset(void)
{
    TCE_ModernThemeReset();
    modernChecked = modernEnabled = modernFonts = modernImageCount = 0;
    memset(modernImages, 0, sizeof(modernImages));
    memset(&modernRegular, 0, sizeof(modernRegular));
    memset(&modernStrong, 0, sizeof(modernStrong));
}

qboolean TCE_ModernUIEnabled(void)
{
    if (!modernChecked)
    {
        fileHandle_t file = 0;
        char marker[32];
        int length = trap_FS_FOpenFile("tce2_ui/runtime.cfg", &file, FS_READ);
        modernChecked = 1;
        if (file)
        {
            if (length > 0 && length < (int)sizeof(marker))
            {
                trap_FS_Read(marker, length, file);
                modernEnabled = TCE_ModernMarkerValid(marker, length);
            }
            trap_FS_FCloseFile(file);
        }
    }
    return modernEnabled ? qtrue : qfalse;
}

static fontInfo_t *TCE_ModernFont(int strong)
{
    if (!modernFonts)
    {
        trap_R_RegisterFont("tce2_ui/Barlow-Regular", 24, &modernRegular);
        trap_R_RegisterFont("tce2_ui/Barlow-SemiBold", 28, &modernStrong);
        if (!(modernRegular.glyphScale > 0)) trap_R_RegisterFont("courbd", 30, &modernRegular);
        if (!(modernStrong.glyphScale > 0)) trap_R_RegisterFont("courbd", 30, &modernStrong);
        modernFonts = 1;
    }
    return strong ? &modernStrong : &modernRegular;
}

static float TCE_ModernWidth(const char *text, float scale, fontInfo_t *font)
{
#ifdef CGAMEDLL
    return (float)CG_Text_Width_Ext(text, scale, 0, font);
#else
    return (float)Text_Width_Ext(text, scale, 0, font);
#endif
}

float TCE_ModernReadableScale(float scale, const fontInfo_t *font)
{
    tce_viewport_t view;
    float cap, preferred, maximum, menuScale;
    int width, height, full = TCE_ViewportDrawMode() == TCE_VIEWPORT_FULLSCREEN;
    if (!TCE_ModernUIEnabled() || !font || !(scale > 0.f) || !(font->glyphScale > 0.f)) return scale;
#ifdef CGAMEDLL
    width = cgs.glconfig.vidWidth; height = cgs.glconfig.vidHeight;
    maximum = (float)CG_Cvar_Get("ui_menuMaxHeight");
    menuScale = (float)CG_Cvar_Get("ui_menuScale");
#else
    width = uiInfo.uiDC.glconfig.vidWidth; height = uiInfo.uiDC.glconfig.vidHeight;
    maximum = trap_Cvar_VariableValue("ui_menuMaxHeight");
    menuScale = trap_Cvar_VariableValue("ui_menuScale");
#endif
    if (!TCE_ViewportFit(&view,width,height,TCE_VIEWPORT_CANVAS_WIDTH,
                         TCE_VIEWPORT_CANVAS_HEIGHT,full ? 0 : maximum,
                         full ? 1 : menuScale,.5f,.5f)) return scale;
    /* The outline renderer stores a one-pixel top guard. H has no descender;
     * this cap metric works for both fixed DAT and DPI-scaled engine fonts. */
    cap = (float)font->glyphs['H'].top - 1.f;
    if (!(cap > 0.f)) cap = (float)font->glyphs['H'].imageHeight;
    if (!(cap > 0.f)) return scale;
    preferred = 8.f / (cap * font->glyphScale * view.scale);
    return preferred > scale && preferred < 4.f ? preferred : scale;
}

float TCE_ModernInkWidth(const char *text, float scale, int limit, const fontInfo_t *font)
{
    float pen = 0, left = 0, right = 0, unit = scale * font->glyphScale;
    int count = 0;
    while (text && *text && (limit <= 0 || count < limit))
    {
        const glyphInfo_t *glyph;
        float a, b;
        if (Q_IsColorString(text)) { text += 2; continue; }
        if ((unsigned char)*text < 20) { ++text; ++count; continue; }
        glyph = &font->glyphs[(unsigned char)*text++]; ++count;
        a = pen + glyph->pitch * unit; b = a + glyph->imageWidth * unit;
        if (a < left) left = a;
        if (b > right) right = b;
        pen += glyph->xSkip * unit;
    }
    if (pen > right) right = pen;
    return right - left;
}

void TCE_ModernFill(float x, float y, float w, float h, const float *color)
{
    vec4_t themed;
    TCE_ModernThemeColor(color,themed,1);
#ifdef CGAMEDLL
    CG_FillRect(x, y, w, h, themed);
#else
    UI_FillRect(x, y, w, h, themed);
#endif
}

void TCE_ModernImage(const char *name, float x, float y, float w, float h)
{
    qhandle_t image = 0; int i;
    if (!name || !*name) return;
    for (i=0; i<modernImageCount; ++i)
        if (!strcmp(name, modernImages[i].name)) { image=modernImages[i].handle; break; }
    if (i == modernImageCount)
    {
        image = trap_R_RegisterShaderNoMip(name);
        if (modernImageCount < (int)(sizeof(modernImages)/sizeof(modernImages[0])))
        {
            Q_strncpyz(modernImages[i].name, name, sizeof(modernImages[i].name));
            modernImages[i].handle=image; ++modernImageCount;
        }
    }
    if (!image) return;
    TCE_ModernThemePicture(x,y,w,h,image,NULL);
    trap_R_SetColor(NULL);
}

void TCE_ModernBackdrop(const char *name)
{
    int previous = TCE_ViewportSetDrawMode(TCE_VIEWPORT_BACKDROP);
    TCE_ModernImage(name,0,0,TCE_VIEWPORT_CANVAS_WIDTH,TCE_VIEWPORT_CANVAS_HEIGHT);
    TCE_ViewportSetDrawMode(previous);
}

void TCE_ModernCursor(float x, float y, qhandle_t shader, int centered)
{
    tce_viewport_t viewport;
    float size, unused=0, menuMaxHeight, menuScale;
    int width,height,fullscreen=TCE_ViewportDrawMode()==TCE_VIEWPORT_FULLSCREEN;
#ifdef CGAMEDLL
    width=cgs.glconfig.vidWidth; height=cgs.glconfig.vidHeight;
    menuMaxHeight=(float)CG_Cvar_Get("ui_menuMaxHeight");
    menuScale=(float)CG_Cvar_Get("ui_menuScale");
#else
    width=uiInfo.uiDC.glconfig.vidWidth; height=uiInfo.uiDC.glconfig.vidHeight;
    menuMaxHeight=trap_Cvar_VariableValue("ui_menuMaxHeight");
    menuScale=trap_Cvar_VariableValue("ui_menuScale");
#endif
    if(shader<=0 || !TCE_ViewportFit(&viewport,width,height,TCE_VIEWPORT_CANVAS_WIDTH,
        TCE_VIEWPORT_CANVAS_HEIGHT,fullscreen?0:menuMaxHeight,
        fullscreen?1:menuScale,.5f,
        fullscreen?.5f:TCE_MenuViewportAnchorY())) return;
    TCE_ViewportProject(&viewport,&x,&y,&unused,&unused);
    size=TCE_ViewportCursorSize(width,height);
    if(centered) { x-=size*(14.f/32.f); y-=size*(14.f/32.f); }
    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(x,y,size,size,0,0,1,1,shader);
}

void TCE_ModernTextFit(float x, float y, float scale, float width,
                       const float *color, const char *text, int strong)
{
    if(!text || width<=0 || scale<=0) return;
    if (!TCE_ModernUIEnabled())
    {
        fontInfo_t *font = TCE_ModernFont(strong);
        float measured = TCE_ModernWidth(text,scale,font);
        if (measured > width) scale *= width / (measured + 1.f);
    }
    TCE_ModernText(x,y,scale,width,color,text,strong);
}

void TCE_ModernPanel(float x, float y, float w, float h, float alpha)
{
    vec4_t back = { .035f, .055f, .075f, 1 };
    back[3]=alpha; TCE_ModernFill(x,y,w,h,back);
    back[0]=.16f; back[1]=.22f; back[2]=.28f;
    TCE_ModernFill(x,y,w,1,back);
}

static void TCE_ModernTextDraw(float x, float y, float scale, float width,
                              const float *color, const char *text, int strong)
{
    char clipped[2048]; size_t length;
    vec4_t rgba; fontInfo_t *font=TCE_ModernFont(strong);
    if (!text || width <= 0) return;
    if (TCE_ModernUIEnabled())
    {
        float ink = TCE_ModernInkWidth(text,scale,0,font);
        if (ink + .01f > width) scale *= width / (ink + .01f);
    }
    Q_strncpyz(clipped,text,sizeof(clipped)); length=strlen(clipped);
    while (length && TCE_ModernWidth(clipped,scale,font)>width) clipped[--length]=0;
    if (length && clipped[length-1]=='^') clipped[length-1]=0;
    TCE_ModernThemeColor(color,rgba,0);
#ifdef CGAMEDLL
    CG_Text_Paint_Ext(x,y,scale,scale,rgba,clipped,0,0,0,font);
#else
    Text_Paint_Ext(x,y,scale,scale,rgba,clipped,0,0,0,font);
#endif
}

void TCE_ModernText(float x, float y, float scale, float width,
                    const float *color, const char *text, int strong)
{
    fontInfo_t *font = TCE_ModernFont(strong);
    float measured;
    if (!text || !(width > 0.f) || !(scale > 0.f)) return;
    if (!TCE_ModernUIEnabled()) { TCE_ModernTextDraw(x,y,scale,width,color,text,strong); return; }
    scale = TCE_ModernReadableScale(scale,font);
    measured = TCE_ModernWidth(text,scale,font);
    if (measured + 1.f > width) scale *= width / (measured + 1.f);
    TCE_ModernTextDraw(x,y,scale,width,color,text,strong);
}

void TCE_ModernParagraph(float x, float y, float scale, float width,
                         float lineHeight, int lines, const float *color, const char *text)
{
    const char *p=text; char line[512]; int row;
    fontInfo_t *font=TCE_ModernFont(0);
    if (!p || lines<=0) return;
    if (TCE_ModernUIEnabled())
    {
        const char *s = text;
        float top = 0, bottom = 0, candidate = TCE_ModernReadableScale(scale,font);
        while (*s)
        {
            const glyphInfo_t *glyph;
            if (Q_IsColorString(s)) { s += 2; continue; }
            if ((unsigned char)*s < 32 || *s == '*') { ++s; continue; }
            glyph = &font->glyphs[(unsigned char)*s++];
            if (glyph->top > top) top = (float)glyph->top;
            if (glyph->imageHeight - glyph->top > bottom) bottom = (float)(glyph->imageHeight - glyph->top);
        }
        if ((top + bottom) * font->glyphScale * candidate > lineHeight && lineHeight > 0.f)
            candidate = lineHeight / ((top + bottom) * font->glyphScale);
        scale = candidate;
    }
    for (row=0; row<lines && *p; ++row)
    {
        int n=0,lastSpace=-1; const char *start;
        while (*p==' ' || *p=='\r') ++p;
        start=p;
        while (*p && *p!='\n' && *p!='*' && n<(int)sizeof(line)-1)
        {
            line[n++]=*p++; line[n]=0;
            if (line[n-1]==' ') lastSpace=n-1;
            if (TCE_ModernWidth(line,scale,font)>width)
            {
                if (lastSpace>0) { n=lastSpace; p=start+n+1; }
                else if (n>1) { --n; --p; }
                break;
            }
        }
        line[n]=0;
        TCE_ModernTextDraw(x,y+row*lineHeight,scale,width,color,line,0);
        if (*p=='\n' || *p=='*') ++p;
    }
}

void TCE_ModernBar(float x, float y, float w, float h, float progress, int milliseconds)
{
    vec4_t track={ .12f,.18f,.23f,1 };
    TCE_ModernFill(x,y,w,h,track);
    if (progress >= 0)
    {
        if (progress>1) progress=1;
        TCE_ModernFill(x,y,w*progress,h,tceModernAmber);
    }
    else
    {
        /* An indeterminate segment is labelled as such, never a fake percent. */
        int phase=(unsigned)milliseconds%2000u;
        TCE_ModernFill(x+(w*.72f)*(float)phase/2000.f,y,w*.28f,h,tceModernAmber);
    }
}
