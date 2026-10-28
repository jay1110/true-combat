#define CGAMEDLL
#include "src/cgame/cg_local.h"
#include "src/cgame/tce_popup_draw.h"
#include "src/ui/tce_viewport.h"
#include "src/game/tce_bg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

cg_t cg;
cgs_t cgs;
tce_gearDef_t gearDef;
weaponInfo_t cg_weapons[MAX_WEAPONS];
static int iconsEnabled, modelCount, modelRegistrations, modelBoundsInvalid;
static vmCvar_t *iconCvar;
static refdef_t modelViews[16];
static refEntity_t modelEntities[16], currentModel;
const char *bg_fireteamNames[MAX_FIRETEAMS / 2] = { "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot" };
vmCvar_t cg_drawFireteamOverlay,cg_paused,cg_aspectMode,cg_drawTeamNames;
centity_t cg_entities[MAX_GENTITIES];
int tceSmokeNewBBox;
static int traceCalls, traceBlocked;
static int modernHud;
static float hudUserScale=.85f,hudHeightCap=1080;
static vmCvar_t *hudCvars[3];
qboolean tce_uiCoordinates;
static snapshot_t snapshot;
static int checks, failures, argumentCount, draws, fills;
static int menuDraws,overlayDraws;
static rectDef_t hudRect;
static char arguments[64][96], rendered[8192], locations[TCE_MAX_LOCATIONS][96];
static char fireteamConfig[MAX_FIRETEAMS][128];
static float lastBarWidth, barStart, measuredNameEnd;
static float panelHeight,lastTextY;
static int paintCount,picCount;
static struct { float x,y,w,h; } pictures[24];
static struct { float x,y,scale,color[4];char text[256]; } paints[64];
static int sortedFireTeamClients[MAX_CLIENTS];
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL %d: %s\n",__LINE__,#c); } } while (0)

void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_Error(int level, const char *format, ...) { (void)level;(void)format;abort(); }
int Q_vsnprintf(char *dest,int size,const char *format,va_list args) { return vsnprintf(dest,(size_t)size,format,args); }
void trap_Cvar_Register(vmCvar_t *cvar,const char *name,const char *value,int flags)
{
    int id;
    if(!strcmp(name,"tceft")||!strcmp(name,"tceftgear")){CHECK(!strcmp(value,"1")&&(flags&CVAR_USERINFO));return;}
    if(!strcmp(name,"cg_drawIcons")){CHECK(!strcmp(value,"1")&&flags==CVAR_ARCHIVE);iconCvar=cvar;cvar->integer=iconsEnabled;return;}
    id=!strcmp(name,"cg_wideScreen")?0:!strcmp(name,"cg_hudScale")?1:!strcmp(name,"cg_hudMaxHeight")?2:-1;
    CHECK(id>=0&&flags==CVAR_ARCHIVE);if(id<0)return;
    CHECK(!strcmp(value,id==0?"1":id==1?"0.85":"1080"));hudCvars[id]=cvar;
    cvar->value=id==0?(float)modernHud:id==1?hudUserScale:hudHeightCap;cvar->integer=(int)cvar->value;
}
void trap_Cvar_Update(vmCvar_t *cvar)
{
    int id;
    if(cvar==iconCvar){cvar->integer=iconsEnabled;return;}
    for(id=0;id<3;++id)if(hudCvars[id]==cvar){cvar->value=id==0?(float)modernHud:id==1?hudUserScale:hudHeightCap;cvar->integer=(int)cvar->value;return;}
    CHECK(0);
}
const char *CG_Argv(int index) { return index>=0&&index<argumentCount?arguments[index]:""; }
int trap_Argc(void) { return argumentCount; }
const char *CG_ConfigString(int index)
{
    if(index>=CS_FIRETEAMS&&index<CS_FIRETEAMS+MAX_FIRETEAMS)return fireteamConfig[index-CS_FIRETEAMS];
    CHECK(index>=CS_LOCATIONS&&index<MAX_CONFIGSTRINGS);
    return index>=CS_LOCATIONS&&index<MAX_CONFIGSTRINGS?locations[index-CS_LOCATIONS]:"";
}
int CG_Text_Width_Ext(const char *text,float scale,int limit,fontInfo_t *font)
{ (void)limit;(void)font;return (int)(strlen(text)*scale*32.f); }
void CG_Text_Paint_Ext(float x,float y,float sx,float sy,vec4_t color,const char *text,float adjust,int limit,int style,fontInfo_t *font)
{
    (void)adjust;(void)limit;(void)style;(void)font;
    CHECK(x>=0&&sx>0&&sx<=.25f&&sy==sx&&color[3]>0);
    CHECK(strlen(rendered)+strlen(text)+2<sizeof(rendered));
    strcat(rendered,text);strcat(rendered,"\n");++draws;
    lastTextY=y;
    if(paintCount<64){paints[paintCount].x=x;paints[paintCount].y=y;paints[paintCount].scale=sx;memcpy(paints[paintCount].color,color,sizeof(vec4_t));Q_strncpyz(paints[paintCount++].text,text,256);}
}
void CG_FillRect(float x,float y,float w,float h,const float *color)
{ (void)x;(void)y;(void)color;CHECK(w>=0&&h>=0);if(!fills)panelHeight=h;if(h==4){lastBarWidth=w;if(w==25)barStart=x;}++fills; }
qhandle_t trap_R_RegisterShaderNoMip(const char *name) { CHECK(strstr(name,"sprites/")||strstr(name,"gfx/tce2/nav/")||strstr(name,"gfx/tce2/fireteam/"));return 42; }
void CG_Trace(trace_t *out,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int skip,int mask)
{ (void)start;(void)mins;(void)maxs;(void)end;CHECK(skip==snapshot.ps.clientNum&&mask==(CONTENTS_SOLID|CONTENTS_BODY));memset(out,0,sizeof(*out));out->fraction=traceBlocked?.5f:1.f;out->entityNum=ENTITYNUM_WORLD;++traceCalls; }
vec_t VectorNormalize(vec3_t vector)
{ float n=sqrtf(DotProduct(vector,vector));if(n){vector[0]/=n;vector[1]/=n;vector[2]/=n;}return n; }
void CG_DrawRect(float x,float y,float w,float h,float size,const float *color)
{ (void)x;(void)y;(void)color;CHECK(w>0&&h>0&&size>0); }
char *BG_GetLocationString(vec2_t pos) { static char grid[32];sprintf(grid,"B3[%i,%i]",(int)pos[0],(int)pos[1]);return grid; }
void trap_R_SetColor(const float *color) {(void)color;}
void trap_R_DrawStretchPic(float x,float y,float w,float h,float s1,float t1,float s2,float t2,qhandle_t shader)
{ (void)x;(void)y;(void)s1;(void)t1;(void)s2;(void)t2;(void)shader;CHECK(w>=0&&h>=0); }
void CG_DrawPic(float x,float y,float w,float h,qhandle_t shader)
{ (void)shader;if(picCount<24){pictures[picCount].x=x;pictures[picCount].y=y;pictures[picCount].w=w;pictures[picCount++].h=h;} }
void CG_DrawSmallStringColor(int x,int y,const char *text,vec4_t color)
{ (void)x;(void)text;(void)color;lastTextY=(float)y; }
int CG_TimeForPopup(popupMessageType_t type) {(void)type;return 10000;}

