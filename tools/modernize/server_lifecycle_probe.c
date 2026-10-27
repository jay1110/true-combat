/* Engine-boundary regression harness; implementations are extracted unchanged. */
#include "src/game/g_local.h"
#include "src/game/tce_bg.h"
#include "src/game/g_tce_pose.h"
#include <stdio.h>
#include <stdlib.h>

level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];
g_campaignInfo_t g_campaigns[1];
tce_gearDef_t gearDef;
vmCvar_t g_gametype, g_gamestate, g_currentRound, g_altStopwatchMode, g_swapteams;
vmCvar_t sv_official, vote_allow_map;
static int failures, checks, handles, loads, frees, missing, official = 1, description;
static int parsedStats, deletedStats, configReads, userinfoSlot, sent[MAX_CLIENTS];
static int poseChanges;
static gclient_t *lastPoseClient;
static char requestedCharacter[256], configCharacter[256], session[1024];
static bg_character_t defaults[TEAM_NUM_TEAMS][NUM_PLAYER_CLASSES], custom;

#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; fprintf(stderr,"line %d: %s\n",__LINE__,#expr); } } while (0)
/* This fixture owns the character-selection service boundary. The actual pose
 * reset body runs in pose_server_regression; do not duplicate it here. */
void G_TCEPoseCharacterChanged(gclient_t *client) {
    ++poseChanges;lastPoseClient=client;
    CHECK(client==&g_clients[2] && client->pers.character!=NULL);
    CHECK(client->ps.legsAnim==0 && client->ps.torsoAnim==0);
    CHECK(client->ps.legsTimer==0 && client->ps.torsoTimer==0);
}
void QDECL Com_Error(int code, const char *fmt, ...) { (void)code;(void)fmt;abort(); }
void QDECL Com_Printf(const char *fmt, ...) { (void)fmt; }
#undef vsnprintf
int Q_vsnprintf(char *dest,int size,const char *fmt,va_list args) { return vsnprintf(dest,(size_t)size,fmt,args); }
void G_Error(const char *fmt, ...) { (void)fmt;abort(); }
void G_Printf(const char *fmt, ...) { (void)fmt; }
void G_refPrintf(gentity_t *e, const char *fmt, ...) { (void)e;(void)fmt; }
int trap_PC_LoadSource(const char *name) { CHECK(strstr(name,"scripts/")==name);++loads;if(missing)return 0;CHECK(handles==0);++handles;return 19; }
int trap_PC_FreeSource(int handle) { CHECK(handle==19 && handles==1);--handles;++frees;return 1; }
void trap_GetServerinfo(char *s,int size) { Q_strncpyz(s,"\\mapname\\obj_fixture",size); }
int BG_MapIsOfficial(const char *s) { (void)s;return official; }
void G_voteCurrentSetting(gentity_t *e,const char *a,const char *b) { (void)e;(void)a;(void)b; }
void G_voteDisableMessage(gentity_t *e,const char *a) { (void)e;(void)a; }
qboolean G_voteDescription(gentity_t *e,qboolean ref,int index) { (void)e;(void)ref;(void)index;return description?qtrue:qfalse; }
void Svcmd_ResetMatch_f(qboolean a,qboolean b) { (void)a;(void)b; }
void trap_SendConsoleCommand(int when,const char *s) { (void)when;(void)s; }
void trap_Cvar_VariableStringBuffer(const char *key,char *s,int size) {
    Q_strncpyz(s,!strcmp(key,"session0")?session:!strcmp(key,"wstats0")?"old-statistics":"1 2 3 4 5 6 7 1 2 3 4 5 6 7",size);
}
void trap_Cvar_Set(const char *key,const char *value) { if(!strcmp(key,"wstats0")){CHECK(!strcmp(value,"0"));++deletedStats;} }
void G_parseStats(char *s) { (void)s;++parsedStats;g_clients[0].sess.damage_given=455; }
void G_CalcRank(gclient_t *client) { (void)client; }
void G_ClientSwap(gclient_t *client) { (void)client; }
void trap_GetUserinfo(int slot,char *s,int size) { userinfoSlot=slot;Com_sprintf(s,size,"\\ch\\%s",requestedCharacter); }
void trap_GetConfigstring(int index,char *s,int size) { CHECK(index>=CS_CHARACTERS && index<CS_CHARACTERS+MAX_CHARACTERS);++configReads;Q_strncpyz(s,configCharacter,size); }
bg_character_t *BG_GetCharacter(int team,int cls) { CHECK(team>=0 && team<TEAM_NUM_TEAMS && cls>=0 && cls<NUM_PLAYER_CLASSES);return &defaults[team][cls]; }
bg_character_t *BG_FindCharacter(const char *name) { CHECK(!strcmp(name,"characters/server.char"));return &custom; }
bg_character_t *BG_FindFreeCharacter(const char *name) { (void)name;abort();return NULL; }
qboolean G_RegisterCharacter(const char *name,bg_character_t *c) { (void)name;(void)c;abort();return qfalse; }
void trap_SendServerCommand(int client,const char *s) { CHECK(client>=0 && client<MAX_CLIENTS);CHECK(strstr(s,"tinfo ")==s);++sent[client]; }

