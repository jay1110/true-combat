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
        TCE_ModernLimboRect(slots[i],254+i*196,318,182,54);
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

static void TCE_ModernLimboSurfacePaint(const rectDef_t *rect, qboolean selected,
                                      qboolean hover, qboolean enabled, qboolean opaque)
{
    vec4_t fill={ .055f,.085f,.115f,.97f };
    vec4_t line={ .16f,.23f,.29f,1 };
    if(hover && enabled) { fill[0]=.10f;fill[1]=.16f;fill[2]=.20f; }
    if(selected && enabled) { fill[0]=.14f;fill[1]=.16f;fill[2]=.16f; }
    if(opaque)
    {
        vec4_t themed;
        /* An open selector covers other controls: retain the selected palette
         * but keep its text readable even with a transparent global theme. */
        TCE_ModernThemeColor(fill,themed,1);
        themed[3]=.98f;
        CG_FillRect(rect->x,rect->y,rect->w,rect->h,themed);
    }
    else TCE_ModernFill(rect->x,rect->y,rect->w,rect->h,fill);
    TCE_ModernFill(rect->x,rect->y+rect->h-1,rect->w,1,
                   selected && enabled ? tceModernAmber : line);
}

static void TCE_ModernLimboSurface(const rectDef_t *rect, qboolean selected,
                                  qboolean hover, qboolean enabled)
{
    TCE_ModernLimboSurfacePaint(rect,selected,hover,enabled,qfalse);
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

static const tce_weaponModel_t *TCE_ModernLimboWeaponModel(weapon_t weapon)
{
    const tce_weaponInfo_t *media;
    /* These four entries describe loadout bundles/abilities, not a firearm. */
    if(weapon<=0 || (int)weapon>=TCE_MAX_WEAPONS ||
       weapon==36 || weapon==19 || weapon==20 || weapon==21) return NULL;
    media=&tce_cg_weapons[weapon];
    if(media->weaponModel[W_TP_MODEL].model>0) return &media->weaponModel[W_TP_MODEL];
    if(media->weaponModel[W_PU_MODEL].model>0) return &media->weaponModel[W_PU_MODEL];
    return NULL;
}

static qboolean TCE_ModernLimboWeaponPreview(float x,float y,float w,float h,weapon_t weapon)
{
    const tce_weaponModel_t *model=TCE_ModernLimboWeaponModel(weapon);
    refdef_t view;
    refEntity_t entity;
    vec3_t mins,maxs,angles={ 0,90,0 };
    float tangent=.41421356f,aspect,distance,horizontal,vertical,depth;
    int i,team=CG_LimboPanel_GetTeam();
    if(!model || w<=0 || h<=0) return qfalse;
    trap_R_ModelBounds(model->model,mins,maxs);
    for(i=0;i<3;++i)
        if(!(mins[i]>=-65536.f && maxs[i]<=65536.f && maxs[i]>=mins[i])) return qfalse;
    horizontal=maxs[0]-mins[0]; vertical=maxs[2]-mins[2]; depth=maxs[1]-mins[1];
    if(!(horizontal>=0 && vertical>=0 && depth>=0) || horizontal+vertical+depth<=0) return qfalse;
    memset(&view,0,sizeof(view)); memset(&entity,0,sizeof(entity));
    CG_AdjustFrom640(&x,&y,&w,&h);
    if(w<1 || h<1) return qfalse;
    aspect=w/h;
    /* Correct perspective aspect and padding keep long rifles and compact
     * pistols in their own bounds; no ET HUD placeholder icons are used. */
    distance=horizontal*.58f/tangent;
    if(vertical*.58f*aspect/tangent>distance) distance=vertical*.58f*aspect/tangent;
    distance+=depth*.5f+1.f;
    entity.origin[0]=distance+(mins[1]+maxs[1])*.5f;
    entity.origin[1]=-(mins[0]+maxs[0])*.5f;
    entity.origin[2]=-(mins[2]+maxs[2])*.5f;
    AnglesToAxis(angles,entity.axis);
    entity.hModel=model->model;
    entity.customSkin=team>=TEAM_AXIS && team<=TEAM_ALLIES && model->skin[team] ? model->skin[team] : model->skin[0];
    entity.renderfx=RF_NOSHADOW|RF_MINLIGHT|RF_FORCENOLOD;
    entity.shaderRGBA[0]=entity.shaderRGBA[1]=entity.shaderRGBA[2]=entity.shaderRGBA[3]=255;
    view.rdflags=RDF_NOWORLDMODEL;
    AxisClear(view.viewaxis);
    view.fov_x=45; view.fov_y=(float)(atan(tangent/aspect)*(360.0/M_PI));
    view.x=(int)x;view.y=(int)y;view.width=(int)w;view.height=(int)h;view.time=cg.time;
    trap_R_SaveViewParms();
    trap_R_ClearScene();trap_R_AddRefEntityToScene(&entity);trap_R_RenderScene(&view);
    trap_R_RestoreViewParms();
    return qtrue;
}

static void TCE_ModernLimboSlot(panel_button_t *button, const char *label)
{
    qboolean enabled=CG_LimboPanel_GetTeam()!=TEAM_SPECTATOR;
    qboolean selected=enabled && button->data[0]==cgs.ccSelectedWeaponNumber;
    weapon_t weapon=enabled ? CG_LimboPanel_GetSelectedWeaponForSlot(button->data[0]) : WP_NONE;
    const char *name,*description,*category;
    float textX=button->rect.x+12;
    TCE_ModernLimboSurface(&button->rect,selected,BG_CursorInRect(&button->rect),enabled);
    TCE_ModernText(button->rect.x+12,button->rect.y+15,.16f,158,
                   selected?tceModernAmber:tceModernMuted,label,1);
    TCE_ModernLimboWeaponInfo(weapon,&name,&description,&category);
    if(enabled && TCE_ModernLimboWeaponPreview(button->rect.x+10,button->rect.y+22,54,26,weapon))
        textX=button->rect.x+74;
    TCE_ModernTextFit(textX,button->rect.y+40,.21f,button->rect.x+button->rect.w-12-textX,
                      enabled?tceModernWhite:tceModernMuted,name && *name ? name : "--",1);
}

static void TCE_ModernLimboWeaponRow(const rectDef_t *rect, weapon_t weapon,
                                     int number,int count,qboolean selected,qboolean expanded)
{
    const char *name,*description,*category;
    qboolean disabled=CG_LimboPanel_RealWeaponIsDisabled(weapon);
    qboolean hover=BG_CursorInRect(rect);
    float scale=rect->h<20 ? .15f : .22f;
    float baseline=rect->y+rect->h*.68f;
    float textX=rect->x+12;
    char countText[48];
    TCE_ModernLimboWeaponInfo(weapon,&name,&description,&category);
    TCE_ModernLimboSurfacePaint(rect,selected,hover,!disabled,expanded);
    if(TCE_ModernLimboWeaponModel(weapon))
    {
        float iconHeight=rect->h-4;
        if(iconHeight>24) iconHeight=24;
        if(TCE_ModernLimboWeaponPreview(rect->x+10,rect->y+(rect->h-iconHeight)*.5f,58,iconHeight,weapon))
            textX=rect->x+80;
    }
    TCE_ModernTextFit(textX,baseline,scale,rect->x+rect->w-134-textX,
                      disabled?tceModernMuted:hover?tceModernAmber:tceModernWhite,
                      description && *description ? description : name,1);
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
    TCE_ModernTextFit(button->rect.x+2,379,.17f,402,tceModernMuted,description,0);
    TCE_ModernTextFit(button->rect.x+416,379,.15f,156,tceModernAmber,category,0);
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
    TCE_ModernText(254,309,.17f,574,tceModernMuted,"03 / GEAR",1);
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
