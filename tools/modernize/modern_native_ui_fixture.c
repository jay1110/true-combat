/* Actual optional native scoreboard/model; renderer and engine services controlled. */
#include "src/cgame/cg_local.h"
#include "src/game/tce_numeric.h"
#include "src/ui/tce_modern_ui.h"
#include "src/ui/ui_public.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
vmCvar_t cg_gameType;
qboolean tce_scoreExtended[MAX_CLIENTS];
qhandle_t tce_countryFlagsShader;
vec4_t tceModernWhite={1,1,1,1},tceModernMuted={.5f,.5f,.5f,1},tceModernAmber={1,.7f,0,1};
static snapshot_t snap;
static int checks,failures,textCount,modernChecked,modernEnabled;
static int mockMarkerLength=-1,mockReads,mockCloses,mockTime=2000,mockHunkUsed,mockHunkExpected;
static char mockMarker[64],mockDownload[128];
static float mockUsed,mockExpected,mockStart;
static float lastProgress;
static uiClientState_t mockState;
static displayContextDef_t mockDC;
displayContextDef_t *DC=&mockDC;
static char texts[2048][256];
static float textY[2048];
static float textX[2048],textScale[2048],textWidth[2048];
static const char *caseName;
static const char *FixtureCampaignName(void){return "Northern Operations";}
static const char *FixtureCampaignDescription(void){return "Campaign mission briefing";}
static void expect(int condition,const char *name)
{++checks;if(!condition){++failures;fprintf(stderr,"%s: %s\n",caseName,name);}}
void QDECL Com_sprintf(char *dest,int size,const char *format,...)
{va_list args;va_start(args,format);vsnprintf(dest,(size_t)size,format,args);va_end(args);if(size>0)dest[size-1]=0;}
void Q_strncpyz(char *dest,const char *src,int size)
{if(size>0){size_t n=strlen(src);if(n>=(size_t)size)n=(size_t)size-1;memcpy(dest,src,n);dest[n]=0;}}
char *QDECL va(const char *format,...)
{static char values[16][2048];static unsigned index;char *p=values[index++&15];va_list args;va_start(args,format);vsnprintf(p,2048,format,args);va_end(args);return p;}
char *CG_TranslateString(const char *text){return(char*)text;}
static const char *FixtureUITranslateString(const char *text){return text;}
const char *CG_ConfigString(int index){return index==CS_MULTI_MAPWINNER?"winner":index==CS_MESSAGE?"Test Map":"";}
char *Info_ValueForKey(const char *text,const char *key)
{(void)text;return !strcmp(key,"mapname")?"obj_delta":!strcmp(key,"sv_hostname")?"Native Server":!strcmp(key,"motd")?"Server MOTD":"-1";}
int CG_CalculateReinfTime(qboolean menu){(void)menu;return 3;}
void CG_DrawPicST(float x,float y,float w,float h,float s,float t,float s2,float t2,qhandle_t shader)
{(void)x;(void)y;(void)w;(void)h;(void)s;(void)t;(void)s2;(void)t2;(void)shader;}
void TCE_ModernFill(float x,float y,float w,float h,const float *c)
{(void)x;(void)y;(void)w;(void)h;(void)c;}
void TCE_ModernPanel(float x,float y,float w,float h,float alpha)
{(void)x;(void)y;(void)w;(void)h;(void)alpha;}
void TCE_ModernText(float x,float y,float scale,float width,const float *color,const char *text,int strong)
{(void)color;(void)strong;if(textCount<2048){Q_strncpyz(texts[textCount],text,256);textY[textCount]=y;textX[textCount]=x;textScale[textCount]=scale;textWidth[textCount++]=width;}}
void TCE_ModernParagraph(float x,float y,float scale,float width,float lineHeight,int lines,const float *c,const char *text)
{(void)lineHeight;(void)lines;TCE_ModernText(x,y,scale,width,c,text?text:"",0);}
void TCE_ModernImage(const char *path,float x,float y,float w,float h){(void)path;(void)x;(void)y;(void)w;(void)h;}
void TCE_ModernBackdrop(const char *path){TCE_ModernImage(path,0,0,852,480);}
void TCE_ModernCursor(float x,float y,qhandle_t shader,int centered)
{(void)x;(void)y;(void)shader;(void)centered;}
void TCE_ModernBar(float x,float y,float w,float h,float progress,int milliseconds)
{(void)x;(void)y;(void)w;(void)h;(void)milliseconds;lastProgress=progress;}
int trap_Milliseconds(void){return mockTime;}
void trap_GetHunkData(int *used,int *expected){*used=mockHunkUsed;*expected=mockHunkExpected;}
qhandle_t trap_R_RegisterShaderNoMip(const char *name){(void)name;return 1;}
int trap_FS_FOpenFile(const char *name,fileHandle_t *file,fsMode_t mode)
{(void)name;(void)mode;*file=mockMarkerLength>=0?1:0;return mockMarkerLength;}
void trap_FS_Read(void *data,int size,fileHandle_t file)
{(void)file;++mockReads;memcpy(data,mockMarker,(size_t)size);}
void trap_FS_FCloseFile(fileHandle_t file){(void)file;++mockCloses;}
void trap_GetClientState(uiClientState_t *state){*state=mockState;}
int trap_GetConfigString(int index,char *buffer,int size){(void)index;Q_strncpyz(buffer,"serverinfo",size);return 1;}
float trap_Cvar_VariableValue(const char *name)
{return !strcmp(name,"cl_downloadCount")?mockUsed:!strcmp(name,"cl_downloadSize")?mockExpected:mockStart;}
void trap_Cvar_VariableStringBuffer(const char *name,char *buffer,int size)
{(void)name;Q_strncpyz(buffer,mockDownload,size);}
void UI_ReadableSize(char *buffer,int size,int value){Com_sprintf(buffer,size,"%dB",value);}
void UI_PrintTime(char *buffer,int size,int value){Com_sprintf(buffer,size,"%ds",value);}
const char *CG_LoadPanel_GameTypeName(gametype_t game){(void)game;return "Objective";}
static int TCE_ScoreCountry(int client){(void)client;return -1;}
static int WM_DrawInfoLine(int x,int y,float fade){(void)x;(void)fade;return y;}

