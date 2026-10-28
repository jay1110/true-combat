#define CGAMEDLL
#include "src/cgame/cg_local.h"
#include "src/cgame/tce_weapon_media.h"
#include "src/cgame/cg_tce_weapon_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools/modernize/stats_courbd30_metrics.h"
#undef vsnprintf

cg_t cg;
cgs_t cgs;
displayContextDef_t cgDC;
vmCvar_t cf_wstats;
vmCvar_t cg_showmiss,cg_autoReload;
qboolean tce_scoreExtended[MAX_CLIENTS];
int numSplinePaths,numPathCorners;
char cg_fxflags;
tce_weaponInfo_t tce_cg_weapons[64];

static int checks, failures, unrelatedCalls, requestCount, warnings, now = 1000;
static int registeredCount, drawCount, rectCount, wordCount;
static struct { float x,y,scale,left,right,top,bottom; char text[128]; } painted[512];
static int paintCount;
static struct { float x,y,w,h; } glyphs[16384];
static int glyphCount;
static float inkAlpha;
static void CG_Actual_Text_Paint_Ext(float,float,float,float,vec4_t,const char*,float,int,int,fontInfo_t*);
static char registered[256][MAX_QPATH], request[128], commandName[128];
static char words[64][128], drawn[16384];
static char playerConfig[MAX_CLIENTS][MAX_INFO_STRING];
static snapshot_t snapshot;
/* Map-restart services are unrelated engine/asset boundaries. The actual
 * CG_MapRestart body and stats cleanup/request functions execute below. */
