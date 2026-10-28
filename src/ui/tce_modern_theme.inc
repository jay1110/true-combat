/* Optional menu paint preferences. Never used for gameplay rendering/state. */
typedef struct {
    vmCvar_t preset, opacityPreset, opacity, accent[3], background[3];
    int initialized, tick, imageCount;
    float alpha, base[3];
} tce_theme_t;
static tce_theme_t modernTheme;
static struct { qhandle_t original, body, accent; } modernThemeImages[80];

static float TCE_ThemeClamp(float value, float fallback, float low, float high)
{
    if (!(value >= -3.402823466e38f && value <= 3.402823466e38f)) return fallback;
    return value < low ? low : value > high ? high : value;
}

void TCE_ModernThemeRefresh(void)
{
    static const float accents[4][3] = {
        { .96f,.73f,.27f }, { .27f,.76f,.94f }, { .98f,.38f,.36f }, { .32f,.85f,.66f }
    };
    static const float bases[4][3] = {
        { .035f,.055f,.075f }, { .025f,.055f,.075f }, { .065f,.030f,.045f }, { .025f,.060f,.050f }
    };
    static const char *channels[] = { "R", "G", "B" };
    static const char *accentDefault[] = { ".96", ".73", ".27" };
    static const char *baseDefault[] = { ".035", ".055", ".075" };
    int i, preset, opacityPreset, tick = trap_Milliseconds() / 16;
    float luminance;
    if (!TCE_ModernUIEnabled()) return;
    if (!modernTheme.initialized)
    {
        trap_Cvar_Register(&modernTheme.preset,"ui_themePreset","0",CVAR_ARCHIVE);
        trap_Cvar_Register(&modernTheme.opacityPreset,"ui_themeOpacityPreset","0",CVAR_ARCHIVE);
        trap_Cvar_Register(&modernTheme.opacity,"ui_themeOpacity",".85",CVAR_ARCHIVE);
        for (i=0;i<3;++i)
        {
            trap_Cvar_Register(&modernTheme.accent[i],va("ui_themeAccent%s",channels[i]),accentDefault[i],CVAR_ARCHIVE);
            trap_Cvar_Register(&modernTheme.background[i],va("ui_themeBackground%s",channels[i]),baseDefault[i],CVAR_ARCHIVE);
        }
        modernTheme.initialized=1; modernTheme.tick=tick-1;
    }
    if (modernTheme.tick==tick) return;
    modernTheme.tick=tick;
    trap_Cvar_Update(&modernTheme.preset); trap_Cvar_Update(&modernTheme.opacityPreset);
    trap_Cvar_Update(&modernTheme.opacity);
    preset=(int)TCE_ThemeClamp(modernTheme.preset.value,0,0,4);
    opacityPreset=(int)TCE_ThemeClamp(modernTheme.opacityPreset.value,0,0,3);
    for (i=0;i<3;++i)
    {
        trap_Cvar_Update(&modernTheme.accent[i]); trap_Cvar_Update(&modernTheme.background[i]);
        tceModernAmber[i]=preset==4 ? TCE_ThemeClamp(modernTheme.accent[i].value,accents[0][i],0,1) : accents[preset][i];
        modernTheme.base[i]=preset==4 ? TCE_ThemeClamp(modernTheme.background[i].value,bases[0][i],0,1) : bases[preset][i];
    }
    modernTheme.alpha=opacityPreset==0 ? 1.f : opacityPreset==1 ? .85f : opacityPreset==2 ? .60f :
                      TCE_ThemeClamp(modernTheme.opacity.value,.85f,.25f,1.f);
    luminance=.2126f*modernTheme.base[0]+.7152f*modernTheme.base[1]+.0722f*modernTheme.base[2];
    for (i=0;i<3;++i)
    {
        tceModernWhite[i]=luminance>.45f ? .07f : (i==0 ? .93f : i==1 ? .96f : .98f);
        tceModernMuted[i]=luminance>.45f ? .19f : (i==0 ? .55f : i==1 ? .64f : .72f);
    }
}

void TCE_ModernThemeColor(const float *source, float *out, int surface)
{
    float highest, lowest, brightness;
    int i, preset;
    if (!source) { for(i=0;i<4;++i) out[i]=1.f; return; }
    memcpy(out,source,sizeof(vec4_t));
    if (!TCE_ModernUIEnabled()) return;
#ifdef CGAMEDLL
    if (!CG_MenuViewportActive()) return;
#endif
    TCE_ModernThemeRefresh();
    preset=(int)TCE_ThemeClamp(modernTheme.preset.value,0,0,4);
    highest=source[0]>source[1] ? source[0] : source[1]; if(source[2]>highest) highest=source[2];
    lowest=source[0]<source[1] ? source[0] : source[1]; if(source[2]<lowest) lowest=source[2];
    if (preset && (source==tceModernAmber || (source[0]>.55f && source[1]>.35f && source[2]<.55f && source[0]>source[2]*1.4f)))
    {
        for(i=0;i<3;++i) out[i]=tceModernAmber[i];
    }
    else if (preset && surface && highest<=.4f)
    {
        brightness=.65f+highest*3.f;
        for(i=0;i<3;++i) out[i]=TCE_ThemeClamp(modernTheme.base[i]*brightness+(surface==2?.09f:0.f),0,0,1);
    }
    else if (preset && !surface && highest-lowest<.20f && lowest>.45f)
    {
        for(i=0;i<3;++i) out[i]=lowest>.73f ? tceModernWhite[i] : tceModernMuted[i];
    }
    /* Text and team colors stay opaque; only menu surfaces use this preference. */
    if(surface) out[3]*=modernTheme.alpha;
}

