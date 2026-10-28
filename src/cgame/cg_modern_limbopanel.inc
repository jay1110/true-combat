/* Optional deployment presentation. Native button objects, input callbacks,
 * team/class IDs and loadout validation remain in cg_limbopanel.c. */
int CG_LimboPanel_RenderCounter_ValueForButton(panel_button_t *button);
static fontInfo_t modernLimboNameFont;
static panel_button_text_t modernLimboNameText;

static void TCE_ModernLimboRect(panel_button_t *button, float x, float y, float w, float h)
{
    button->rect.x=x; button->rect.y=y; button->rect.w=w; button->rect.h=h;
}

static void TCE_ModernLimboLayout(void)
{
    panel_button_t *teams[]={ &teamButton0,&teamButton1,&teamButton2 };
    panel_button_t *classes[]={ &classButton0,&classButton1,&classButton2,
                                &classButton3,&classButton4,&classButton5 };
    panel_button_t *slots[]={ &weaponLight1,&weaponLight2,&weaponLight3 };
    int i;
    memset(&modernLimboNameFont,0,sizeof(modernLimboNameFont));
    trap_R_RegisterFont("tce2_ui/Barlow-Regular",24,&modernLimboNameFont);
    modernLimboNameText=nameEditFont;
    if(modernLimboNameFont.glyphScale>0) modernLimboNameText.font=&modernLimboNameFont;
    nameEdit.font=&modernLimboNameText;
    /* Re-use the original controls, rather than adding a second input layer. */
    TCE_ModernLimboRect(&playerLimboHead,42,112,64,86);
    TCE_ModernLimboRect(&nameEdit,42,227,176,20);
    for(i=0;i<3;++i)
    {
        TCE_ModernLimboRect(teams[i],254+i*196,114,182,76);
        TCE_ModernLimboRect(classes[i*2],254+i*196,246,86,38);
        TCE_ModernLimboRect(classes[i*2+1],350+i*196,246,86,38);
        TCE_ModernLimboRect(slots[i],254+i*196,328,182,28);
    }
    TCE_ModernLimboRect(&weaponPanel,254,382,574,30);
    TCE_ModernLimboRect(&cancelButton,506,430,140,32);
    TCE_ModernLimboRect(&okButton,658,430,170,32);
}

static void TCE_ModernLimboGeometry(void)
{
    int count=CG_LimboPanel_GetTeam()==TEAM_SPECTATOR ? 1 : CG_LimboPanel_WeaponCount();
    /* Native KeyUp walks upward using this same rect.h. Keep all alternatives
     * on screen even for a class table containing MAX_WEAPS_PER_CLASS entries. */
    weaponPanel.rect.h=count>10 ? 284.f/(count-1) : 30.f;
}

static void TCE_ModernLimboSurface(const rectDef_t *rect, qboolean selected,
                                   qboolean hover, qboolean enabled)
{
    vec4_t fill={ .055f,.085f,.115f,.97f };
    vec4_t line={ .16f,.23f,.29f,1 };
    if(hover && enabled) { fill[0]=.10f;fill[1]=.16f;fill[2]=.20f; }
    if(selected && enabled) { fill[0]=.14f;fill[1]=.16f;fill[2]=.16f; }
    TCE_ModernFill(rect->x,rect->y,rect->w,rect->h,fill);
    TCE_ModernFill(rect->x,rect->y+rect->h-1,rect->w,1,
                   selected && enabled ? tceModernAmber : line);
}

static void TCE_ModernLimboTeam(panel_button_t *button, panel_button_t *counter)
{
    static const char *names[]={ "SPECOPS","TERRORISTS","SPECTATOR" };
    qhandle_t icons[]={ cgs.media.limboTeamButtonAllies,cgs.media.limboTeamButtonAxis,
                        cgs.media.limboTeamButtonSpec };
    int number=button->data[0];
    qboolean selected=CG_LimboPanel_GetTeam()==teamOrder[number];
    TCE_ModernLimboSurface(&button->rect,selected,BG_CursorInRect(&button->rect),qtrue);
    trap_R_SetColor(NULL);
    CG_DrawPic(button->rect.x+14,button->rect.y+10,51,33,icons[number]);
    TCE_ModernText(button->rect.x+76,button->rect.y+31,.30f,92,
                   selected?tceModernAmber:tceModernWhite,
                   va("%02i",CG_LimboPanel_RenderCounter_ValueForButton(counter)),1);
    TCE_ModernText(button->rect.x+14,button->rect.y+62,.23f,154,
                   selected?tceModernWhite:tceModernMuted,names[number],1);
}