void CG_TCEPresentationRestart(void) {}
void CG_TCEFireteamInvalidate(void) {}
void CG_TCEHitDebugReset(void) {}
void CG_LimboPanel_RequestObjective(void) {}
void CG_ParseWolfinfo(void) {}
void CG_ParseEntitiesFromString(void) {}
void CG_LoadObjectiveData(void) {}
void CG_InitLocalEntities(void) {}
void CG_InitMarkPolys(void) {}
void CG_InitPM(void) {}
void BG_BuildSplinePaths(void) {}
void InitSmokeSprites(void) {}
void CG_ClearParticles(void) {}
void CG_ClearFlameChunks(void) {}
void CG_SoundInit(void) {}
void CG_StartMusic(void) {}
void CG_ChargeTimesChanged(void) {}
void CG_ParseFireteams(void) {}
void CG_ParseOIDInfos(void) {}
void CG_ParseSpawns(void) {}
void CG_ParseTagConnects(void) {}
void CG_SetupEliteLighting(void) {}
void trap_R_SetFog(int a,int b,int c,float d,float e,float f,float g) {(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;}
void trap_Cvar_Set(const char *name,const char *value) {(void)name;(void)value;}
void trap_S_ClearLoopingSounds(void) {}
void trap_S_ClearSounds(qboolean clear) {(void)clear;}
void trap_R_ClearDecals(void) {}
const char *CG_ConfigString(int index)
{return index>=CS_PLAYERS&&index<CS_PLAYERS+MAX_CLIENTS?playerConfig[index-CS_PLAYERS]:"0";}

#define CHECK(c) do { ++checks; if (!(c)) { ++failures; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void appendDraw(const char *text, float alpha)
{
    if (alpha > 0 && strlen(drawn) + strlen(text) + 2 < sizeof(drawn))
    {
        strcat(drawn, text); strcat(drawn, "\n"); ++drawCount;
    }
}
void trap_AddCommand(const char *name)
{
    if (registeredCount >= 256) abort();
    Q_strncpyz(registered[registeredCount++], name, MAX_QPATH);
}
void trap_SendClientCommand(const char *text)
{
    ++requestCount; Q_strncpyz(request, text, sizeof(request));
}
int trap_Milliseconds(void) { return now; }
void CG_Printf(const char *format, ...) { (void)format; ++warnings; }
const char *CG_LocalizeServerCommand(const char *text) { return text; }
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_Error(int level, const char *format, ...) { (void)level; (void)format; abort(); }
int Q_vsnprintf(char *dest, int size, const char *format, va_list args)
{ return vsnprintf(dest, (size_t)size, format, args); }
const char *CG_Argv(int index) { return index == 0 ? commandName : words[index]; }
int trap_Argc(void) { return wordCount; }
void CG_demoAviFPSDraw(void) {}
void CG_demoTimescaleDraw(void) {}
void CG_cursorUpdate(void) {}
void CG_mvDraw(cg_window_t *window) { (void)window; }
void CG_mvOverlayDisplay(void) {}
void CG_DrawPic(float x, float y, float w, float h, qhandle_t shader)
{ (void)x; (void)y; (void)w; (void)h; (void)shader; }
void CG_FillRect(float x, float y, float w, float h, const float *color)
{ (void)x; (void)y; (void)w; (void)h; if (color[3] > 0) ++rectCount; }
void CG_DrawRect(float x, float y, float w, float h, float size, const float *color)
{ (void)x; (void)y; (void)w; (void)h; (void)size; (void)color; }
void CG_AdjustFrom640(float *x,float *y,float *w,float *h) { (void)x;(void)y;(void)w;(void)h; }
void trap_R_SetColor(const float *color) { inkAlpha=color?color[3]:1; }
void trap_R_DrawStretchPic(float x,float y,float w,float h,float s,float t,float s2,float t2,qhandle_t shader)
{
    (void)s;(void)t;(void)s2;(void)t2;(void)shader;
    if(inkAlpha>0&&w>0&&h>0){CHECK(glyphCount<16384);if(glyphCount<16384){glyphs[glyphCount].x=x;glyphs[glyphCount].y=y;glyphs[glyphCount].w=w;glyphs[glyphCount++].h=h;}}
}
void CG_Text_Paint_Ext(float x, float y, float sx, float sy, vec4_t color,
                       const char *text, float adjust, int limit, int style, fontInfo_t *font)
{
    int first=glyphCount,i;
    CG_Actual_Text_Paint_Ext(x,y,sx,sy,color,text,adjust,limit,style,font);
    if(color[3]>0&&paintCount<512){
        painted[paintCount].x=x;painted[paintCount].y=y;painted[paintCount].scale=sx;
        painted[paintCount].left=painted[paintCount].right=x;painted[paintCount].top=painted[paintCount].bottom=y;
        for(i=first;i<glyphCount;++i){
            if(glyphs[i].x<painted[paintCount].left)painted[paintCount].left=glyphs[i].x;
            if(glyphs[i].x+glyphs[i].w>painted[paintCount].right)painted[paintCount].right=glyphs[i].x+glyphs[i].w;
            if(glyphs[i].y<painted[paintCount].top)painted[paintCount].top=glyphs[i].y;
            if(glyphs[i].y+glyphs[i].h>painted[paintCount].bottom)painted[paintCount].bottom=glyphs[i].y+glyphs[i].h;
        }
        Q_strncpyz(painted[paintCount++].text,text,128);
    }
    appendDraw(text,color[3]);
}
void CG_DrawStringExt2(int x, int y, const char *text, const float *color,
                       qboolean force, qboolean shadow, int w, int h, int max)
{ (void)x; (void)y; (void)force; (void)shadow; (void)w; (void)h; (void)max; appendDraw(text, color[3]); }
int CG_DrawStrlen(const char *text) { return (int)strlen(text); }

typedef struct { char *cmd; void (*function)(void); } consoleCommand_t;
/* UNRELATED_COMMAND_STUBS */
/* PRODUCTION_FUNCTIONS */

static int registrations(const char *name)
{
    int i, total = 0;
    for (i = 0; i < registeredCount; ++i) if (!strcmp(name, registered[i])) ++total;
    return total;
}
static qboolean dispatch(const char *name)
{
    Q_strncpyz(commandName, name, sizeof(commandName)); return CG_ConsoleCommand();
}
static void clearDraw(void) { drawCount = rectCount = paintCount = glyphCount = 0; drawn[0] = 0; }
static void reset(void)
{
    memset(&cg, 0, sizeof(cg)); memset(&cgs, 0, sizeof(cgs));
    memset(&snapshot, 0, sizeof(snapshot)); memset(words, 0, sizeof(words));
    memset(playerConfig, 0, sizeof(playerConfig));
    cg.snap = &snapshot; cg.time = now = 1000; cf_wstats.value = 1;
    statsActualFont(&cgs.media.limboFont2);
    CG_TCEWeaponStatsReset();wordCount=0;
    cg.snap->ps.clientNum = 7; cg.snap->ps.persistant[PERS_TEAM] = TEAM_AXIS;
    Q_strncpyz(cgs.clientinfo[7].name, "PopupPlayer", sizeof(cgs.clientinfo[7].name));
    cgs.clientinfo[7].team = TEAM_AXIS;
    strcpy(playerConfig[7],"n\\PopupPlayer\\t\\1");
    CG_windowInit(); CG_initStrings(); requestCount = warnings = 0; clearDraw();
}
static void reply(void)
{
    /* Real existing wws layout: client, rounds, mask; five weapon counters;
     * damage given/received/team, then skill mask. WS_LUGER has headshots. */
    int values[] = { 7, 2, 1 << WS_LUGER, 6, 10, 3, 4, 2, 123, 45, 6, 0 };
    unsigned i;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) sprintf(words[i + 1], "%d", values[i]);
    CG_wstatsParse_cmd();
}
static int requestFor(int client)
{
    int target,sequence;return sscanf(request,"wstats %d tce1 %d",&target,&sequence)==2&&target==client&&sequence>0;
}
static void tceReply(int offset,int total)
{
    int client,sequence,i,n=total-offset<8?total-offset:8,at=1;
    sscanf(request,"wstats %d tce1 %d",&client,&sequence);
    sprintf(words[at++],"%d",sequence);sprintf(words[at++],"%d",client);
    sprintf(words[at++],"%d",offset);sprintf(words[at++],"%d",total);
    strcpy(words[at++],"2");strcpy(words[at++],"123");strcpy(words[at++],"45");strcpy(words[at++],"6");
    for(i=0;i<n;++i){sprintf(words[at++],"%d",offset+i+1);strcpy(words[at++],"10");strcpy(words[at++],"6");strcpy(words[at++],"3");strcpy(words[at++],"4");strcpy(words[at++],"2");}
    wordCount=at;
}
static void tceStats(void)
{
    int kind,i,j;float right[6]={0};int header[6];
    const char *columns[]={"Weapon","Acc %","Hits/Att.","Kills","Deaths","HS"};
    reset();CHECK(dispatch("+wstats"));CHECK(requestFor(7));
    strcpy(tce_cg_weapons[1].deployMenuShortName,"Wide WWW");strcpy(tce_cg_weapons[2].deployMenuShortName,"Beretta 92");
    tceReply(0,9);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow==NULL);
    tceReply(8,9);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow!=NULL);
    CHECK(cg.statsWindow->fontScaleX==.14f);now+=111;clearDraw();CG_windowDraw();
    CHECK(strstr(drawn,"Beretta 92")&&strstr(drawn,"Wide WWW"));CHECK(!strstr(drawn,"Luger")&&!strstr(drawn,"MP-40"));
    CHECK(strstr(drawn,"6/10")!=NULL&&!strstr(drawn,"^3Wide WWW"));
    CHECK(cg.statsWindow->colorBackground[0]==.035f&&cg.statsWindow->colorBorder[0]==.44f);
    for(i=0;i<6;++i){header[i]=-1;for(j=0;j<paintCount;++j)if(!strcmp(painted[j].text,columns[i])){header[i]=j;right[i]=painted[j].right;break;}CHECK(header[i]>=0);}
    for(i=1;i<6;++i)CHECK(painted[header[i]].x>=right[i-1]+7);
    for(i=0;i<paintCount;++i)if(!strcmp(painted[i].text,"6/10"))CHECK(fabsf(painted[i].right-right[2])<1.01f);
    CHECK(cg.statsWindow->w<640&&cg.statsWindow->x+cg.statsWindow->w<=640);
    CHECK(cg.statsWindow->y>=0&&cg.statsWindow->y+cg.statsWindow->h<=480);
    /* New malformed/stale/partial packets must never replace the last complete
     * window. Every case begins with a real new pending request. */
    for(kind=0;kind<9;++kind){cg_window_t *before=cg.statsWindow;int lines=before->lineCount;
        cg.time+=501;CHECK(dispatch("+wstats"));tceReply(0,kind<2?1:9);
        if(kind==0)strcpy(words[1],"0");else if(kind==1)strcpy(words[2],"8");
        else if(kind==2)strcpy(words[3],"8");else if(kind==3)strcpy(words[4],"64");
        else if(kind==4)strcpy(words[9],"0");else if(kind==5)strcpy(words[15],words[9]);
        else if(kind==6)strcpy(words[10],"2147483648");else if(kind==7)--wordCount;
        else strcpy(words[14],"-1");
        CG_TCEWeaponStatsParse();CHECK(cg.statsWindow==before&&before->lineCount==lines);
    }
    i=cg.statsWindow->lineCount;
    cg.time+=501;CHECK(dispatch("+wstats"));tceReply(0,10);CG_TCEWeaponStatsParse();
    cg.time+=2001;tceReply(8,10);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow->lineCount==i);
    CHECK(dispatch("-wstats"));tceReply(0,0);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow==NULL);
    cg.time+=501;CHECK(dispatch("+wstats"));tceReply(0,0);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow!=NULL);
    now+=111;clearDraw();CG_windowDraw();CHECK(strstr(drawn,"No weapon info available."));
    CG_windowDraw();CHECK(cg.winHandler.numActiveWindows==1); /* finish the previous popup's fade cleanup */
    /* Repeated long/short replacement may not leak the shared 80-string pool. */
    for(kind=0;kind<12;++kind){int active=0;
        cg.time+=501;CHECK(dispatch("+wstats"));tceReply(0,kind&1?0:9);CG_TCEWeaponStatsParse();
        if(!(kind&1)){tceReply(8,9);CG_TCEWeaponStatsParse();}
        for(i=0;i<MAX_STRINGS;++i)active+=cg.aStringPool[i].fActive!=0;
        if(active!=cg.statsWindow->lineCount)printf("pool active%d statsLines%d windows%d\n",active,cg.statsWindow->lineCount,cg.winHandler.numActiveWindows);
        CHECK(active==cg.statsWindow->lineCount);CHECK(cg.statsWindow->lineCount>3);
    }
    CHECK(dispatch("-wstats"));now+=112;CG_windowDraw();CG_windowDraw();
    for(i=0;i<MAX_STRINGS;++i)CHECK(!cg.aStringPool[i].fActive);
}
static void statsRestart(void)
{
    int i,before;
    reset();CHECK(dispatch("+wstats"));tceReply(0,1);CG_TCEWeaponStatsParse();
    CHECK(cg.showStats&&cg.statsWindow!=NULL);CG_MapRestart();
    CHECK(!cg.showStats&&cg.statsWindow==NULL&&cg.statsRequestTime==0);
    now+=112;CG_windowDraw();CG_windowDraw();
    for(i=0;i<MAX_STRINGS;++i)CHECK(!cg.aStringPool[i].fActive);
    /* A partially received old transaction cannot reopen after restart. */
    CHECK(dispatch("+wstats"));tceReply(0,9);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow==NULL);
    CG_MapRestart();tceReply(8,9);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow==NULL&&!cg.showStats);
    before=requestCount;CHECK(dispatch("+wstats"));CHECK(requestCount==before+1&&requestFor(7));
    tceReply(0,0);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow!=NULL&&cg.showStats);
}
static void statsQuickReopen(void)
{
    int before;char oldSequence[64];
    reset();CHECK(dispatch("+wstats"));tceReply(0,1);CG_TCEWeaponStatsParse();
    CHECK(dispatch("-wstats"));cg.time=1100;before=requestCount;
    CHECK(dispatch("+wstats"));CHECK(requestCount==before&&cg.statsWindow!=NULL); /* committed cache */
    cg.time=1500;CG_windowDraw();CHECK(requestCount==before);
    cg.time=1501;CG_windowDraw();CHECK(requestCount==before+1); /* waits for the real server-rate interval */
    tceReply(0,1);strcpy(words[11],"8");CG_TCEWeaponStatsParse();
    now+=112;clearDraw();CG_windowDraw();CHECK(strstr(drawn,"80.0")!=NULL); /* New transaction accuracy. */

    reset();CHECK(dispatch("+wstats"));tceReply(0,1);strcpy(oldSequence,words[1]);
    CHECK(dispatch("-wstats"));cg.time=1100;CHECK(dispatch("+wstats"));
    CHECK(requestCount==1&&cg.statsWindow!=NULL&&cg.statsWindow->lineCount==1);
    CG_TCEWeaponStatsParse();CHECK(cg.statsWindow->lineCount==1); /* canceled first nonce */
    now+=112;clearDraw();CG_windowDraw();CHECK(strstr(drawn,"Loading weapon stats..."));
    cg.time=1500;CG_windowDraw();CHECK(requestCount==1);
    cg.time=1501;CG_windowDraw();CHECK(requestCount==2);
    tceReply(0,1);strcpy(words[1],oldSequence);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow->lineCount==1);
    tceReply(0,1);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow->lineCount>1);

    reset();CHECK(dispatch("+wstats"));CHECK(dispatch("-wstats"));cg.time=1100;
    CHECK(dispatch("+wstats"));CHECK(dispatch("-wstats"));cg.time=2000;CG_windowDraw();
    CHECK(requestCount==1&&!cg.showStats&&cg.statsWindow==NULL); /* release cancels the queued transport */
    reset();CHECK(dispatch("+wstats"));cg.time=3001;CG_windowDraw();CHECK(requestCount==2);
    tceReply(0,0);CG_TCEWeaponStatsParse();CHECK(cg.statsWindow!=NULL); /* lost/rate-dropped reply retries */
}
static void statsCacheIdentity(void)
{
    int kind;
    /* A followed slot is reused before the 500ms server limit. Also change a
     * config key while preserving name/team: cache identity is the entire CS. */
    for(kind=0;kind<2;++kind){
        reset();cg.snap->ps.pm_flags|=PMF_FOLLOW;cg.snap->ps.clientNum=1;
        strcpy(cgs.clientinfo[1].name,"Player A");strcpy(playerConfig[1],"n\\Player A\\t\\1\\x\\1");
        CHECK(dispatch("+wstats"));CHECK(requestFor(1));tceReply(0,1);CG_TCEWeaponStatsParse();
        CHECK(cg.statsWindow!=NULL&&cg.statsWindow->lineCount>1);
        CHECK(dispatch("-wstats"));cg.time=1100;now+=112;CG_windowDraw();CG_windowDraw();
        if(!kind){strcpy(cgs.clientinfo[1].name,"Player B");strcpy(playerConfig[1],"n\\Player B\\t\\1\\x\\1");}
        else strcpy(playerConfig[1],"n\\Player A\\t\\1\\x\\2");
        CHECK(dispatch("+wstats"));CHECK(requestCount==1&&cg.statsWindow!=NULL);
        CHECK(cg.statsWindow->lineCount==1);
        now+=112;clearDraw();CG_windowDraw();CHECK(strstr(drawn,"Loading weapon stats..."));
        cg.time=1500;CG_windowDraw();CHECK(requestCount==1);
        cg.time=1501;CG_windowDraw();CHECK(requestCount==2&&requestFor(1));
        tceReply(0,1);strcpy(words[11],"8");CG_TCEWeaponStatsParse();
        CHECK(cg.statsWindow->lineCount>1);now+=112;clearDraw();CG_windowDraw();
        CHECK(strstr(drawn,kind?"Player A":"Player B")!=NULL);CHECK(strstr(drawn,"80.0")!=NULL);
    }
}

