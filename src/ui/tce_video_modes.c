#include "ui_local.h"
#include "tce_video_modes.h"

/* Recovered UI_RunMenuScript video branches (Linux 0002919e, Windows
 * apply block 4000a410). Custom-resolution snapshot/restore deliberately adapts
 * the original raw r_mode contract; see ui_script_controller_closure_524.md. */
static const int resolutions[][2] = {
    {640,480},{800,600},{960,720},{1024,768},{1152,864},
    {1280,1024},{1600,1200},{2048,1536},{856,480},
    {1024,576},{1280,720},{1360,768},{1600,900},{1920,1080},
    {1024,640},{1280,800},{1440,900},{1600,1000},{1680,1050},{1920,1200}
};

int TCE_UI_GetVideoMode(void) {
    int mode=(int)trap_Cvar_VariableValue("r_mode"),i,width,height;
    if(mode!=-1)return mode;
    width=(int)trap_Cvar_VariableValue("r_customwidth");
    height=(int)trap_Cvar_VariableValue("r_customheight");
    for(i=9;i<sizeof(resolutions)/sizeof(resolutions[0]);++i)
        if(width==resolutions[i][0]&&height==resolutions[i][1])return i+3;
    return -1; /* Preserve arbitrary user resolutions. */
}

int TCE_UI_ApplyVideoMode(int mode) {
    if(mode>=12) {
        /* Original switch defaults unknown extended modes to 1024x576. */
        trap_Cvar_SetValue("r_mode",-1);
        if(mode>22)mode=12;
        trap_Cvar_SetValue("r_customwidth",resolutions[mode-3][0]);
        trap_Cvar_SetValue("r_customheight",resolutions[mode-3][1]);
        trap_Cvar_SetValue("cg_aspectMode",mode>=17 ? 1 : 0);
    } else {
        trap_Cvar_SetValue("r_mode",mode);
    }
    return 1;
}

void TCE_UI_SaveVideoMode(void) {
    trap_Cvar_SetValue("r_oldMode",TCE_UI_GetVideoMode());
    trap_Cvar_SetValue("ui_oldCustomWidth",trap_Cvar_VariableValue("r_customwidth"));
    trap_Cvar_SetValue("ui_oldCustomHeight",trap_Cvar_VariableValue("r_customheight"));
    trap_Cvar_SetValue("ui_oldAspectMode",trap_Cvar_VariableValue("cg_aspectMode"));
    trap_Cvar_SetValue("ui_oldCustomAspect",trap_Cvar_VariableValue("r_customaspect"));
}

void TCE_UI_ResetVideoMode(void) {
    char saved[32];int mode;
    trap_Cvar_VariableStringBuffer("r_oldMode",saved,sizeof(saved));
    if(!saved[0])return;
    mode=atoi(saved);
    /* Original vidReset substitutes standard mode 3 for a saved zero. */
    if(mode==0)mode=3;
    if(mode==-1) {
        trap_Cvar_SetValue("r_customwidth",trap_Cvar_VariableValue("ui_oldCustomWidth"));
        trap_Cvar_SetValue("r_customheight",trap_Cvar_VariableValue("ui_oldCustomHeight"));
        trap_Cvar_SetValue("r_customaspect",trap_Cvar_VariableValue("ui_oldCustomAspect"));
    }
    TCE_UI_ApplyVideoMode(mode);
    trap_Cvar_SetValue("cg_aspectMode",trap_Cvar_VariableValue("ui_oldAspectMode"));
    trap_Cvar_Set("r_oldMode","");
}