/* ACTUAL_FUNCTIONS */

static void votes(void) {
    int n, mode;gentity_t *ent=&g_entities[0];ent->client=&g_clients[0];
    for(mode=0;mode<5;++mode) {
        missing=mode==1;sv_official.integer=mode==2;official=mode!=2;
        vote_allow_map.integer=mode!=3;description=mode==4;
        for(n=0;n<256;++n) {
            int before=frees,result=G_Map_v(ent,5,"map","obj_fixture",qfalse);
            CHECK(handles==0);CHECK(result==(mode==0?G_OK:G_INVALID));
            CHECK(frees-before==(missing?0:1));
        }
    }
    CHECK(loads==1280 && frees==1024);
}

static void sessions(void) {
    int mode, reset;const int modes[]={2,5,6,7};
    Q_strncpyz(session,"2 1000 0 0 2 8 2 21 2 8 2 21 0 0 0 0 0 0 9 17 11 0 0 0 2 1 0 0 0 0 0 42 0",sizeof(session));
    for(mode=0;mode<4;++mode)for(reset=0;reset<2;++reset) {
        gclient_t *c=&g_clients[0];memset(c,0,sizeof(*c));
        level.fResetStats=reset;
        g_gametype.integer=modes[mode];g_gamestate.integer=GS_PLAYING;g_currentRound.integer=1;
        parsedStats=deletedStats=0;G_ReadSessionData(c);
        CHECK(c->sess.sessionTeam==TEAM_ALLIES && c->sess.playerWeapon==8 && c->sess.latchPlayerWeapon==8);
        CHECK(c->sess.playerWeapon2==2 && c->sess.playerWeapon3==21);
        CHECK(parsedStats==!reset && deletedStats==reset);
        CHECK(c->sess.kills==(reset?0:11) && c->sess.deaths==(reset?0:9));
        CHECK(c->sess.damage_given==(reset?0:455));
        if(reset) { CHECK(c->sess.game_points==0 && c->sess.team_kills==0 && c->sess.suicides==0 && c->sess.rounds==0);CHECK(c->sess.skillpoints[0]==0 && c->sess.medals[0]==0); }
    }
}

static void characters(void) {
    const char *bad[]={"-1","16","99999999999999999999999999","2garbage","+2"," 2","skin"};
    int mode,i;gclient_t *c=&g_clients[2];memset(c,0,sizeof(*c));c->sess.sessionTeam=TEAM_AXIS;c->sess.playerType=PC_SOLDIER;
    poseChanges=0;lastPoseClient=NULL;
    Q_strncpyz(configCharacter,"characters/server.char",sizeof(configCharacter));
    for(mode=0;mode<4;++mode) {
        const int modes[]={2,5,6,7};g_gametype.integer=modes[mode];g_entities[2].r.svFlags=0;
        for(i=0;i<MAX_CHARACTERS;++i) {
            Com_sprintf(requestedCharacter,sizeof(requestedCharacter),"%d",i);configReads=0;G_UpdateCharacter(c);
            CHECK(c->pers.character==&defaults[TEAM_AXIS][PC_SOLDIER] && c->pers.characterIndex==-1 && configReads==0);
        }
    }
    CHECK(poseChanges==1 && lastPoseClient==c);
    CHECK(userinfoSlot==2);g_entities[2].r.svFlags=SVF_BOT;
    for(i=0;i<(int)(sizeof(bad)/sizeof(bad[0]));++i) {
        Q_strncpyz(requestedCharacter,bad[i],sizeof(requestedCharacter));configReads=0;G_UpdateCharacter(c);
        CHECK(configReads==0 && c->pers.characterIndex==-1);
    }
    CHECK(poseChanges==1);
    c->ps.legsAnim=3;c->ps.torsoAnim=4;c->ps.legsTimer=5;c->ps.torsoTimer=6;
    Q_strncpyz(requestedCharacter,"2",sizeof(requestedCharacter));configReads=0;G_UpdateCharacter(c);
    CHECK(configReads==1 && c->pers.character==&custom && c->pers.characterIndex==2);
    CHECK(poseChanges==2 && lastPoseClient==c);
    configCharacter[0]=0;G_UpdateCharacter(c);CHECK(c->pers.characterIndex==-1);
    CHECK(poseChanges==3);
    g_entities[2].r.svFlags=0;requestedCharacter[0]=0;c->sess.sessionTeam=TEAM_ALLIES;G_UpdateCharacter(c);
    CHECK(c->pers.character==&defaults[TEAM_ALLIES][PC_SOLDIER]);
    CHECK(poseChanges==4);
    c->ps.legsAnim=7;c->ps.torsoAnim=9;c->ps.legsTimer=11;c->ps.torsoTimer=13;
    G_UpdateCharacter(c);
    CHECK(poseChanges==4 && c->ps.legsAnim==7 && c->ps.torsoAnim==9);
    CHECK(c->ps.legsTimer==11 && c->ps.torsoTimer==13);
}