static void TCE_ModernLimboClass(panel_button_t *button)
{
    qboolean enabled=CG_LimboPanel_GetTeam()!=TEAM_SPECTATOR;
    qboolean selected=enabled && button->data[1]==CG_LimboPanel_GetClass();
    TCE_ModernLimboSurface(&button->rect,selected,BG_CursorInRect(&button->rect),enabled);
    TCE_ModernText(button->rect.x+12,button->rect.y+24,.24f,48,
                   selected?tceModernAmber:tceModernMuted,
                   button->data[1]%2 ? "02" : "01",1);
    if(selected) TCE_ModernImage("tce2_ui/icons/check",button->rect.x+58,button->rect.y+12,14,14);
}

static void TCE_ModernLimboSlot(panel_button_t *button, const char *label)
{
    qboolean enabled=CG_LimboPanel_GetTeam()!=TEAM_SPECTATOR;
    qboolean selected=enabled && button->data[0]==cgs.ccSelectedWeaponNumber;
    TCE_ModernLimboSurface(&button->rect,selected,BG_CursorInRect(&button->rect),enabled);
    TCE_ModernText(button->rect.x+12,button->rect.y+19,.22f,158,
                   selected?tceModernAmber:tceModernMuted,label,1);
}

static void TCE_ModernLimboWeaponInfo(weapon_t weapon, const char **name,
                                      const char **description,const char **category)
{
    weaponType_t *type;
    *name=*description=*category="";
    if(weapon==36 || weapon==19 || weapon==20 || weapon==21)
    {
        type=WM_FindWeaponTypeForWeapon(weapon);
        if(type) { *name=type->desc;*description=type->description;*category=type->category; }
    }
    else if((int)weapon>=0 && (int)weapon<(int)TCE_MAX_WEAPONS)
    {
        *name=tce_cg_weapons[weapon].deployMenuShortName;
        *description=tce_cg_weapons[weapon].deployMenuDescription;
        *category=tce_cg_weapons[weapon].deployMenuType;
    }
}

static void TCE_ModernLimboWeaponRow(const rectDef_t *rect, weapon_t weapon,
                                     int number,int count,qboolean selected,qboolean expanded)
{
    const char *name,*description,*category;
    qboolean disabled=CG_LimboPanel_RealWeaponIsDisabled(weapon);
    qboolean hover=BG_CursorInRect(rect);
    float scale=rect->h<20 ? .15f : .22f;
    float baseline=rect->y+rect->h*.68f;
    char countText[48];
    TCE_ModernLimboWeaponInfo(weapon,&name,&description,&category);
    TCE_ModernLimboSurface(rect,selected,hover,!disabled);
    TCE_ModernText(rect->x+12,baseline,scale,rect->w-146,
                   disabled?tceModernMuted:hover?tceModernAmber:tceModernWhite,name,1);
    Com_sprintf(countText,sizeof(countText),disabled?"%d / %d  LOCKED":"%d / %d",number,count);
    TCE_ModernText(rect->x+rect->w-118,baseline,scale*.85f,96,tceModernMuted,countText,0);
    if(!expanded && count>1)
        TCE_ModernImage("tce2_ui/icons/chevron_up",rect->x+rect->w-22,rect->y+(rect->h-12)*.5f,12,12);
}

static void TCE_ModernLimboWeaponPanel(panel_button_t *button,weapon_t weapon,int count)
{
    const char *name,*description,*category;
    int i,position=0,selected=CG_LimboPanel_GetSelectedWeaponNum();
    qboolean expanded=BG_PanelButtons_GetFocusButton()==button && count>1;
    rectDef_t rect=button->rect;
    if(CG_LimboPanel_GetTeam()==TEAM_SPECTATOR)
    {
        TCE_ModernLimboSurface(&rect,qfalse,qfalse,qfalse);
        TCE_ModernText(rect.x+12,rect.y+21,.22f,rect.w-24,tceModernMuted,"SPECTATOR",1);
        return;
    }
    TCE_ModernLimboWeaponInfo(weapon,&name,&description,&category);
    TCE_ModernText(button->rect.x+2,374,.17f,402,tceModernMuted,description,0);
    TCE_ModernText(button->rect.x+416,374,.15f,156,tceModernAmber,category,0);
    TCE_ModernLimboWeaponRow(&rect,weapon,TCE_AddInt32(selected,1),count,qtrue,expanded);
    if(!expanded) return;
    if(BG_CursorInRect(&rect) && button->data[7]!=0)
    { SOUND_FOCUS;button->data[7]=0; }
    rect.y-=rect.h;
    for(i=0;i<count;++i)
    {
        weapon_t alternative=CG_LimboPanel_GetWeaponForNumber(i,cgs.ccSelectedWeaponNumber,qtrue);
        if(alternative==weapon) continue;
        ++position;
        TCE_ModernLimboWeaponRow(&rect,alternative,i+1,count,qfalse,qtrue);
        if(BG_CursorInRect(&rect) && button->data[7]!=position)
        { SOUND_FOCUS;button->data[7]=position; }
        rect.y-=rect.h;
    }
}

