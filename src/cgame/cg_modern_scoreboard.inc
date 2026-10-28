/* Optional native theme. Existing score wire/privacy/state helpers stay authoritative. */
static float tceModernScoreFade;

static int TCE_ModernScoreCount(int team)
{
    int i,n=0;
    for (i=0;i<cg.numScores && i<MAX_CLIENTS;++i)
        if (cg.scores[i].client>=0 && cg.scores[i].client<MAX_CLIENTS &&
            cgs.clientinfo[cg.scores[i].client].team==team) ++n;
    return n;
}

static void TCE_ModernScoreText(float x,float y,float scale,float width,const float *color,const char *text,int strong)
{
    vec4_t c; memcpy(c,color,sizeof(c)); c[3]*=tceModernScoreFade;
    TCE_ModernText(x,y,scale,width,c,text,strong);
}

static int TCE_ModernScoreObjectives(int x,int y,int width,float fade)
{
    int count=TCE_ModernScoreCount(TEAM_SPECTATOR),pages=TCE_ModernRosterPages(count,24);
    int page=((unsigned)cg.time/8000u)%pages,i,n=0,shown=0,seconds;
    const char *map=CG_ConfigString(CS_MESSAGE);
    vec4_t dim={.015f,.025f,.04f,.92f};
    char text[256];
    (void)x;(void)y;(void)width;
    tceModernScoreFade=fade;dim[3]*=fade;
    TCE_ModernFill(8,8,836,464,dim);
    TCE_ModernScoreText(26,33,.31f,500,tceModernWhite,"MATCH STANDINGS",1);
    TCE_ModernScoreText(26,53,.18f,500,tceModernMuted,*map?map:cgs.rawmapname,0);
    Com_sprintf(text,sizeof(text),"%s %d   |   %s [%d] : [%d]",CG_TranslateString("ROUND"),cgs.currentRound+1,
                CG_TranslateString("SCORE"),cg.teamWonRounds[1],cg.teamWonRounds[0]);
    TCE_ModernScoreText(26,76,.20f,520,tceModernAmber,text,1);
    if (cg.snap->ps.pm_type==PM_INTERMISSION)
    {
        int winner=atoi(Info_ValueForKey(CG_ConfigString(CS_MULTI_MAPWINNER),"winner"));
        seconds=5-(cg.time-cgs.intermissionStartTime)/1000;if(seconds<0)seconds=0;
        Com_sprintf(text,sizeof(text),"%s  |  %d %s",winner<0?"INTERMISSION":winner?"SPECOPS WIN":"TERRORISTS WIN",
                    seconds,CG_TranslateString("SECS TO NEXT DEPLOYMENT"));
    }
    else if (cgs.gamestate!=GS_PLAYING) Q_strncpyz(text,CG_TranslateString("WARMUP"),sizeof(text));
    else
    {
        double remaining=(double)cgs.timelimit*60.0-(double)(cg.time-cgs.levelStartTime)/1000.0;
        if(cgs.timelimit<=0)Q_strncpyz(text,"MISSION TIME  --:--",sizeof(text));
        else if(remaining<0)Q_strncpyz(text,CG_TranslateString("SUDDEN DEATH"),sizeof(text));
        else { if(remaining>2147483000.0)remaining=2147483000.0;seconds=(int)remaining;
            Com_sprintf(text,sizeof(text),"MISSION TIME  %02d:%02d",seconds/60,seconds%60); }
    }
    TCE_ModernScoreText(540,32,.20f,282,tceModernWhite,text,1);
    if(cgs.gametype!=5 && (cg.snap->ps.persistant[PERS_TEAM]==TEAM_AXIS || cg.snap->ps.persistant[PERS_TEAM]==TEAM_ALLIES))
    {
        seconds=CG_CalculateReinfTime(qfalse);
        if(seconds>0){Com_sprintf(text,sizeof(text),"REINFORCE  %d:%02d",seconds/60,seconds%60);
            TCE_ModernScoreText(540,53,.17f,280,tceModernMuted,text,0);}
    }
    if(cgs.gametype==GT_WOLF_CAMPAIGN){Com_sprintf(text,sizeof(text),"MAP %d / %d",cgs.currentCampaignMap+1,cgs.campaignData.mapCount);
        TCE_ModernScoreText(540,76,.17f,280,tceModernMuted,text,0);}
    TCE_ModernPanel(24,405,804,61,fade*.86f);
    Com_sprintf(text,sizeof(text),"SPECTATORS  %d%s",count,pages>1?va("   |   PAGE %d/%d / 8s",page+1,pages):"");
    TCE_ModernScoreText(34,419,.19f,780,tceModernMuted,text,1);
    for(i=0;i<cg.numScores && i<MAX_CLIENTS;++i)
    {
        int client=cg.scores[i].client;
        if(client<0 || client>=MAX_CLIENTS || cgs.clientinfo[client].team!=TEAM_SPECTATOR)continue;
        if(n++<page*24 || shown>=24)continue;
        TCE_ModernScoreText(34+(shown%8)*98,433+(shown/8)*13,.20f,94,tceModernWhite,cgs.clientinfo[client].name,0);++shown;
    }
    if(!count)TCE_ModernScoreText(34,441,.21f,760,tceModernMuted,"No spectators",0);
    return 96;
}