static void team_snapshot(void) {
    int i;memset(sent,0,sizeof(sent));level.numConnectedClients=2;level.tinfoAxis[0]=0;
    for(i=0;i<2;++i) { level.sortedClients[i]=i;g_entities[i].client=&g_clients[i];g_entities[i].inuse=qtrue;g_entities[i].r.svFlags=0;
        memset(&g_clients[i],0,sizeof(g_clients[i]));g_clients[i].sess.sessionTeam=TEAM_AXIS;g_clients[i].ps.stats[STAT_HEALTH]=100; }
    g_clients[0].pers.connected=CON_CONNECTED;g_clients[1].pers.connected=CON_CONNECTING;
    TeamplayInfoMessage(TEAM_AXIS);CHECK(sent[0]==1 && sent[1]==0);
    g_clients[1].pers.connected=CON_CONNECTED;TeamplayInfoMessage(TEAM_AXIS);CHECK(sent[1]==0);
    /* ClientBegin's source integration is checked by the runner. */
    level.tinfoAxis[0]=0;level.tinfoAllies[0]=0;TeamplayInfoMessage(TEAM_AXIS);CHECK(sent[1]==1);
}

static void loadouts(void) {
    int id,slot,role;const int utility[]={TCE_WP_K1,TCE_WP_G1,TCE_WP_G2,TCE_WP_G3,TCE_WP_DC1};
    memset(&gearDef,0,sizeof(gearDef));
    for(id=0;id<TCE_MAX_WEAPONS;++id) { gearDef.weaponFile[id][0]='x';gearDef.team[id]=2;for(role=0;role<3;++role)gearDef.requiredSkill[id][role]=1; }
    for(slot=1;slot<=2;++slot)for(role=0;role<3;++role) {
        for(id=0;id<TCE_MAX_WEAPONS;++id)gearDef.slot[id]=slot;
        for(id=0;id<(int)(sizeof(utility)/sizeof(utility[0]));++id)CHECK(!TCE_SpawnWeaponAvailable(utility[id],role,3,TEAM_ALLIES,slot));
        CHECK(!TCE_SpawnWeaponAvailable(-1,role,3,TEAM_ALLIES,slot));CHECK(!TCE_SpawnWeaponAvailable(TCE_MAX_WEAPONS,role,3,TEAM_ALLIES,slot));
        CHECK(TCE_SpawnWeaponAvailable(8,role,3,TEAM_ALLIES,slot));
        CHECK(!TCE_SpawnWeaponAvailable(8,role,3,TEAM_ALLIES,3-slot));
        CHECK(!TCE_SpawnWeaponAvailable(8,-1,3,TEAM_ALLIES,slot));CHECK(!TCE_SpawnWeaponAvailable(8,3,3,TEAM_ALLIES,slot));
        gearDef.requiredSkill[8][role]=4;CHECK(!TCE_SpawnWeaponAvailable(8,role,3,TEAM_ALLIES,slot));gearDef.requiredSkill[8][role]=1;
        gearDef.team[8]=0;CHECK(!TCE_SpawnWeaponAvailable(8,role,3,TEAM_ALLIES,slot));gearDef.team[8]=2;
    }
}

int main(void) {
    level.clients=g_clients;votes();sessions();characters();team_snapshot();loadouts();
    printf("%d server lifecycle checks, %d failures\n",checks,failures);return failures?1:0;
}
