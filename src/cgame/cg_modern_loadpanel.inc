static void CG_DrawModernLoadPanel(qboolean interactive)
{
    char image[MAX_QPATH],info[MAX_INFO_STRING],detail[256],campaign[192];
    const char *description="",*title=cgs.rawmapname;
    int used=0,expected=0;float progress;
    const char *campaignName=NULL;
    if(cgs.arenaInfoLoaded && *cgs.arenaData.longname)title=cgs.arenaData.longname;
    campaign[0]=0;
    if(cgs.gametype==GT_WOLF_CAMPAIGN && DC->nameForCampaign)
        campaignName=DC->nameForCampaign();
    if(campaignName && *campaignName)
    {
        if(cgs.campaignData.mapCount>0 && cgs.currentCampaignMap>=0 &&
           cgs.currentCampaignMap<cgs.campaignData.mapCount)
            Com_sprintf(campaign,sizeof(campaign),"%s / MISSION %d OF %d",campaignName,
                        cgs.currentCampaignMap+1,cgs.campaignData.mapCount);
        else Q_strncpyz(campaign,campaignName,sizeof(campaign));
    }
    if(cgs.gametype==GT_WOLF_CAMPAIGN && DC->descriptionForCampaign)
        description=DC->descriptionForCampaign();
    else if(cgs.arenaInfoLoaded)
        description=cgs.gametype==GT_WOLF_LMS?cgs.arenaData.lmsdescription:cgs.arenaData.description;
    TCE_ModernImage("tce2_ui/background",0,0,852,480);
    TCE_ModernPanel(0,0,852,480,.58f);
    TCE_ModernText(26,42,.23f,800,tceModernAmber,TCE_PROJECT_TITLE,1);
    TCE_ModernText(26,76,.42f,800,tceModernWhite,*title?title:"PREPARING MISSION",1);
    TCE_ModernPanel(24,96,516,284,.96f);
    Com_sprintf(image,sizeof(image),"levelshots/%s",*cgs.rawmapname?cgs.rawmapname:"unknownmap");
    if(!trap_R_RegisterShaderNoMip(image))Q_strncpyz(image,"levelshots/unknownmap",sizeof(image));
    TCE_ModernImage(image,30,102,504,252);
    TCE_ModernText(38,371,.18f,480,tceModernMuted,
                  cgs.arenaInfoLoaded?cgs.arenaData.authors:"",0);
    TCE_ModernPanel(556,96,272,284,.96f);
    TCE_ModernText(572,121,.16f,240,tceModernAmber,"MISSION / SERVER",1);
    TCE_ModernText(572,145,.22f,240,tceModernWhite,CG_LoadPanel_GameTypeName(cgs.gametype),1);
    Q_strncpyz(info,CG_ConfigString(CS_SERVERINFO),sizeof(info));
    TCE_ModernText(572,168,.17f,240,tceModernMuted,Info_ValueForKey(info,"sv_hostname"),0);
    if(*campaign)
        TCE_ModernParagraph(572,190,.18f,240,12,2,tceModernAmber,campaign);
    TCE_ModernParagraph(572,*campaign?218:194,.18f,240,13,*campaign?9:10,tceModernWhite,description);
    TCE_ModernParagraph(572,342,.15f,240,12,2,tceModernMuted,CG_ConfigString(CS_MOTD));
    TCE_ModernPanel(24,394,804,70,.96f);
    TCE_ModernText(38,415,.18f,770,tceModernWhite,
                  *cg.infoScreenText?cg.infoScreenText:"Loading game assets",0);
    trap_GetHunkData(&used,&expected);progress=TCE_ModernProgress(used,expected);
    TCE_ModernBar(38,429,776,5,progress,trap_Milliseconds());
    if(progress>=0)Com_sprintf(detail,sizeof(detail),"%d%%   /   ASSET MEMORY",(int)(progress*100));
    else Q_strncpyz(detail,"PREPARING ASSETS / SIZE NOT YET KNOWN",sizeof(detail));
    TCE_ModernText(38,452,.14f,770,tceModernMuted,detail,0);
    if(interactive)DC->drawHandlePic(DC->cursorx,DC->cursory,24,24,DC->Assets.cursor);
}