static int TCE_ModernScoreInfo(int x,int y,float fade)
{
    (void)x;(void)y;
    WM_DrawInfoLine(26,69,fade);
    return 96;
}

static int TCE_ModernScoreTeam(int legacyX,int y,int team,float fade,int ignoredRows)
{
    static const float columns[]={145,172,199,226,258,300,339,369};
    static const char *labels[]={"KILL","DTH","SUI","TK","SCORE","DR","AA","PING"};
    float x=team==TEAM_AXIS?24.f:438.f,rowHeight,scale;
    vec4_t accent={.85f,.26f,.23f,1},rowColor={.10f,.15f,.19f,.55f};
    int count=TCE_ModernScoreCount(team),pages=TCE_ModernRosterPages(count,16);
    int page=((unsigned)cg.time/8000u)%pages,rows=count-page*16,i,j,n=0,shown=0;
    int localTeam=cgs.clientinfo[cg.clientNum].team;
    char text[128];
    (void)legacyX;(void)ignoredRows;(void)fade;
    if(rows>16)rows=16;if(rows<1)rows=1;
    rowHeight=rows>11?246.f/rows:22.f;scale=rows>11?.22f:.24f;
    if(team==TEAM_ALLIES){accent[0]=.21f;accent[1]=.60f;accent[2]=.91f;}
    accent[3]=tceModernScoreFade;cg.teamPlayers[team]=count;
    TCE_ModernPanel(x,y,390,300,tceModernScoreFade*.94f);
    TCE_ModernFill(x,y,390,3,accent);
    Com_sprintf(text,sizeof(text),"%s  %d",CG_TranslateString(team==TEAM_AXIS?"TERRORISTS":"SPECOPS"),cg.teamScores[team-1]);
    TCE_ModernScoreText(x+12,y+26,.30f,270,tceModernWhite,text,1);
    Com_sprintf(text,sizeof(text),"%d PLAYERS%s",count,pages>1?va(" | %d/%d",page+1,pages):"");
    TCE_ModernScoreText(x+260,y+26,.19f,120,tceModernMuted,text,0);
    if(cg_gameType.integer==7 && cg.teamFirstBlood==team)
        TCE_ModernScoreText(x+12,y+39,.17f,150,tceModernAmber,"FIRST BLOOD",1);
    TCE_ModernScoreText(x+12,y+51,.18f,125,tceModernMuted,"PLAYER / STATUS",1);
    for(j=0;j<8;++j)TCE_ModernScoreText(x+columns[j],y+51,.18f,j==4?40:30,tceModernMuted,CG_TranslateString(labels[j]),1);
    for(i=0;i<cg.numScores && i<MAX_CLIENTS;++i)
    {
        score_t *score=&cg.scores[i];const clientInfo_t *ci;float top,base,nameX;
        int state,health,respawns,teammate,showStatus,hidden;
        if(score->client<0 || score->client>=MAX_CLIENTS)continue;
        ci=&cgs.clientinfo[score->client];if(ci->team!=team)continue;
        if(n++<page*16 || shown>=16)continue;
        top=y+57+shown*rowHeight;base=top+rowHeight*.76f;nameX=x+12;
        rowColor[3]=(shown&1)?.35f:.16f;rowColor[3]*=tceModernScoreFade;
        if(score->client==cg.snap->ps.clientNum){rowColor[0]=.31f;rowColor[1]=.28f;rowColor[2]=.16f;rowColor[3]=.68f*tceModernScoreFade;}
        else{rowColor[0]=.10f;rowColor[1]=.15f;rowColor[2]=.19f;}
        TCE_ModernFill(x+6,top,378,rowHeight-1,rowColor);
        state=TCE_ScoreSnapshotState(score->client);health=ci->health;respawns=score->respawnsLeft;
        if(state>=0){health=state==2?-1:state;if(state==1 && respawns==-2)respawns=-1;}
        teammate=localTeam!=TEAM_SPECTATOR && ci->team==localTeam;
        showStatus=cg.snap->ps.pm_type==PM_INTERMISSION || teammate;
        if(showStatus)
        {
            const char *status=(respawns==-2 || (teammate && health==-1))?"OUT":(teammate && health==0)?"DOWN":"";
            if(ci->powerups&0xc0){TCE_ModernScoreText(nameX,base,scale*.72f,18,tceModernAmber,"OBJ",1);nameX+=19;}
            if(*status){TCE_ModernScoreText(nameX,base,scale*.72f,24,tceModernAmber,status,1);nameX+=26;}
        }
        if(TCE_ScoreCountry(score->client)>=0 && score->ping!=-1 && score->ping!=999)
        {
            int country=TCE_ScoreCountry(score->client);float s=(country&15)/16.f,t=(country>>4)/16.f;
            CG_DrawPicST(nameX,top+(rowHeight-10)*.5f,13,10,s,t,s+1.f/16,t+1.f/16,tce_countryFlagsShader);nameX+=15;
        }
        Com_sprintf(text,sizeof(text),"%s%s",ci->botSkill>0?"^8[B]^7 ":"",ci->name);
        TCE_ModernScoreText(nameX,base,scale,x+139-nameX,tceModernWhite,text,0);
        hidden=tce_scoreExtended[i] && TCE_ModernHiddenStats(cg_gameType.integer,cgs.tceKillMessage,cg.snap->ps.stats[STAT_HEALTH],cg.snap->ps.pm_type);
        for(j=0;j<8;++j)
        {
            if(j==7)Com_sprintf(text,sizeof(text),score->ping==-1?"CONNECT":"%d",score->ping);
            else if(!tce_scoreExtended[i] && j!=4)Q_strncpyz(text,"-",sizeof(text));
            else if(hidden && (j==1 || j==2 || j==4 || j==5))Q_strncpyz(text,"n/a",sizeof(text));
            else if(j==5)Com_sprintf(text,sizeof(text),"%.2f",(double)score->damageRating*(double).01f);
            else {int values[]={score->kills,score->deaths,score->suicides,score->teamKills,score->score,0,TCE_AddInt32(score->classRating,1)};
                Com_sprintf(text,sizeof(text),"%d",values[j]);}
            TCE_ModernScoreText(x+columns[j],base,scale,j==4?40:30,tceModernWhite,text,0);
        }
        ++shown;
    }
    if(!count)TCE_ModernScoreText(x+12,y+87,.24f,365,tceModernMuted,"Waiting for players",0);
    return y+300;
}