static void TCE_ModernLimboRender(void)
{
    panel_button_t *classes[]={ &classButton0,&classButton1,&classButton2,
                                &classButton3,&classButton4,&classButton5 };
    clientInfo_t *client=&cgs.clientinfo[cg.clientNum];
    const char *team=CG_LimboPanel_GetTeam()==TEAM_ALLIES ? "SPECOPS" :
                     CG_LimboPanel_GetTeam()==TEAM_AXIS ? "TERRORISTS" : "SPECTATOR";
    const char *roles[]={ "Assault","Recon","Sniper" };
    vec4_t shade={ .018f,.028f,.04f,.86f };
    qboolean spectator=CG_LimboPanel_GetTeam()==TEAM_SPECTATOR;
    int role,i;
    TCE_ModernFill(0,0,852,480,shade);
    TCE_ModernText(24,37,.17f,800,tceModernAmber,TCE_PROJECT_TITLE,1);
    TCE_ModernText(24,74,.56f,600,tceModernWhite,"DEPLOYMENT MENU",1);
    TCE_ModernText(644,62,.18f,184,tceModernMuted,cgs.rawmapname,0);
    TCE_ModernPanel(24,94,212,318,.95f);
    CG_LimboPanel_RenderHead(&playerLimboHead);
    TCE_ModernText(122,129,.18f,100,tceModernMuted,"IDENTITY",1);
    TCE_ModernText(122,153,.21f,100,tceModernWhite,team,1);
    if(!spectator)
        TCE_ModernText(122,177,.20f,100,tceModernAmber,roles[BG_WolfClassToTCE(CG_LimboPanel_GetClass())],0);
    TCE_ModernText(42,222,.17f,176,tceModernMuted,"NAME",1);
    TCE_ModernPanel(36,228,188,28,1);
    TCE_ModernThemeColor(nameEditFont.colour,modernLimboNameText.colour,0);
    BG_PanelButton_RenderEdit(&nameEdit);
    TCE_ModernFill(36,256,188,1,BG_PanelButtons_GetFocusButton()==&nameEdit?tceModernAmber:tceModernMuted);
    TCE_ModernText(42,287,.17f,176,tceModernMuted,"ARMAMENT AVAILABILITY",1);
    for(role=0;role<3;++role)
    {
        int rating=role==BG_WolfClassToTCE(client->tceScorePlayerClass) ?
                    client->tceClassRating : client->tcePreviousClassRating;
        qboolean selected=!spectator && role==BG_WolfClassToTCE(CG_LimboPanel_GetClass());
        TCE_ModernText(42,315+role*30,.20f,132,selected?tceModernWhite:tceModernMuted,roles[role],0);
        TCE_ModernText(184,315+role*30,.23f,34,selected?tceModernAmber:tceModernMuted,
                       spectator?"--":va("%i",TCE_AddInt32(rating,1)),1);
    }
    TCE_ModernText(254,103,.17f,574,tceModernMuted,"01 / TEAM",1);
    TCE_ModernLimboTeam(&teamButton0,&teamCounter0);
    TCE_ModernLimboTeam(&teamButton1,&teamCounter1);
    TCE_ModernLimboTeam(&teamButton2,&teamCounter2);
    TCE_ModernText(254,214,.17f,574,tceModernMuted,"02 / IDENTITY",1);
    for(i=0;i<3;++i)
    {
        TCE_ModernText(254+i*196,237,.22f,182,tceModernWhite,roles[i],1);
        TCE_ModernLimboClass(classes[i*2]);TCE_ModernLimboClass(classes[i*2+1]);
    }
    TCE_ModernText(254,316,.17f,574,tceModernMuted,"03 / GEAR",1);
    TCE_ModernLimboSlot(&weaponLight1,"Primary");
    TCE_ModernLimboSlot(&weaponLight2,"Sidearm");
    TCE_ModernLimboSlot(&weaponLight3,"Special");
    TCE_ModernLimboSurface(&cancelButton.rect,qfalse,BG_CursorInRect(&cancelButton.rect),qtrue);
    TCE_ModernText(cancelButton.rect.x+18,cancelButton.rect.y+22,.23f,104,tceModernWhite,"CANCEL",1);
    TCE_ModernLimboSurface(&okButton.rect,qtrue,BG_CursorInRect(&okButton.rect),qtrue);
    TCE_ModernText(okButton.rect.x+18,okButton.rect.y+22,.23f,134,tceModernAmber,"OK",1);
    /* Draw the native loadout selector last: its upward alternatives must cover
     * the team/identity cards, not be painted underneath their labels. */
    CG_LimboPanel_WeaponPanel(&weaponPanel);
}
