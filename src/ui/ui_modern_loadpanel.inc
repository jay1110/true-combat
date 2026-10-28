static int UI_ModernCounter(const char *name)
{
    float value=trap_Cvar_VariableValue(name);
    if(!(value>0))return 0;
    if(value>=2147483000.f)return 2147483000;
    return (int)value;
}

static void UI_DrawModernLoadPanel(qboolean ownerdraw)
{
    uiClientState_t state;
    char serverinfo[MAX_INFO_STRING],download[MAX_INFO_VALUE],image[MAX_QPATH],map[MAX_QPATH];
    char summary[512],size[64],total[64],rateText[64],eta[64];
    const char *status="Preparing game",*mapTitle="CONNECTING";
    int used,expected,start,elapsed,rate=0;float progress=-1;
    trap_GetClientState(&state);
    trap_GetConfigString(CS_SERVERINFO,serverinfo,sizeof(serverinfo));
    trap_Cvar_VariableStringBuffer("cl_downloadName",download,sizeof(download));
    map[0]=0;
    /* Old gamestate configstrings can belong to the previous server while a
     * challenge/download is in progress. Never label that as the new map. */
    if(state.connState>=CA_LOADING)
    {
        Q_strncpyz(map,Info_ValueForKey(serverinfo,"mapname"),sizeof(map));
        if(*map)mapTitle=map;
    }
    TCE_ModernImage("tce2_ui/background",0,0,852,480);
    TCE_ModernPanel(0,0,852,480,.62f);
    TCE_ModernText(26,42,.23f,800,tceModernAmber,TCE_PROJECT_TITLE,1);
    TCE_ModernText(26,76,.42f,800,tceModernWhite,mapTitle,1);
    TCE_ModernPanel(24,96,516,284,.96f);
    Com_sprintf(image,sizeof(image),"levelshots/%s",*map?map:"unknownmap");
    if(!trap_R_RegisterShaderNoMip(image))Q_strncpyz(image,"levelshots/unknownmap",sizeof(image));
    TCE_ModernImage(image,30,102,504,252);
    TCE_ModernText(38,371,.17f,480,tceModernMuted,*map?map:"Map preview available after server gamestate",0);
    TCE_ModernPanel(556,96,272,284,.96f);
    TCE_ModernText(572,121,.16f,240,tceModernAmber,"SERVER CONNECTION",1);
    TCE_ModernParagraph(572,145,.22f,240,16,3,tceModernWhite,state.servername);
    TCE_ModernParagraph(572,199,.17f,240,13,7,tceModernMuted,Info_ValueForKey(state.updateInfoString,"motd"));
    if(state.connState<CA_CONNECTED && *state.messageString)
        TCE_ModernParagraph(572,300,.17f,240,12,6,tceModernAmber,state.messageString);
    switch(state.connState)
    {
    case CA_CONNECTING:status=va(trap_TranslateString("Awaiting connection...%i"),state.connectPacketCount);break;
    case CA_CHALLENGING:status=va(trap_TranslateString("Awaiting challenge...%i"),state.connectPacketCount);break;
    case CA_CONNECTED:status=trap_TranslateString("Awaiting gamestate...");break;
    case CA_LOADING:status="Loading map and game assets";break;
    case CA_PRIMED:status="Waiting for first snapshot";break;
    case CA_ACTIVE:status="Connected";break;
    default:status="Preparing connection";break;
    }
    summary[0]=0;
    if(!ownerdraw && *download)
    {
        used=UI_ModernCounter("cl_downloadCount");expected=UI_ModernCounter("cl_downloadSize");
        start=UI_ModernCounter("cl_downloadTime");elapsed=trap_Milliseconds()-start;
        progress=TCE_ModernProgress(used,expected);
        if(start>0 && elapsed>=1000)rate=(int)((double)used*1000.0/(double)elapsed);
        UI_ReadableSize(size,sizeof(size),used);UI_ReadableSize(total,sizeof(total),expected);
        UI_ReadableSize(rateText,sizeof(rateText),rate);
        if(rate>0 && expected>used)UI_PrintTime(eta,sizeof(eta),(expected-used)/rate);
        else Q_strncpyz(eta,"estimating",sizeof(eta));
        Com_sprintf(summary,sizeof(summary),"%s / %s  |  %s/s  |  %s %s",size,
                    expected>0?total:"unknown size",rate>0?rateText:"--",trap_TranslateString("Estimated time left:"),eta);
        status=download;
        TCE_ModernText(572,288,.17f,240,tceModernAmber,"DOWNLOADING CONTENT",1);
    }
    TCE_ModernPanel(24,394,804,70,.96f);
    TCE_ModernText(38,415,.19f,770,tceModernWhite,status,1);
    TCE_ModernBar(38,429,776,5,progress,trap_Milliseconds());
    TCE_ModernText(38,452,.15f,770,tceModernMuted,*summary?summary:"ESC / cancel connection",0);
}
