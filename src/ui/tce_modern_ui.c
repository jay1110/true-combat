/* Shared by the native UI and CGame, not by the engine or QAGame. */
#ifdef CGAMEDLL
#include "../cgame/cg_local.h"
#else
#include "ui_local.h"
extern int Text_Width_Ext(const char *text, float scale, int limit, fontInfo_t *font);
#endif
#include "tce_modern_ui.h"

static int modernChecked, modernEnabled, modernFonts;
static fontInfo_t modernRegular, modernStrong;
static struct { char name[MAX_QPATH]; qhandle_t handle; } modernImages[16];
static int modernImageCount;
vec4_t tceModernWhite = { .93f, .96f, .98f, 1 };
vec4_t tceModernMuted = { .55f, .64f, .72f, 1 };
vec4_t tceModernAmber = { .96f, .73f, .27f, 1 };

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

void TCE_ModernFill(float x, float y, float w, float h, const float *color)
{
#ifdef CGAMEDLL
    CG_FillRect(x, y, w, h, color);
#else
    UI_FillRect(x, y, w, h, color);
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
    trap_R_SetColor(NULL);
#ifdef CGAMEDLL
    CG_DrawPic(x,y,w,h,image);
#else
    UI_DrawHandlePic(x,y,w,h,image);
#endif
}

void TCE_ModernPanel(float x, float y, float w, float h, float alpha)
{
    vec4_t back = { .035f, .055f, .075f, 1 };
    back[3]=alpha; TCE_ModernFill(x,y,w,h,back);
    back[0]=.16f; back[1]=.22f; back[2]=.28f;
    TCE_ModernFill(x,y,w,1,back);
}

void TCE_ModernText(float x, float y, float scale, float width,
                    const float *color, const char *text, int strong)
{
    char clipped[2048]; size_t length;
    vec4_t rgba; fontInfo_t *font=TCE_ModernFont(strong);
    if (!text || width <= 0) return;
    Q_strncpyz(clipped,text,sizeof(clipped)); length=strlen(clipped);
    while (length && TCE_ModernWidth(clipped,scale,font)>width) clipped[--length]=0;
    if (length && clipped[length-1]=='^') clipped[length-1]=0;
    memcpy(rgba,color,sizeof(rgba));
#ifdef CGAMEDLL
    CG_Text_Paint_Ext(x,y,scale,scale,rgba,clipped,0,0,0,font);
#else
    Text_Paint_Ext(x,y,scale,scale,rgba,clipped,0,0,0,font);
#endif
}

void TCE_ModernParagraph(float x, float y, float scale, float width,
                         float lineHeight, int lines, const float *color, const char *text)
{
    const char *p=text; char line[512]; int row;
    fontInfo_t *font=TCE_ModernFont(0);
    if (!p || lines<=0) return;
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
        TCE_ModernText(x,y+row*lineHeight,scale,width,color,line,0);
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