/* ACTUAL_VIEWPORT_CONTEXT */

void AxisClear(vec3_t axis[3]) { memset(axis,0,sizeof(vec3_t)*3);axis[0][0]=axis[1][1]=axis[2][2]=1; }
void CG_RegisterWeapon(int id,qboolean force) { CHECK(id>0&&id<TCE_MAX_WEAPONS&&!force);++modelRegistrations; }
void trap_R_ModelBounds(clipHandle_t model,vec3_t mins,vec3_t maxs)
{ CHECK(model>0);VectorSet(mins,-31,-4,-7);VectorSet(maxs,39,6,11);if(modelBoundsInvalid)mins[0]=(float)strtod("nan",NULL); }
void trap_R_ClearScene(void) {}
void trap_R_AddRefEntityToScene(const refEntity_t *entity) { currentModel=*entity; }
void trap_R_RenderScene(const refdef_t *view)
{ CHECK(modelCount<16);if(modelCount<16){modelViews[modelCount]=*view;modelEntities[modelCount++]=currentModel;} }
/* ACTUAL_TC_CLASS_FUNCTION */
/* ACTUAL_SORT_FUNCTIONS */
#include "src/cgame/cg_tce_fireteam.c"
void CG_Fireteams_Draw(void) { ++menuDraws; }
void CG_DrawFireTeamOverlay(rectDef_t *rect)
{ ++overlayDraws;hudRect=*rect;CG_DrawTCEFireteamOverlay(rect); }
/* ACTUAL_HUD_FUNCTION */
typedef tce_popup_draw_item_t pmListItem_t;
static pmListItem_t popupItems[5],*cg_pmWaitingList,*cg_pmOldList;
static float popupOffset,popupBottom;
static qboolean popupOffsetReady;
/* ACTUAL_POPUP_FUNCTIONS */
#include "src/cgame/tce_popup_draw.c"