/* The real painter submits glyph images above/below a baseline. Assert those
 * image rectangles, not a second implementation of the layout calculation. */
static void statsGeometry(void)
{
    const float scales[]={.5f,1.f,1.2f,2.f,8.f,-1.f};
    int mode,scale,offset,i,j,summary;
    for(mode=0;mode<2;++mode)for(scale=0;scale<6;++scale){
        reset();cf_wstats.value=scales[scale];
        cg.snap->ps.pm_type=mode?PM_INTERMISSION:PM_NORMAL;
        memset(cgs.clientinfo[7].name,'W',sizeof(cgs.clientinfo[7].name)-1);
        for(i=1;i<64;++i){memset(tce_cg_weapons[i].deployMenuShortName,'W',sizeof(tce_cg_weapons[i].deployMenuShortName)-1);tce_cg_weapons[i].deployMenuShortName[sizeof(tce_cg_weapons[i].deployMenuShortName)-1]=0;}
        CHECK(dispatch("+wstats"));
        for(offset=0;offset<63;offset+=8){
            tceReply(offset,63);
            for(i=5;i<wordCount;++i)if(i<9||(i-9)%6)strcpy(words[i],"2147483647");
            CG_TCEWeaponStatsParse();
        }
        now+=111;clearDraw();CG_windowDraw();CHECK(cg.statsWindow!=NULL);
        CHECK(cg.statsWindow->x>=0&&cg.statsWindow->y>=0);
        CHECK(cg.statsWindow->x+cg.statsWindow->w<=640&&cg.statsWindow->y+cg.statsWindow->h<=480);
        summary=0;
        for(i=0;i<paintCount;++i){
            CHECK(painted[i].left>=cg.statsWindow->x+1.f);
            CHECK(painted[i].right<=cg.statsWindow->x+cg.statsWindow->w-1.f);
            CHECK(painted[i].top>=cg.statsWindow->y+1.f);
            CHECK(painted[i].bottom<=cg.statsWindow->y+cg.statsWindow->h-1.f);
            if(strstr(painted[i].text,"Damage: Given ")){++summary;CHECK(strstr(painted[i].text," | Received ")&&strstr(painted[i].text," | Team "));}
            for(j=0;j<i;++j)if(fabsf(painted[i].y-painted[j].y)>.1f)
                CHECK(painted[i].bottom<=painted[j].top-1.f||painted[j].bottom<=painted[i].top-1.f);
        }
        CHECK(summary==1);CHECK(strstr(drawn,"Showing 40 of 63 weapons."));
        /* A subsequent short reply must restore the requested compact size
         * and bottom anchor, rather than cumulatively scaling/moving it. */
        cg.time+=501;CHECK(dispatch("+wstats"));tceReply(0,0);CG_TCEWeaponStatsParse();
        now+=111;clearDraw();CG_windowDraw();
        CHECK(cg.statsWindow->y>=0&&cg.statsWindow->y+cg.statsWindow->h<=480);
        CHECK(painted[0].top>=cg.statsWindow->y+1.f);
        cg.time+=501;Q_strncpyz(cgs.clientinfo[7].name,"^1W\"'_^7gj",sizeof(cgs.clientinfo[7].name));
        Q_strncpyz(tce_cg_weapons[1].deployMenuShortName,"gj_\"'",sizeof(tce_cg_weapons[1].deployMenuShortName));
        CHECK(dispatch("+wstats"));tceReply(0,1);CG_TCEWeaponStatsParse();
        now+=111;clearDraw();CG_windowDraw();
        CHECK(strstr(drawn,"Weapon stats: W\"'_gj")!=NULL);
        for(i=0;i<paintCount;++i){CHECK(painted[i].top>=cg.statsWindow->y+1.f);CHECK(painted[i].bottom<=cg.statsWindow->y+cg.statsWindow->h-1.f);}
    }
}
static char fullDump[16384];
static void captureFullStats(char *text) { Q_strcat(fullDump,sizeof(fullDump),text); }
static void statsConsoleCompatibility(void)
{
    reset();CHECK(dispatch("+wstats"));reply();fullDump[0]=0;
    CG_parseWeaponStats_cmd(captureFullStats);
    CHECK(strstr(fullDump,"Damage Given: ^7123")!=NULL);
    CHECK(strstr(fullDump,"Team Damage: ^76")!=NULL);
    CHECK(strstr(fullDump,"Damage Recvd: ^745")!=NULL);
    CHECK(strstr(fullDump,"Damage: Given ")==NULL);
}
int main(void)
{
    cg_window_t *window;
    int i, before;
    CG_InitConsoleCommands();
    CHECK(registrations("+wstats") == 1); CHECK(registrations("-wstats") == 1);
    CHECK(registrations("+stats") == 1); CHECK(registrations("-stats") == 1);
#ifdef FEATURE_NAVSTUDIO
    CHECK(registrations("wp_add") == 1); CHECK(registrations("listbotgoals") == 1);
#else
    for (i = 0; i < registeredCount; ++i) CHECK(strncmp(registered[i], "wp_", 3) != 0);
    CHECK(registrations("listbotgoals") == 0);
    CHECK(registrations("setGoalName") == 0);
#endif
    reset(); cg.snap = NULL;
    CHECK(!dispatch("+wstats")); CHECK(!cg.showStats && requestCount == 0);
    reset();
    /* Existing aliases retain their historical no-op semantics. */
    CHECK(dispatch("+stats")); CHECK(!cg.showStats && requestCount == 0);
    CHECK(dispatch("-stats")); CHECK(!cg.showStats && requestCount == 0);
    if (!registrations("+wstats"))
    {
        CHECK(dispatch("+wstats")); CHECK(cg.showStats); CHECK(requestCount == 1);
        printf("stats popup: %d checks, %d failures (missing production registration)\n", checks, failures);
        return 1;
    }
    CHECK(dispatch("+wstats")); CHECK(cg.showStats); CHECK(requestCount == 1);
    CHECK(requestFor(7)); CHECK(cg.statsRequestTime == 1500);
    CHECK(cg.statsWindow == NULL);
    reply(); window = cg.statsWindow;
    CHECK(window != NULL && window->id == WID_STATS && window->inuse);
    CHECK(window && window->lineCount > 4);
    now += 111; clearDraw(); CG_windowDraw();
    CHECK(drawCount > 4 && rectCount > 0);
    CHECK(strstr(drawn, "PopupPlayer") != NULL);
    CHECK(strstr(drawn, "Acrcy Hits/Atts Kll Dth HS") != NULL);
    CHECK(strstr(drawn, "60.0") != NULL && strstr(drawn, "6/10") != NULL);
    CHECK(strstr(drawn, "^2  3 ^1  4 ^3 2") != NULL);
    CHECK(strstr(drawn, "Damage: Given 123 | Received 45 | Team 6") != NULL);
    CHECK(window->w > 10 && window->h > 0);
    CHECK(window->x == 4 && window->y >= 0 && window->y + window->h <= 480);
    before = requestCount; cg.time = 1500;
    CHECK(dispatch("+wstats")); CHECK(requestCount == before);
    cg.time = 1501; CHECK(dispatch("+wstats")); CHECK(requestCount == before + 1);
    reply(); CHECK(cg.statsWindow == window && cg.winHandler.numActiveWindows == 1);
    CHECK(dispatch("-wstats")); CHECK(!cg.showStats && cg.statsWindow == NULL);
    CHECK(window->state == WSTATE_SHUTDOWN);
    reply(); CHECK(cg.statsWindow == NULL && !cg.showStats);
    now += 111; clearDraw(); CG_windowDraw();
    now += 1; clearDraw(); CG_windowDraw();
    CHECK(drawCount == 0 && cg.winHandler.numActiveWindows == 0);
    CHECK(dispatch("-wstats")); CHECK(cg.winHandler.numActiveWindows == 0);
    CHECK(dispatch("+wstats")); reply(); CHECK(cg.statsWindow != NULL);
    reset(); cg.snap->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
    CHECK(dispatch("+wstats")); CHECK(!cg.showStats && requestCount == 0 && warnings == 1);
    reset(); cg.snap->ps.pm_flags |= PMF_FOLLOW;
    cg.snap->ps.clientNum = 12;
    CHECK(dispatch("+wstats")); CHECK(requestFor(12));
    reset(); cg.mvTotalClients = 1; cg.mvCurrentActive = &cg.winHandler.window[0];
    cg.mvCurrentActive->mvInfo = 23;
    CHECK(dispatch("+wstats")); CHECK(requestFor(23));
    reset(); cg.winHandler.numActiveWindows = MAX_WINDOW_COUNT;
    CHECK(dispatch("+wstats")); reply(); CHECK(cg.statsWindow == NULL && !cg.showStats);
    CHECK(dispatch("-wstats"));
    CHECK(!dispatch("no_such_popup_command"));
    tceStats();
    statsRestart();
    statsQuickReopen();
    statsCacheIdentity();
    statsGeometry();
    statsConsoleCompatibility();
    printf("stats popup: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