static void TCE_ThemeRegisterImage(const char *name, const char *target)
{
    int i=modernTheme.imageCount++;
    modernThemeImages[i].original=trap_R_RegisterShaderNoMip(name);
    modernThemeImages[i].body=trap_R_RegisterShaderNoMip(va("tce2_ui/theme/%s_body",target));
    modernThemeImages[i].accent=trap_R_RegisterShaderNoMip(va("tce2_ui/theme/%s_accent",target));
}

static void TCE_ThemeImages(void)
{
    static const char *widgets[]={ "panel","panel_header","card","button","button_hover","button_pressed","button_disabled",
        "tab","tab_hover","tab_active","selector","selector_focus","slider_track","slider_thumb","slider_thumb_hover",
        "scroll_track","scroll_thumb","scroll_thumb_hover","check_off","check_on","check_mixed","radio_off","radio_on","separator","focus" };
    static const char *icons[]={ "play","multiplayer","host","replays","settings","profile","credits","exit","graphics","audio","controls","network",
        "chevron_up","chevron_down","chevron_left","chevron_right","close","check","search","refresh","folder","download","lock","warning","info","server" };
    static const char *aliases[][2]={ {"scrollbar","scroll_track"},{"scrollbar_arrow_dwn_a","icons/chevron_down"},
        {"scrollbar_arrow_up_a","icons/chevron_up"},{"scrollbar_arrow_left","icons/chevron_left"},{"scrollbar_arrow_right","icons/chevron_right"},
        {"scrollbar_thumb","scroll_thumb"},{"slider2","slider_track"},{"sliderbutt_1","slider_thumb"},
        {"check","check_on"},{"check_not","check_mixed"},{"check_no","check_off"} };
    int i;
    if(modernTheme.imageCount) return;
    for(i=0;i<(int)(sizeof(widgets)/sizeof(widgets[0]));++i) TCE_ThemeRegisterImage(va("tce2_ui/%s",widgets[i]),widgets[i]);
    for(i=0;i<(int)(sizeof(icons)/sizeof(icons[0]));++i) TCE_ThemeRegisterImage(va("tce2_ui/icons/%s",icons[i]),va("icons/%s",icons[i]));
    for(i=0;i<(int)(sizeof(aliases)/sizeof(aliases[0]));++i) TCE_ThemeRegisterImage(va("ui/assets/%s.tga",aliases[i][0]),aliases[i][1]);
}

static void TCE_ThemeRawPicture(float x,float y,float w,float h,qhandle_t shader)
{
#ifdef CGAMEDLL
    CG_DrawPic(x,y,w,h,shader);
#else
    UI_DrawHandlePic(x,y,w,h,shader);
#endif
}

void TCE_ModernThemePicture(float x,float y,float w,float h,qhandle_t shader,const float *source)
{
    vec4_t tint,body;
    int i,preset,found=-1;
#ifdef CGAMEDLL
    if (!CG_MenuViewportActive())
    {
        trap_R_SetColor(source); TCE_ThemeRawPicture(x,y,w,h,shader); return;
    }
#endif
    TCE_ModernThemeRefresh();
    if(TCE_ModernUIEnabled())
    {
        TCE_ThemeImages();
        for(i=0;i<modernTheme.imageCount;++i) if(modernThemeImages[i].original==shader) { found=i;break; }
    }
    if(source) memcpy(tint,source,sizeof(tint)); else for(i=0;i<4;++i) tint[i]=1.f;
    if(found<0)
    {
        if(source && source[0]<=.4f && source[1]<=.4f && source[2]<=.4f)
        {
            TCE_ModernThemeColor(source,tint,1);
            trap_R_SetColor(tint); TCE_ThemeRawPicture(x,y,w,h,shader); trap_R_SetColor(source);
        }
        else { trap_R_SetColor(source); TCE_ThemeRawPicture(x,y,w,h,shader); }
        return;
    }
    tint[3]*=modernTheme.alpha;
    preset=(int)TCE_ThemeClamp(modernTheme.preset.value,0,0,4);
    if(!preset)
    {
        trap_R_SetColor(tint); TCE_ThemeRawPicture(x,y,w,h,shader);
    }
    else
    {
        /* Palette-colored icons are masks, not a second multiplication of the
         * selected accent. Preserve neutral disabled modulation and alpha. */
        if ((tint[0]>.55f && tint[1]>.35f && tint[2]<.55f && tint[0]>tint[2]*1.4f) ||
            (fabsf(tint[0]-tceModernAmber[0])<.001f && fabsf(tint[1]-tceModernAmber[1])<.001f && fabsf(tint[2]-tceModernAmber[2])<.001f) ||
            (tint[0]>.73f && tint[1]>.73f && tint[2]>.73f))
            tint[0]=tint[1]=tint[2]=1.f;
        for(i=0;i<3;++i) body[i]=TCE_ThemeClamp(modernTheme.base[i]*4.f,0,0,1)*tint[i];
        body[3]=tint[3]; trap_R_SetColor(body); TCE_ThemeRawPicture(x,y,w,h,modernThemeImages[found].body);
        for(i=0;i<3;++i) body[i]=tceModernAmber[i]*tint[i];
        trap_R_SetColor(body); TCE_ThemeRawPicture(x,y,w,h,modernThemeImages[found].accent);
    }
    trap_R_SetColor(source);
}

static void TCE_ModernThemeReset(void)
{
    memset(&modernTheme,0,sizeof(modernTheme));
    memset(modernThemeImages,0,sizeof(modernThemeImages));
    tceModernWhite[0]=.93f;tceModernWhite[1]=.96f;tceModernWhite[2]=.98f;
    tceModernMuted[0]=.55f;tceModernMuted[1]=.64f;tceModernMuted[2]=.72f;
    tceModernAmber[0]=.96f;tceModernAmber[1]=.73f;tceModernAmber[2]=.27f;
}