static void packet(const char *text)
{
    char buffer[1024],*word;
    Q_strncpyz(buffer,text,sizeof(buffer));argumentCount=0;
    word=strtok(buffer," ");
    while(word){CHECK(argumentCount<64);Q_strncpyz(arguments[argumentCount++],word,96);word=strtok(NULL," ");}
}
static void render(void)
{
    rectDef_t rect={8,8,220,0};draws=fills=paintCount=picCount=modelCount=modelRegistrations=0;lastBarWidth=-1;rendered[0]=0;CG_DrawTCEFireteamOverlay(&rect);
}
static void setup(void)
{
    int i;
    iconsEnabled=modelCount=modelRegistrations=modelBoundsInvalid=0;memset(&gearDef,0,sizeof(gearDef));memset(cg_weapons,0,sizeof(cg_weapons));
    memset(&cg,0,sizeof(cg));memset(&cgs,0,sizeof(cgs));memset(&snapshot,0,sizeof(snapshot));
    memset(fireteamConfig,0,sizeof(fireteamConfig));
    cg.snap=&snapshot;cg.clientNum=0;cg.time=10000;snapshot.ps.clientNum=0;snapshot.ps.stats[STAT_HEALTH]=100;
    cg_paused.integer=0;cg_aspectMode.integer=0;tce_uiCoordinates=qfalse;
    modernHud=0;hudUserScale=.85f;hudHeightCap=1080;CG_HudViewportInit();
    cgs.glconfig.vidWidth=1280;cgs.glconfig.vidHeight=720;cgs.screenXScale=2.f;cgs.screenYScale=1.5f;
    cg_pmWaitingList=cg_pmOldList=NULL;popupBottom=0;picCount=paintCount=0;
    rendered[0]=0;draws=fills=0;
    cg_drawFireteamOverlay.integer=1;cg.fireTeams[0].inuse=qtrue;cg.fireTeams[0].leader=0;
    for(i=0;i<2;++i){cgs.clientinfo[i].infoValid=qtrue;cgs.clientinfo[i].team=TEAM_AXIS;cgs.clientinfo[i].fireteamData=&cg.fireTeams[0];}
    Q_strncpyz(cgs.clientinfo[0].name,"Leader",sizeof(cgs.clientinfo[0].name));
    Q_strncpyz(cgs.clientinfo[1].name,"Partner",sizeof(cgs.clientinfo[1].name));
    strcpy(locations[1],"North Warehouse");strcpy(locations[2],"South Tower");
    CG_TCEFireteamReset();CG_SortClientFireteam();
}
static void parseGood(void)
{
    packet("tceftinfo 0 2 0 100 1 120 220 30 1 75 2 200 300 40");CG_ParseTCEFireteamInfo();
}
static void protocolAndPresentation(void)
{
    int kind;
    tce_fireteamInfo_t before[MAX_CLIENTS];
    setup();parseGood();CHECK(fireteamInfo[1].valid&&fireteamInfo[1].health==75&&fireteamInfo[1].area==2);
    render();CHECK(strstr(rendered,"Partner")&&!strstr(rendered,"75")&&strstr(rendered,"South Tower"));
    CHECK(lastBarWidth==18.75f&&panelHeight==46.f);
    memcpy(before,fireteamInfo,sizeof(before));
    for(kind=0;kind<12;++kind){
        parseGood();
        switch(kind){
        case 0:strcpy(arguments[9],"64");break;
        case 1:strcpy(arguments[9],"-1");break;
        case 2:strcpy(arguments[9],"0");break;
        case 3:strcpy(arguments[1],"4");break;
        case 4:strcpy(arguments[2],"7");break;
        case 5:strcpy(arguments[10],"1000");break;
        case 6:strcpy(arguments[11],"99999999");break;
        case 7:strcpy(arguments[14],"1048577");break;
        case 8:strcpy(arguments[14],"999999999999999999999");break;
        case 9:strcpy(arguments[14],"oops");break;
        case 10:argumentCount--;break;
        case 11:cgs.clientinfo[1].team=TEAM_ALLIES;break;
        }
        CG_ParseTCEFireteamInfo();CHECK(!memcmp(before,fireteamInfo,sizeof(before)));cgs.clientinfo[1].team=TEAM_AXIS;
    }
    cg.time+=2501;render();CHECK(strstr(rendered,"...")&&!strstr(rendered,"75")&&!strstr(rendered,"South Tower"));
    parseGood();strcpy(arguments[10],"0");CG_ParseTCEFireteamInfo();render();CHECK(strstr(rendered,"DEAD")&&!strstr(rendered,"Killed in action"));
    strcpy(arguments[10],"-1");CG_ParseTCEFireteamInfo();render();CHECK(strstr(rendered,"WAIT")&&!strstr(rendered,"Waiting to respawn"));
    parseGood();render();CHECK(fireteamInfo[1].health==75&&!strstr(rendered,"DEAD")&&!strstr(rendered,"WAIT"));
    snapshot.ps.stats[STAT_HEALTH]=-1;render();CHECK(strstr(rendered,"DEAD")&&!strstr(rendered,"WAIT"));
    snapshot.ps.pm_flags=PMF_LIMBO;render();CHECK(strstr(rendered,"WAIT"));
    snapshot.ps.pm_flags=0;snapshot.ps.stats[STAT_HEALTH]=100;
    memset(cgs.clientinfo[1].name,'W',sizeof(cgs.clientinfo[1].name)-1);cgs.clientinfo[1].name[sizeof(cgs.clientinfo[1].name)-1]=0;
    render();CHECK(strstr(rendered,"...")!=NULL);
    cg_drawFireteamOverlay.integer=0;render();CHECK(draws==0&&fills==0);
    cg_drawFireteamOverlay.integer=1;cgs.clientinfo[0].team=TEAM_SPECTATOR;render();CHECK(draws==0&&fills==0);
    cgs.clientinfo[0].team=TEAM_AXIS;cgs.clientinfo[0].fireteamData=NULL;render();CHECK(draws==0&&fills==0);
}
static void sorting(void)
{
    int i,j,a,b;
    setup();
    for(i=0;i<MAX_CLIENTS;++i)for(j=0;j<MAX_CLIENTS;++j){
        a=CG_SortFireTeam(&i,&j);b=CG_SortFireTeam(&j,&i);CHECK((a<0&&b>0)||(a>0&&b<0)||(!a&&!b));
    }
    CHECK(CG_SortedFireTeamPlayerForPosition(0,6)==&cgs.clientinfo[0]);
    CHECK(CG_SortedFireTeamPlayerForPosition(1,6)==&cgs.clientinfo[1]);
    CHECK(CG_SortedFireTeamPlayerForPosition(2,6)==NULL);
    cgs.clientinfo[1].team=TEAM_ALLIES;CG_SortClientFireteam();CHECK(CG_SortedFireTeamPlayerForPosition(1,6)==NULL);
}
static void fullFireteam(void)
{
    const int ids[6]={0,1,7,31,32,63};
    char text[800],entry[96],name[32];
    int i,j;
    setup();
    for(i=0;i<6;++i){
        clientInfo_t *ci=&cgs.clientinfo[ids[i]];
        ci->infoValid=qtrue;ci->team=TEAM_AXIS;
        Com_sprintf(ci->name,sizeof(ci->name),"Member%02i",ids[i]);
    }
    /* Exact wire encoding from G_UpdateFireteamConfigString: both mask halves,
     * including their signed bit31 boundary, not a contiguous low-bit shortcut. */
    strcpy(fireteamConfig[0],"\\id\\0\\l\\0\\c\\8000000180000083");
    CG_ParseFireteams();
    for(i=0;i<MAX_CLIENTS;++i){
        int member=0;
        for(j=0;j<6;++j)if(ids[j]==i)member=1;
        CHECK((CG_IsOnFireteam(i)==&cg.fireTeams[0])==member);
        CHECK((cg.fireTeams[0].joinOrder[i]!=0)==member);
    }
    strcpy(text,"tceftinfo 0 6");
    for(i=0;i<6;++i){
        Com_sprintf(entry,sizeof(entry)," %i %i 2 %i %i 40",ids[i],80+i,100+ids[i],200+ids[i]);
        Q_strcat(text,sizeof(text),entry);
    }
    packet(text);CG_ParseTCEFireteamInfo();render();
    CHECK(strstr(rendered,"Fireteam Alpha")!=NULL);
    for(i=0;i<6;++i){
        const char *found;
        Com_sprintf(name,sizeof(name),"Member%02i",ids[i]);found=strstr(rendered,name);
        CHECK(found!=NULL);if(found)CHECK(strstr(found+strlen(name),name)==NULL);
        CHECK(fireteamInfo[ids[i]].valid&&fireteamInfo[ids[i]].health==80+i);
        CHECK(CG_SortedFireTeamPlayerForPosition(i,6)==&cgs.clientinfo[ids[i]]);
    }
    CHECK(CG_SortedFireTeamPlayerForPosition(6,6)==NULL);
    CHECK(draws==14); /* Header, leader mark + six name/location rows. */
}
static void membershipAndSlotReuse(void)
{
    setup();parseGood();CHECK(fireteamInfo[1].valid&&fireteamInfo[1].health==75);
    strcpy(fireteamConfig[0],"\\id\\0\\l\\0\\c\\0000000000000003");
    CG_ParseFireteams();CHECK(!fireteamInfo[0].valid&&!fireteamInfo[1].valid);
    CHECK(CG_IsOnFireteam(0)==&cg.fireTeams[0]&&CG_IsOnFireteam(1)==&cg.fireTeams[0]);
    parseGood();CHECK(fireteamInfo[1].valid);
    /* Server disconnect removes the bit before the same slot can rejoin. */
    cgs.clientinfo[1].infoValid=qfalse;
    strcpy(fireteamConfig[0],"\\id\\0\\l\\0\\c\\0000000000000001");
    CG_ParseFireteams();render();
    CHECK(CG_IsOnFireteam(1)==NULL&&!fireteamInfo[1].valid&&!strstr(rendered,"Partner"));
    cgs.clientinfo[1].infoValid=qtrue;strcpy(cgs.clientinfo[1].name,"Rejoined");
    strcpy(fireteamConfig[0],"\\id\\0\\l\\0\\c\\0000000000000003");
    CG_ParseFireteams();render();
    CHECK(CG_IsOnFireteam(1)==&cg.fireTeams[0]&&!fireteamInfo[1].valid);
    CHECK(strstr(rendered,"Rejoined")&&strstr(rendered,"...")&&!strstr(rendered,"75"));
    packet("tceftinfo 0 2 0 100 1 120 220 30 1 22 1 900 901 40");CG_ParseTCEFireteamInfo();render();
    CHECK(fireteamInfo[1].health==22&&strstr(rendered,"North Ware")&&!strstr(rendered,"B3[900,901]"));
    CHECK(!strstr(rendered,"75")&&!strstr(rendered,"South Tower"));
    strcpy(fireteamConfig[0],"\\id\\-1");CG_ParseFireteams();render();
    CHECK(!fireteamInfo[1].valid&&CG_IsOnFireteam(0)==NULL&&CG_IsOnFireteam(1)==NULL&&draws==0);
}
static void following(void)
{
    setup();
    packet("tceftinfo 0 2 0 -1 1 120 220 30 1 75 2 200 300 40");CG_ParseTCEFireteamInfo();
    snapshot.ps.clientNum=1;snapshot.ps.pm_flags=PMF_FOLLOW|PMF_LIMBO;
    snapshot.ps.stats[STAT_HEALTH]=12;snapshot.ps.holdable[13]=1;
    VectorSet(snapshot.ps.origin,-900,-901,30);render();
    CHECK(strstr(rendered,"WAIT")&&fireteamInfo[1].health==75&&strstr(rendered,"South Tower"));
    CHECK(!strstr(rendered,"12")&&!strstr(rendered,"B3[-900,-901]"));
    packet("tceftinfo 0 2 0 -1 1 120 220 30 1 0 2 200 300 40");CG_ParseTCEFireteamInfo();
    snapshot.ps.stats[STAT_HEALTH]=100;render();
    CHECK(strstr(rendered,"WAIT")&&strstr(rendered,"DEAD")&&!strstr(rendered,"100"));
    cgs.clientinfo[0].team=TEAM_SPECTATOR;snapshot.ps.persistant[PERS_TEAM]=TEAM_AXIS;render();
    CHECK(draws==0&&fills==0);
}
static void hudIntegration(void)
{
    const int flags[]={0,PMF_FOLLOW,PMF_LIMBO,PMF_FOLLOW|PMF_LIMBO};
    const int health[]={100,0,-1};
    int f,h;
    setup();parseGood();
    for(f=0;f<4;++f)for(h=0;h<3;++h){
        snapshot.ps.pm_flags=flags[f];snapshot.ps.stats[STAT_HEALTH]=health[h];
        snapshot.ps.clientNum=(flags[f]&PMF_FOLLOW)?1:0;
        menuDraws=overlayDraws=0;CG_DrawFireteamHUD();
        CHECK(menuDraws==0&&overlayDraws==1);
        CHECK(hudRect.x==8&&hudRect.w==220);
        CHECK(hudRect.y==8);
        CHECK(hudRect.y+14+6*14==106); /* six single-line players */
    }
    cg.showFireteamMenu=qtrue;cgs.eventHandling=CGAME_EVENT_FIRETEAMMSG;
    menuDraws=overlayDraws=0;CG_DrawFireteamHUD();CHECK(menuDraws==1&&overlayDraws==0);
    cgs.eventHandling=CGAME_EVENT_NONE;menuDraws=overlayDraws=0;
    CG_DrawFireteamHUD();CHECK(menuDraws==0&&overlayDraws==0);
    cg.showFireteamMenu=qfalse;menuDraws=overlayDraws=0;
    CG_DrawFireteamHUD();CHECK(menuDraws==0&&overlayDraws==1);
}
static void compactLayout(void)
{
    int count,i,row,seen;
    char name[256];
    for(count=1;count<=6;++count){
        setup();
        for(i=0;i<6;++i){
            cgs.clientinfo[i].infoValid=i<count;cgs.clientinfo[i].team=TEAM_AXIS;
            cgs.clientinfo[i].fireteamData=i<count?&cg.fireTeams[0]:NULL;
            sprintf(cgs.clientinfo[i].name,"^1Player%02i",i);
        }
        CG_SortClientFireteam();render();
        CHECK(panelHeight==18+14*count&&CG_TCEFireteamHeight()==panelHeight);
        CHECK(paintCount==2+2*count); /* leader, name/location per player */
        for(row=0;row<count;++row){
            seen=0;
            for(i=0;i<paintCount;++i)if(paints[i].y==8+16+row*14+10){
                ++seen;CHECK(paints[i].x>=8&&paints[i].x<228);
                CHECK(!strchr(paints[i].text,'^'));
                if(paints[i].x==32)CHECK(strlen(paints[i].text)<=13);
                if(paints[i].x==136)CHECK(CG_Text_Width_Ext(paints[i].text,.13f,0,NULL)<=78);
            }
            CHECK(seen==(row==0?3:2));
        }
    }
    strcpy(name,"^1WWWWWW^^WWWWWW^^1WWWWWWWWWWWWWWWW");CG_TCEFireteamFit(name,61,.13f);
    CHECK(!strchr(name,'^')&&strstr(name,"...")&&CG_Text_Width_Ext(name,.13f,0,NULL)<=61);
    cgs.clientinfo[1].fireteamData=NULL;CG_SortClientFireteam();CHECK(CG_TCEFireteamHeight()==88);
    cg.showFireteamMenu=qtrue;CHECK(CG_TCEFireteamHeight()==0);
    cg.showFireteamMenu=qfalse;cg_paused.integer=1;CHECK(CG_TCEFireteamHeight()==0);
    cg_paused.integer=0;cg_drawFireteamOverlay.integer=0;CHECK(CG_TCEFireteamHeight()==0);
}
static void gearAndMeasuredLayout(void)
{
    const int widths[]={1024,1280,1280,3440,3840},heights[]={768,720,800,1440,2160};
    tce_fireteamInfo_t before[MAX_CLIENTS];int i,screen,corner;char expected[64];
    setup();parseGood();render();measuredNameEnd=0;
    for(i=0;i<paintCount;++i)if(paints[i].x==32&&paints[i].y>22){
        float right=paints[i].x+CG_Text_Width_Ext(paints[i].text,.14f,0,NULL);if(right>measuredNameEnd)measuredNameEnd=right;
    }
    CHECK(barStart-measuredNameEnd==5.f); /* shared measured name column, no 72-unit slot */
    for(i=0;i<MAX_FIRETEAMS/2;++i){cg.fireTeams[0].ident=i;render();sprintf(expected,"Fireteam %s",bg_fireteamNames[i]);CHECK(strstr(rendered,expected)!=NULL);}
    setup();iconsEnabled=1;snapshot.ps.weapon=44;snapshot.ps.stats[STAT_PLAYER_CLASS]=2;
    strcpy(gearDef.weaponFile[44],"custom-gear-m4.weap");strcpy(gearDef.weaponFile[43],"ak47.weap");
    cg_weapons[44].weaponModel[W_PU_MODEL].model=1044;cg_weapons[44].weaponModel[W_PU_MODEL].skin[0]=2044;
    cg_weapons[43].weaponModel[W_PU_MODEL].model=1043;cg_weapons[43].weaponModel[W_PU_MODEL].skin[0]=2043;
    cg_entities[1].currentValid=qfalse;cg_entities[1].currentState.weapon=7;cgs.clientinfo[1].cls=1;
    packet("tceftinfo 0 2 0 100 1 120 220 30 0 7 0 1 75 2 200 300 40 0 43 4");CG_ParseTCEFireteamInfo();
    CHECK(fireteamInfo[1].weapon==43&&fireteamInfo[1].playerClass==4);
    for(screen=0;screen<5;++screen){
        cgs.glconfig.vidWidth=widths[screen];cgs.glconfig.vidHeight=heights[screen];
        cgs.screenXScale=widths[screen]/640.f;cgs.screenYScale=heights[screen]/480.f;
        modernHud=1;CG_HudViewportUpdate();CG_HudViewportBegin(0,0);render();
        CHECK(modelCount==2&&modelRegistrations==2&&picCount==2);
        CHECK(modelEntities[0].hModel==1044&&modelEntities[0].customSkin==2044);
        CHECK((modelEntities[0].renderfx&(RF_HILIGHT|RF_MINLIGHT|RF_NOSHADOW))==(RF_HILIGHT|RF_MINLIGHT|RF_NOSHADOW));
        CHECK(modelEntities[1].hModel==1043&&modelEntities[1].customSkin==2043);
        for(i=0;i<modelCount;++i){
            refdef_t *v=&modelViews[i];refEntity_t *e=&modelEntities[i];
            float ty=tanf(v->fov_y*(float)(M_PI/360.0)),tx=tanf(v->fov_x*(float)(M_PI/360.0));
            CHECK(v->rdflags==RDF_NOWORLDMODEL&&v->width>0&&v->height>0&&v->time==cg.time);
            CHECK(fabsf(tx/ty-(float)v->width/v->height)<.0001f);
            CHECK((float)v->width/v->height>2.7f); /*34x12 enlarged icon, integer pixel rounding */ /* physical aspect, not HUD math ratio */
            for(corner=0;corner<8;++corner){
                float x=e->origin[0]-(corner&2?6.f:-4.f),y=e->origin[1]+(corner&1?39.f:-31.f),z=e->origin[2]+(corner&4?11.f:-7.f);
                CHECK(x>0&&fabsf(y)<x*tx&&fabsf(z)<x*ty);
            }
        }
        CG_HudViewportEnd();
    }
    memcpy(before,fireteamInfo,sizeof(before));strcpy(arguments[19],"64");CG_ParseTCEFireteamInfo();CHECK(!memcmp(before,fireteamInfo,sizeof(before)));
    strcpy(arguments[19],"43");strcpy(arguments[20],"7");CG_ParseTCEFireteamInfo();CHECK(!memcmp(before,fireteamInfo,sizeof(before)));
    strcpy(arguments[20],"-1");strcpy(arguments[19],"-1");CG_ParseTCEFireteamInfo();
    snapshot.ps.weapon=0;snapshot.ps.stats[STAT_PLAYER_CLASS]=-1;render();CHECK(modelCount==0&&picCount==0); /* nil clears icons */
    strcpy(arguments[19],"43");strcpy(arguments[20],"4");CG_ParseTCEFireteamInfo();render();CHECK(modelCount==1&&picCount==1);
    iconsEnabled=0;render();CHECK(modelCount==0&&picCount==0);
    iconsEnabled=1;modelBoundsInvalid=1;render();CHECK(modelCount==0);modelBoundsInvalid=0;
    cg_weapons[43].weaponModel[W_PU_MODEL].model=0;cg_weapons[43].weaponModel[W_TP_MODEL].model=777;cg_weapons[43].weaponModel[W_TP_MODEL].skin[0]=2777;
    render();CHECK(modelCount==1&&modelEntities[0].hModel==777&&modelEntities[0].customSkin==2777); /* original item PU -> TP */
    cg_weapons[43].weaponModel[W_TP_MODEL].model=0;cg_weapons[43].weaponModel[W_FP_MODEL].model=888;render();CHECK(modelCount==0); /* never substitute arms */
    cg_weapons[43].weaponModel[W_PU_MODEL].model=1043;
    cg.time+=2501;render();CHECK(modelCount==0&&picCount==0);
    parseGood();CHECK(fireteamInfo[1].weapon==-1&&fireteamInfo[1].playerClass==-1);render();CHECK(modelCount==0&&picCount==0);
    packet("tceftinfo 0 2 0 100 1 120 220 30 0 1 75 2 200 300 40 0");CG_ParseTCEFireteamInfo();
    CHECK(fireteamInfo[1].weapon==-1&&fireteamInfo[1].playerClass==-1);render();CHECK(modelCount==0&&picCount==0);
    packet("tceftinfo 0 2 0 100 1 120 220 30 0 -1 -1 1 0 2 200 300 40 0 43 4");CG_ParseTCEFireteamInfo();render();CHECK(modelCount==0&&picCount==0);
    CG_TCEFireteamInvalidate();CHECK(!fireteamInfo[1].valid);
}
static void objectiveAndNames(void)
{
    int objective;char text[256],name[256];tce_fireteamInfo_t before[MAX_CLIENTS];
    setup();
    for(objective=0;objective<=4;++objective){
        sprintf(text,"tceftinfo 0 2 0 100 1 120 220 30 0 1 75 2 200 300 40 %i",objective);
        packet(text);CG_ParseTCEFireteamInfo();render();
        CHECK(fireteamInfo[1].objective==objective&&picCount==(objective?1:0));
    }
    memcpy(before,fireteamInfo,sizeof(before));strcpy(arguments[16],"5");CG_ParseTCEFireteamInfo();CHECK(!memcmp(before,fireteamInfo,sizeof(before)));
    strcpy(arguments[16],"-1");CG_ParseTCEFireteamInfo();CHECK(!memcmp(before,fireteamInfo,sizeof(before)));
    strcpy(arguments[16],"1");strcpy(arguments[11],"0");CG_ParseTCEFireteamInfo();CHECK(!memcmp(before,fireteamInfo,sizeof(before)));
    cg.time+=2501;render();CHECK(picCount==0);
    parseGood();CHECK(fireteamInfo[1].objective==0); /* Legacy packet clears carrier. */
    strcpy(name,"^1ABCDEFGHIJKL");CG_TCEFireteamName(name);CHECK(!strcmp(name,"ABCDEFGHIJ..."));
    strcpy(name,"^1ABCDEFGHIJ");CG_TCEFireteamName(name);CHECK(!strcmp(name,"ABCDEFGHIJ"));
    snapshot.ps.stats[STAT_TCE_WEAPON_FLAGS]=0x100;render();CHECK(picCount==1);
    CG_TCEFireteamInvalidate();snapshot.ps.stats[STAT_TCE_WEAPON_FLAGS]=0;render();CHECK(!picCount);
    cg_drawTeamNames.integer=1;cg.predictedPlayerState.persistant[PERS_TEAM]=TEAM_AXIS;
    cg.predictedPlayerState.clientNum=0;cg.tceNameTanHalfFov=1;
    VectorSet(cg.refdef.viewaxis[0],1,0,0);VectorSet(cg.refdef.viewaxis[1],0,1,0);VectorSet(cg.refdef.viewaxis[2],0,0,1);
    memset(cg_entities,0,sizeof(cg_entities));cg_entities[1].currentValid=qtrue;cg_entities[1].tceVisible=qtrue;cg_entities[1].currentState.eType=ET_PLAYER;
    VectorSet(cg_entities[1].lerpOrigin,200,0,0);strcpy(cgs.clientinfo[1].name,"^1Partner");
    traceBlocked=0;traceCalls=0;paintCount=0;rendered[0]=0;CG_DrawTeamNames();
    CHECK(paintCount==1&&traceCalls==1&&!strcmp(paints[0].text,"Partner"));
    CHECK(paints[0].color[1]>.7f&&paints[0].color[0]<.6f);
    traceBlocked=1;paintCount=0;CG_DrawTeamNames();CHECK(paintCount==0);
    traceBlocked=0;cg_entities[1].currentValid=qfalse;paintCount=0;CG_DrawTeamNames();CHECK(paintCount==0);
    cg_entities[1].currentValid=qtrue;cg_entities[1].tceVisible=qfalse;CG_DrawTeamNames();CHECK(paintCount==0);
    cg_entities[1].tceVisible=qtrue;cgs.clientinfo[1].fireteamData=NULL;CG_DrawTeamNames();CHECK(paintCount==0);
    cgs.clientinfo[1].fireteamData=&cg.fireTeams[0];cgs.clientinfo[1].team=TEAM_ALLIES;CG_DrawTeamNames();CHECK(paintCount==0);
    cgs.clientinfo[1].team=TEAM_AXIS;cg_entities[1].currentState.eFlags=EF_DEAD;CG_DrawTeamNames();CHECK(paintCount==0);
    cg_entities[1].currentState.eFlags=0;cg_drawTeamNames.integer=0;CG_DrawTeamNames();CHECK(paintCount==0);
}
static float screenY(float y)
{
    float x=0,w=0,h=1; qboolean before=tce_uiCoordinates;
    tce_uiCoordinates=qtrue;CG_AdjustFrom640(&x,&y,&w,&h);tce_uiCoordinates=before;return y;
}
static float sdkScreenY(float y)
{
    float x=0,w=0,h=1;qboolean before=tce_uiCoordinates;
    tce_uiCoordinates=qfalse;CG_AdjustFrom640(&x,&y,&w,&h);tce_uiCoordinates=before;return y;
}
static void textBelowFireteam(void)
{
    const int widths[]={1024,1280,1280},heights[]={768,720,800};
    const int counts[]={1,3,6},messages[]={0,1,5};
    int screen,aspect,n,c,respawn,i;
    vec4_t white={1,1,1,1};
    for(c=0;c<3;++c)for(screen=0;screen<3;++screen)for(aspect=0;aspect<3;++aspect)
    for(respawn=-1;respawn<=0;++respawn)for(n=0;n<3;++n){
        float bottom,start,offset;
        setup();
        for(i=0;i<6;++i){
            cgs.clientinfo[i].infoValid=i<counts[c];cgs.clientinfo[i].team=TEAM_AXIS;
            cgs.clientinfo[i].fireteamData=i<counts[c]?&cg.fireTeams[0]:NULL;
        }
        CG_SortClientFireteam();
        cgs.glconfig.vidWidth=widths[screen];cgs.glconfig.vidHeight=heights[screen];
        cgs.screenXScale=widths[screen]/640.f;cgs.screenYScale=heights[screen]/480.f;
        cg_aspectMode.integer=aspect;snapshot.ps.persistant[PERS_RESPAWNS_LEFT]=respawn;
        memset(popupItems,0,sizeof(popupItems));
        for(i=0;i<messages[n];++i){
            strcpy(popupItems[i].message,"Kill / Death notification");popupItems[i].time=cg.time;
            popupItems[i].next=i+1<messages[n]?&popupItems[i+1]:NULL;
        }
        cg_pmWaitingList=messages[n]?popupItems:NULL;cg_pmOldList=messages[n]>1?&popupItems[1]:NULL;
        CG_DrawPMItems();CHECK(!tce_uiCoordinates);CHECK(picCount==messages[n]);
        bottom=sdkScreenY(8+CG_TCEFireteamHeight()+8);
        start=screenY(CG_TCEFireteamTextY(-480));CHECK(start>=bottom-.01f);
        for(i=0;i<picCount;++i){
            CHECK(screenY(pictures[i].y)>=bottom-.01f);
            if(i)CHECK(fabsf(pictures[i].y-pictures[i-1].y-14)<.001f);
            CHECK(fabsf(paints[i].y-pictures[i].y-12)<.001f);
        }
        offset=CG_TCEFireteamStatusOffset();
        CG_FireteamMessagePaint(8,118,.25f,.25f,white,"Following",0,0,0,NULL);
        CHECK(screenY(lastTextY-14)>=bottom-.01f);
        if(picCount)CHECK(lastTextY-14>=CG_PopupMessageBottom()+5.9f);
        CG_FireteamMessageSmall(8,118,"Waiting",white);CHECK(lastTextY==118+ceilf(offset));
        CG_MessagePaint(8,118,.2f,.2f,white,"Unrelated objective",0,0,0,NULL);CHECK(lastTextY==118);
        cg.showFireteamMenu=qtrue;CHECK(CG_TCEFireteamHeight()==0&&CG_TCEFireteamStatusOffset()==0);
        picCount=paintCount=0;rendered[0]=0;CG_DrawPMItems();
        if(picCount)CHECK(fabsf(pictures[0].y-((aspect==1?-20.67f:aspect==2?-74.f:6.f)+4-(respawn>=0?20:0)))<.001f);
        CHECK(CG_TCEFireteamTextY(10)==10); /* no reservation while admin is open */
    }
}
static void modernTextBelowFireteam(void)
{
    const int widths[]={1024,1280,1280,3440,3840},heights[]={768,720,800,1440,2160};
    const float scales[]={.5f,.85f,1.f,1.5f};
    const int counts[]={1,3,6};int screen,scale,n,c,i;
    vec4_t white={1,1,1,1};
    for(c=0;c<3;++c)for(screen=0;screen<5;++screen)for(scale=0;scale<4;++scale)for(n=0;n<3;++n){
        float minimum,physical,baseline;
        setup();for(i=0;i<6;++i){cgs.clientinfo[i].infoValid=i<counts[c];cgs.clientinfo[i].team=TEAM_AXIS;cgs.clientinfo[i].fireteamData=i<counts[c]?&cg.fireTeams[0]:NULL;}
        CG_SortClientFireteam();cgs.glconfig.vidWidth=widths[screen];cgs.glconfig.vidHeight=heights[screen];
        cgs.screenXScale=widths[screen]/640.f;cgs.screenYScale=heights[screen]/480.f;
        modernHud=1;hudUserScale=scales[scale];CG_HudViewportUpdate();CG_HudViewportBegin(0,0);
        minimum=sdkScreenY(8+CG_TCEFireteamHeight()+8);physical=screenY(CG_TCEFireteamTextY(-480));
        CHECK(fabsf(physical-minimum)<.02f);CHECK(!tce_uiCoordinates);
        /* All real popup records use the TC canvas, while FT uses SDK640.
         * Verify their physical top instead of assuming equal native scales. */
        memset(popupItems,0,sizeof(popupItems));
        for(i=0;i<n+1;++i){strcpy(popupItems[i].message,"Kill / Death notification");popupItems[i].time=cg.time;popupItems[i].next=i<n?&popupItems[i+1]:NULL;}
        cg_pmWaitingList=popupItems;cg_pmOldList=n?&popupItems[1]:NULL;CG_DrawPMItems();
        CHECK(picCount==n+1);
        for(i=0;i<picCount;++i){CHECK(screenY(pictures[i].y)>=minimum-.02f);if(i)CHECK(pictures[i].y>=pictures[i-1].y+13.99f);}
        CG_FireteamMessagePaint(8,118,.25f,.25f,white,"Following",0,0,0,NULL);
        baseline=screenY(lastTextY-14);CHECK(baseline>=minimum-.02f);
        CHECK(lastTextY-14>=CG_PopupMessageBottom()+5.99f);
        cg.showFireteamMenu=qtrue;CHECK(CG_TCEFireteamTextY(10)==10);cg.showFireteamMenu=qfalse;
        CG_HudViewportEnd();CHECK(!tce_uiCoordinates);
    }
}
int main(void)
{
    protocolAndPresentation();sorting();fullFireteam();membershipAndSlotReuse();following();hudIntegration();compactLayout();textBelowFireteam();modernTextBelowFireteam();gearAndMeasuredLayout();objectiveAndNames();
    printf("Fireteam client: %i checks, %i failures\n",checks,failures);return failures?1:0;
}