/* ACTUAL_MODEL */
/* ACTUAL_SNAPSHOT_STATE */
/* ACTUAL_SCOREBOARD */
/* ACTUAL_CONNECT */

static int occurrences(const char *text)
{int i,n=0;for(i=0;i<textCount;++i)if(!strcmp(texts[i],text))++n;return n;}
static int prefix(const char *text)
{int i,n=0;for(i=0;i<textCount;++i)if(!strncmp(texts[i],text,strlen(text)))++n;return n;}
static void reset(int count,int team)
{
    int i;memset(&cg,0,sizeof(cg));memset(&cgs,0,sizeof(cgs));memset(cg_entities,0,sizeof(cg_entities));memset(&snap,0,sizeof(snap));
    memset(tce_scoreExtended,1,sizeof(tce_scoreExtended));cg.snap=&snap;cg.numScores=count;cg.clientNum=0;
    snap.ps.pm_type=PM_NORMAL;snap.ps.stats[STAT_HEALTH]=100;snap.ps.persistant[PERS_TEAM]=team;
    cgs.gametype=cg_gameType.integer=5;cgs.tceKillMessage=1;
    for(i=0;i<count;++i){cg.scores[i].client=i;cg.scores[i].kills=12;cg.scores[i].deaths=2;cg.scores[i].suicides=1;
        cg.scores[i].teamKills=3;cg.scores[i].score=97;cg.scores[i].damageRating=125;cg.scores[i].classRating=4;
        cg.scores[i].ping=50;cg.scores[i].respawnsLeft=-1;cgs.clientinfo[i].team=team;cgs.clientinfo[i].health=100;
        Com_sprintf(cgs.clientinfo[i].name,sizeof(cgs.clientinfo[i].name),"Player%02d",i);}
    textCount=0;tceModernScoreFade=1;
}

