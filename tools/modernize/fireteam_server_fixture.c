#define GAMEDLL
#define FEATURE_OMNIBOT
#define FEATURE_LUA
#include "src/game/g_local.h"
#include "src/game/tce_lua.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf
gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
static gclient_t clients[MAX_CLIENTS];
static int checks,failures,sends,to[64];
static char commands[64][800],userinfo[MAX_CLIENTS][64];
static char fireteamConfig[MAX_FIRETEAMS][128];
const char *bg_fireteamNames[MAX_FIRETEAMS/2]={"Alpha","Bravo","Charlie","Delta","Echo","Foxtrot"};
vmCvar_t g_autoFireteams,g_gamestate,g_dedicated,g_voiceChatsAllowed;
static int commandArgc,luaConsumes,created,joined,left,invited,voiceRecords,scoreCalls,voteCalls;
static char commandArgs[12][128];
#define CHECK(c) do { ++checks;if(!(c)){++failures;printf("FAIL %i: %s\n",__LINE__,#c);} } while(0)
void QDECL Com_Printf(const char *format,...) { (void)format; }
void QDECL Com_Error(int code,const char *format,...) { (void)code;(void)format;abort(); }
void QDECL G_Error(const char *format,...) { (void)format;abort(); }
void QDECL G_Printf(const char *format,...) { (void)format; }
void QDECL G_LogPrintf(const char *format,...) { (void)format; }
int Q_vsnprintf(char *dest,int size,const char *format,va_list args) { return vsnprintf(dest,(size_t)size,format,args); }
void trap_GetUserinfo(int id,char *buffer,int size) { CHECK(id>=0&&id<MAX_CLIENTS);Q_strncpyz(buffer,userinfo[id],size); }
void trap_SendServerCommand(int id,const char *text)
{ CHECK(sends<64);to[sends]=id;Q_strncpyz(commands[sends++],text,800); }
void trap_SetConfigstring(int index,const char *text)
{
    CHECK(index>=CS_FIRETEAMS&&index<CS_FIRETEAMS+MAX_FIRETEAMS);
    if(index>=CS_FIRETEAMS&&index<CS_FIRETEAMS+MAX_FIRETEAMS)Q_strncpyz(fireteamConfig[index-CS_FIRETEAMS],text,128);
}
gentity_t *Team_GetLocation(gentity_t *player) { return &g_entities[100+(player-g_entities)]; }
qboolean Team_GetLocationMsg(gentity_t *player,char *text,int size) { (void)player;(void)text;(void)size;return qfalse; }
qboolean OnSameTeam(gentity_t *a,gentity_t *b) { return a->client->sess.sessionTeam==b->client->sess.sessionTeam; }
int BG_cleanName(const char *in,char *out,unsigned int size,qboolean colors) { (void)colors;Q_strncpyz(out,in,(int)size);return (int)strlen(out); }
char *BG_GetLocationString(vec2_t point) { (void)point;return "B3"; }
int trap_Argc(void) { return commandArgc; }
void trap_Argv(int arg,char *text,int size) { Q_strncpyz(text,arg>=0&&arg<commandArgc?commandArgs[arg]:"",size); }
qboolean TCE_LuaCommand(int id,qboolean console) { (void)id;(void)console;return luaConsumes; }
const char *TCE_LuaChat(int sender,int target,const char *text,char *buffer,int size)
{ (void)sender;(void)target;(void)buffer;(void)size;return text; }
void Bot_Event_FireTeamCreated(int id,int ident) { CHECK(id>=0&&ident>0);++created; }
void Bot_Event_JoinedFireTeam(int id,gentity_t *leader) { CHECK(id>=0&&leader!=NULL);++joined; }
void Bot_Event_LeftFireTeam(int id) { CHECK(id>=0);++left; }
void Bot_Event_InviteFireTeam(int id,int other) { CHECK(id>=0&&other>=0);++invited; }
void BotRecordVoiceChat(int id,int target,const char *text,int mode,qboolean noresponse)
{ (void)id;(void)target;(void)text;(void)mode;(void)noresponse;++voiceRecords; }
void Cmd_Score_f(gentity_t *ent) { (void)ent;++scoreCalls; }
void Cmd_Vote_f(gentity_t *ent) { (void)ent;++voteCalls; }
/* ACTUAL_SERVER_FUNCTION */
#define MAX_SAY_TEXT 150
/* ACTUAL_COMMAND_FUNCTIONS */
static void setup(void)
{
    int i;
    memset(g_entities,0,sizeof(g_entities));memset(clients,0,sizeof(clients));memset(&level,0,sizeof(level));
    memset(userinfo,0,sizeof(userinfo));sends=0;level.maxclients=64;
    level.clients=clients;level.time=10000;level.numConnectedClients=4;
    g_gamestate.integer=GS_PLAYING;g_autoFireteams.integer=0;g_voiceChatsAllowed.integer=10;
    commandArgc=luaConsumes=created=joined=left=invited=voiceRecords=scoreCalls=voteCalls=0;
    memset(fireteamConfig,0,sizeof(fireteamConfig));
    for(i=0;i<MAX_FIRETEAMS;++i)memset(level.fireTeams[i].joinOrder,-1,sizeof(level.fireTeams[i].joinOrder));
    for(i=0;i<4;++i){
        gentity_t *p=&g_entities[i];p->inuse=qtrue;p->client=&clients[i];p->health=100;
        clients[i].pers.connected=CON_CONNECTED;clients[i].sess.sessionTeam=i<2?TEAM_AXIS:TEAM_ALLIES;
        clients[i].ps.stats[STAT_HEALTH]=100;p->s.number=p->s.clientNum=i;level.sortedClients[i]=i;
        VectorSet(p->r.currentOrigin,100+i,200+i,300+i);g_entities[100+i].health=i+1;
        strcpy(userinfo[i],"\\tceft\\1");
        level.fireTeams[i/2].inuse=qtrue;level.fireTeams[i/2].joinOrder[i%2]=(char)i;
    }
}
static void command(const char *text)
{
    char copy[512],*word;
    Q_strncpyz(copy,text,sizeof(copy));commandArgc=0;word=strtok(copy," ");
    while(word&&commandArgc<12){Q_strncpyz(commandArgs[commandArgc++],word,128);word=strtok(NULL," ");}
    ClientCommand(0);
}
static void lifecycleAndCommands(void)
{
    fireteamData_t *ft;int i,kind;
    setup();memset(level.fireTeams,0,sizeof(level.fireTeams));
    command("fireteam create");CHECK(G_IsFireteamLeader(0,&ft)&&ft->ident==1&&created==1&&joined==1);
    CHECK(!strcmp(fireteamConfig[0],"\\id\\0\\l\\0\\c\\0000000000000001"));
    command("fireteam create");CHECK(created==1&&G_CountTeamFireteams(TEAM_AXIS)==1);
    g_entities[1].r.svFlags|=SVF_BOT;command("fireteam invite 2");
    CHECK(G_IsOnFireteam(1,NULL)&&joined==2&&invited==1);
    command("fireteam invite 3");CHECK(!G_IsOnFireteam(2,NULL)); /* Enemy. */
    command("say_buddy BuddyText");CHECK(strstr(commands[sends-1],"BuddyText")!=NULL);
    sends=0;command("say_buddy BuddyText");CHECK(sends==2&&to[0]==0&&to[1]==1);
    sends=0;command("vsay_buddy -1 0 FTFollowMe");CHECK(sends==2&&to[0]==0&&to[1]==1&&voiceRecords==2);
    CHECK(strstr(commands[0],"vbchat")&&strstr(commands[0],"FTFollowMe"));
    command("fireteam kick 2");CHECK(!G_IsOnFireteam(1,NULL)&&left==1);
    g_entities[1].r.svFlags=0;command("fireteam invite 2");
    CHECK(!G_IsOnFireteam(1,NULL)&&clients[1].pers.invitationClient==0&&clients[1].pers.invitationEndTime==30500);
    G_AddClientToFireteam(1,0);CHECK(G_IsOnFireteam(1,NULL)); /* Existing vote-accept endpoint. */
    for(i=4;i<8;++i){g_entities[i].inuse=qtrue;g_entities[i].client=&clients[i];clients[i].pers.connected=CON_CONNECTED;clients[i].sess.sessionTeam=TEAM_AXIS;G_AddClientToFireteam(i,0);}
    CHECK(ft->joinOrder[5]==7);g_entities[8].inuse=qtrue;g_entities[8].client=&clients[8];clients[8].pers.connected=CON_CONNECTED;clients[8].sess.sessionTeam=TEAM_AXIS;
    G_AddClientToFireteam(8,0);CHECK(!G_IsOnFireteam(8,NULL));
    G_RemoveClientFromFireteams(4,qtrue,qfalse);CHECK(!G_IsOnFireteam(4,NULL));
    G_AddClientToFireteam(8,0);CHECK(G_IsOnFireteam(8,NULL));
    command("fireteam disband");CHECK(!ft->inuse&&!G_IsOnFireteam(0,NULL)&&!G_IsOnFireteam(8,NULL));
    CHECK(!strcmp(fireteamConfig[0],"\\id\\-1"));
    for(kind=0;kind<10;++kind){
        setup();sends=0;
        if(kind==0)clients[0].sess.muted=1;
        if(kind==1)clients[0].sess.sessionTeam=TEAM_SPECTATOR;
        if(kind==2)clients[0].sess.sessionTeam=TEAM_FREE;
        if(kind==3)clients[0].ps.stats[STAT_HEALTH]=0;
        if(kind==4)clients[0].ps.eFlags=EF_DEAD;
        if(kind==5)clients[0].ps.pm_type=PM_DEAD;
        if(kind==6)clients[0].ps.pm_flags=PMF_FOLLOW;
        if(kind==7)luaConsumes=1;
        if(kind==8)g_gamestate.integer=GS_WARMUP;
        if(kind==9)clients[0].ps.eFlags=EF_DEAD|EF_TALK; /* Preserve native equality gate. */
        command("say_buddy BuddyText");
        CHECK((kind>=8)?sends==2:sends<=1);if(kind<8&&sends)CHECK(strstr(commands[0],"BuddyText")==NULL);
        sends=0;command("vsay_buddy -1 0 FTFollowMe");
        CHECK(kind==9?sends==2:sends<=1);if(kind!=9&&sends)CHECK(strstr(commands[0],"FTFollowMe")==NULL);
    }
    setup();sends=0;command("say_team TeamText");CHECK(sends==2&&to[1]==1);
    sends=0;command("vsay_team TeamFollowMe");CHECK(sends==2&&strstr(commands[0],"vtchat"));
    sends=0;command("vsay GlobalDisabled");CHECK(sends==0);
    command("score");command("vote yes");CHECK(scoreCalls==1&&voteCalls==1);
    setup();memset(level.fireTeams,0,sizeof(level.fireTeams));clients[0].sess.sessionTeam=TEAM_SPECTATOR;
    command("fireteam create");CHECK(!G_IsOnFireteam(0,NULL));
    clients[0].sess.sessionTeam=TEAM_AXIS;clients[0].pers.connected=CON_CONNECTING;command("fireteam create");CHECK(created==0);
    clients[0].pers.connected=CON_CONNECTED;g_autoFireteams.integer=1;command("fireteam create");CHECK(created==1&&clients[0].pers.autofireteamEndTime==30500);
}
static void fullFireteam(void)
{
    const int ids[6]={0,1,7,31,32,63};
    char expected[800],entry[96];
    int i;
    setup();
    for(i=0;i<MAX_FIRETEAMS;++i){level.fireTeams[i].inuse=qfalse;memset(level.fireTeams[i].joinOrder,-1,sizeof(level.fireTeams[i].joinOrder));}
    level.fireTeams[0].inuse=qtrue;level.fireTeams[0].ident=1;
    strcpy(expected,"tceftinfo 0 6");
    for(i=0;i<6;++i){
        int id=ids[i];gentity_t *p=&g_entities[id];
        p->inuse=qtrue;p->client=&clients[id];p->health=80+i;
        clients[id].pers.connected=CON_CONNECTED;clients[id].sess.sessionTeam=TEAM_AXIS;
        VectorSet(p->r.currentOrigin,100+id,200+id,40);g_entities[100+id].health=2;
        strcpy(userinfo[id],"\\tceft\\1");level.fireTeams[0].joinOrder[i]=(char)id;
        Com_sprintf(entry,sizeof(entry)," %i %i 2 %i %i 40 0",id,80+i,100+id,200+id);Q_strcat(expected,sizeof(expected),entry);
    }
    G_UpdateFireteamConfigString(&level.fireTeams[0]);
    CHECK(!strcmp(fireteamConfig[0],"\\id\\0\\l\\0\\c\\8000000180000083"));
    G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(sends==6);
    for(i=0;i<6;++i){CHECK(to[i]==ids[i]);CHECK(!strcmp(commands[i],expected));CHECK(strlen(commands[i])<MAX_STRING_CHARS);}
    /* CONNECTING reused slot gets no recipient or stale member entry. */
    sends=0;clients[63].pers.connected=CON_CONNECTING;G_TCEFireteamInfoMessage(TEAM_AXIS);
    CHECK(sends==5);for(i=0;i<sends;++i)CHECK(to[i]!=63&&strstr(commands[i]," 63 85 ")==NULL);
    clients[63].pers.connected=CON_CONNECTED;g_entities[63].health=22;sends=0;
    G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(sends==6&&strstr(commands[0]," 63 22 2 163 263 40")!=NULL);
}
int main(void)
{
    int kind;
    setup();G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(sends==2&&to[0]==0&&to[1]==1);
    CHECK(strstr(commands[0],"tceftinfo 0 2 0 100 1 100 200 300 0 1 100 2 101 201 301 0")!=NULL);
    CHECK(!strcmp(commands[0],commands[1]));
    for(kind=0;kind<5;++kind){
        setup();
        if(kind==0)strcpy(userinfo[1],"\\tceft\\0");
        if(kind==1)clients[1].pers.connected=CON_CONNECTING;
        if(kind==2)clients[1].sess.sessionTeam=TEAM_ALLIES;
        if(kind==3)g_entities[1].inuse=qfalse;
        if(kind==4)level.fireTeams[0].joinOrder[1]=127;
        G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(sends==1&&to[0]==0);
    }
    setup();g_entities[1].health=-1;G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(strstr(commands[0]," 1 0 2 ")!=NULL);
    setup();clients[1].ps.pm_flags=PMF_LIMBO;clients[1].ps.stats[STAT_HEALTH]=100;
    G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(strstr(commands[0]," 1 -1 2 ")!=NULL);
    setup();g_entities[101].health=TCE_MAX_LOCATIONS;G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(strstr(commands[0]," 1 100 0 ")!=NULL);
    setup();G_TCEFireteamInfoMessage(TEAM_SPECTATOR);CHECK(sends==0);
    setup();level.fireTeams[0].joinOrder[0]=127;G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(sends==0);
    for(kind=1;kind<=4;++kind){
        char suffix[96];setup();
        if(kind==1)clients[1].ps.stats[STAT_TCE_WEAPON_FLAGS]=0x100;
        if(kind==2)clients[1].ps.stats[STAT_TCE_FLAGS]=0x100;
        if(kind==3)clients[1].ps.powerups[PW_REDFLAG]=INT_MAX;
        if(kind==4)clients[1].ps.powerups[PW_BLUEFLAG]=INT_MAX;
        G_TCEFireteamInfoMessage(TEAM_AXIS);sprintf(suffix," 1 100 2 101 201 301 %i",kind);CHECK(strstr(commands[0],suffix)!=NULL);
        sends=0;g_entities[1].health=0;G_TCEFireteamInfoMessage(TEAM_AXIS);CHECK(strstr(commands[0]," 1 0 2 101 201 301 0")!=NULL);
    }
    fullFireteam();
    lifecycleAndCommands();
    printf("Fireteam server: %i checks, %i failures\n",checks,failures);return failures?1:0;
}