int main(void)
{
    int i,page,seen[MAX_CLIENTS]={0};
    caseName="marker/progress";
    expect(TCE_ModernMarkerValid("TCE2_MODERN_UI 1\n",17),"exact opt-in marker");
    expect(!TCE_ModernMarkerValid("TCE2_MODERN_UI 1",16),"no truncated marker");
    expect(!TCE_ModernMarkerValid("TCE2_MODERN_UI 0\n",17),"disabled marker rejected");
    expect(!TCE_ModernMarkerValid("TCE2_MODERN_UI 1\nexec quit",26),"marker cannot execute config");
    expect(!TCE_ModernMarkerValid(NULL,17),"null marker rejected");
    expect(TCE_ModernProgress(0,0)<0,"unknown is indeterminate");
    expect(TCE_ModernProgress(-1,100)==0,"negative progress bounded");
    expect(TCE_ModernProgress(100,100)==1 && TCE_ModernProgress(200,100)==1,"full/oversize bounded");
    expect(TCE_ModernProgress(2147483646,2147483647)<1.00001f,"large counters finite");
    expect(TCE_ModernRosterPages(64,32)==2 && TCE_ModernRosterPages(64,24)==3,"complete roster paging");
    expect(TCE_ModernRosterPages(64,16)==4,"legible sixteen-player page capacity");
    expect(TCE_ModernRosterPages(0,0)==1 && TCE_ModernRosterPages(2147483647,32)==2,"page input bounds");
    caseName="native marker read";modernChecked=modernEnabled=0;mockMarkerLength=-1;
    expect(!TCE_ModernUIEnabled() && !mockReads && !mockCloses,"missing marker preserves legacy without reads");
    mockMarkerLength=17;memcpy(mockMarker,"TCE2_MODERN_UI 1\n",17);
    expect(!TCE_ModernUIEnabled() && !mockReads,"changing VFS requires module reset");
    modernChecked=0;expect(TCE_ModernUIEnabled() && mockReads==1 && mockCloses==1,"exact marker activates and closes");
    expect(TCE_ModernUIEnabled() && mockReads==1,"native marker is cached");
    modernChecked=modernEnabled=0;mockMarkerLength=60;
    expect(!TCE_ModernUIEnabled() && mockReads==1 && mockCloses==2,"oversize marker never read, handle closed");
    caseName="GT5 hidden statistics";reset(1,TEAM_AXIS);cgs.tceKillMessage=0;
    TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);expect(occurrences("n/a")==4,"four protected columns stay private");
    textCount=0;cgs.tceKillMessage=1;TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
    expect(!occurrences("n/a") && occurrences("1.25")==1 && occurrences("5")==1,"DR and AA retain correct meaning");
    textCount=0;cg.scores[0].damageRating=100000000;TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
    expect(occurrences("999999.98")==1,"legacy binary32 .01 rating constant retained");
    textCount=0;cgs.tceKillMessage=0;snap.ps.stats[STAT_HEALTH]=0;
    TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);expect(!occurrences("n/a"),"dead-viewer values remain available");
    caseName="seven-field transport";reset(1,TEAM_AXIS);tce_scoreExtended[0]=qfalse;
    TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
    expect(occurrences("-")==6 && occurrences("97")==1,"missing extended fields never invented");
    caseName="teammate state";reset(2,TEAM_AXIS);cg.scores[1].respawnsLeft=-2;
    cg_entities[1].currentValid=qtrue;cg_entities[1].currentState.number=1;cg_entities[1].currentState.clientNum=1;
    cg_entities[1].currentState.eType=ET_PLAYER;
    TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);expect(!occurrences("OUT"),"fresh living teammate supersedes stale packet");
    textCount=0;cg_entities[1].currentValid=qfalse;TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
    expect(occurrences("OUT")==1,"off-PVS retains wire status");
    textCount=0;cgs.clientinfo[1].powerups=0xc0;TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
    expect(occurrences("OBJ")==1 && occurrences("OUT")==1,"objective and eliminated indicators both retained");
    caseName="enemy privacy";reset(2,TEAM_ALLIES);cgs.clientinfo[0].team=TEAM_AXIS;cg.scores[1].respawnsLeft=-2;cgs.clientinfo[1].health=-1;
    TCE_ModernScoreTeam(0,96,TEAM_ALLIES,1,20);expect(!occurrences("OUT") && !occurrences("DOWN"),"living observer gets no enemy status");
    caseName="legible standard layout";reset(2,TEAM_AXIS);TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
    {
        float first=0,second=0;int found=0;
        for(i=0;i<textCount;++i)if(!strncmp(texts[i],"Player",6))
        {
            if(!found)first=textY[i];else second=textY[i];++found;
            expect(textScale[i]>=.239f,"standard names use readable .24 font");
            expect(textX[i]==36 && textWidth[i]>=120,"empty status no longer steals healthy player name width");
        }
        expect(found==2 && second-first>21.99f && second-first<22.01f,"native standard row spacing22");
        expect(occurrences("KILL")==1 && occurrences("DTH")==1 && occurrences("SUI")==1,"legible stat headers retained");
    }
    caseName="64 player roster";reset(64,TEAM_AXIS);
    for(page=0;page<4;++page)
    {
        textCount=0;cg.time=page*8000;TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);
        expect(prefix("Player")==16,"sixteen-player native page count");
        for(i=0;i<textCount;++i)expect(textY[i]<405,"row stays above spectators");
        for(i=0;i<MAX_CLIENTS;++i)
        {
            char name[32];int present;Com_sprintf(name,sizeof(name),"Player%02d",i);present=occurrences(name);
            expect(present==(i>=page*16 && i<(page+1)*16),"exact page membership, no duplicate");seen[i]+=present;
        }
    }
    for(i=0;i<MAX_CLIENTS;++i)expect(seen[i]==1,"all64 players appear exactly once in complete rotation");
    caseName="spectators";reset(64,TEAM_SPECTATOR);TCE_ModernScoreObjectives(0,0,0,1);
    expect(prefix("Player")==24,"spectator page one");textCount=0;cg.time=16000;TCE_ModernScoreObjectives(0,0,0,1);
    expect(prefix("Player")==16 && occurrences("Player63")==1,"last spectator page no omission or duplicate");
    caseName="invalid client";reset(1,TEAM_AXIS);cg.scores[0].client=MAX_CLIENTS+20;
    TCE_ModernScoreTeam(0,96,TEAM_AXIS,1,20);expect(prefix("Player")==0,"invalid slot never indexed");
    caseName="native connect status";textCount=0;memset(&mockState,0,sizeof(mockState));
    Q_strncpyz(mockState.servername,"test.example:27960",sizeof(mockState.servername));
    mockState.connState=CA_CONNECTING;mockState.connectPacketCount=3;
    UI_DrawModernLoadPanel(qfalse);
    expect(occurrences("CONNECTING")==1 && !occurrences("obj_delta"),"previous gamestate map not exposed while connecting");
    expect(prefix("Awaiting connection...")==1 && lastProgress<0,"real connect counter / no invented progress");
    textCount=0;mockState.connState=CA_CHALLENGING;UI_DrawModernLoadPanel(qfalse);
    expect(prefix("Awaiting challenge...")==1,"native challenge status retained");
    textCount=0;mockState.connState=CA_LOADING;UI_DrawModernLoadPanel(qfalse);
    expect(occurrences("obj_delta")>=1 && occurrences("Loading map and game assets")==1,"new gamestate map becomes available");
    caseName="download progress";textCount=0;mockState.connState=CA_CONNECTED;
    Q_strncpyz(mockDownload,"map_asset.pk3",sizeof(mockDownload));mockUsed=500;mockExpected=1000;mockStart=1000;
    UI_DrawModernLoadPanel(qfalse);expect(lastProgress==.5f && occurrences("map_asset.pk3")==1,"native file/count/size shown");
    expect(prefix("500B / 1000B")==1,"download transferred bytes/total/rate/ETA retained");
    textCount=0;mockExpected=0;UI_DrawModernLoadPanel(qfalse);
    expect(lastProgress<0 && prefix("500B / unknown size")==1,"unknown file size remains indeterminate");
    caseName="cgame load";textCount=0;reset(0,TEAM_AXIS);mockHunkUsed=25;mockHunkExpected=100;
    Q_strncpyz(cgs.rawmapname,"obj_delta",sizeof(cgs.rawmapname));Q_strncpyz(cg.infoScreenText,"World collision",sizeof(cg.infoScreenText));
    CG_DrawModernLoadPanel(qfalse);
    expect(lastProgress==.25f && occurrences("World collision")==1,"actual hunk progress and asset stage");
    expect(occurrences("Objective")==1 && occurrences("Native Server")==1,"gametype and server retained");
    caseName="mission content";textCount=0;cgs.arenaInfoLoaded=qtrue;cgs.gametype=GT_WOLF;
    Q_strncpyz(cgs.arenaData.longname,"Delta Operations",sizeof(cgs.arenaData.longname));
    Q_strncpyz(cgs.arenaData.authors,"Original Map Author",sizeof(cgs.arenaData.authors));
    Q_strncpyz(cgs.arenaData.description,"Ordinary mission briefing",sizeof(cgs.arenaData.description));
    Q_strncpyz(cgs.arenaData.lmsdescription,"Last survivor briefing",sizeof(cgs.arenaData.lmsdescription));
    CG_DrawModernLoadPanel(qfalse);
    expect(occurrences("Delta Operations")==1 && occurrences("Original Map Author")==1,"map title and authors retained");
    expect(occurrences("Ordinary mission briefing")==1,"ordinary map description retained");
    textCount=0;cgs.gametype=GT_WOLF_LMS;CG_DrawModernLoadPanel(qfalse);
    expect(occurrences("Last survivor briefing")==1 && !occurrences("Ordinary mission briefing"),"LMS-specific briefing selected");
    textCount=0;cgs.gametype=GT_WOLF_CAMPAIGN;DC->nameForCampaign=FixtureCampaignName;
    DC->descriptionForCampaign=FixtureCampaignDescription;cgs.currentCampaignMap=2;cgs.campaignData.mapCount=6;
    CG_DrawModernLoadPanel(qfalse);
    expect(occurrences("Northern Operations / MISSION 3 OF 6")==1,"campaign name and mission index retained");
    expect(occurrences("Campaign mission briefing")==1,"campaign-specific description retained");
    for(i=0;i<textCount;++i)if(!strcmp(texts[i],"Campaign mission briefing"))
        expect(textY[i]==218 && textWidth[i]==240,"campaign briefing stays inside existing mission panel");
    textCount=0;cgs.currentCampaignMap=2147483647;CG_DrawModernLoadPanel(qfalse);
    expect(occurrences("Northern Operations")==1,"invalid campaign index cannot overflow or invent a mission");
    textCount=0;DC->nameForCampaign=NULL;CG_DrawModernLoadPanel(qfalse);
    expect(!prefix("Northern Operations") && occurrences("Campaign mission briefing")==1,"missing campaign-name callback retains briefing");
    printf("{\"checks\":%d,\"failures\":%d}\n",checks,failures);return failures?1:0;
}
