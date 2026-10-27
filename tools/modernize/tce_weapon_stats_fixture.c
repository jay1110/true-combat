#include "src/game/g_local.h"
#include "src/game/tce_bg.h"
#include "src/game/g_tce_weapon_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#undef vsnprintf

gentity_t g_entities[MAX_GENTITIES];
static gclient_t clients[MAX_CLIENTS];
level_locals_t level;
vmCvar_t g_gamestate;
tce_weaponDef_t weaponDef[64];tce_gearDef_t gearDef;
static int checks,failures,argcValue,sent,recipient[16];
static char args[8][64],packets[16][MAX_STRING_CHARS];
#define CHECK(c) do{++checks;if(!(c)){++failures;fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#c);}}while(0)
void QDECL Com_Printf(const char *fmt,...){(void)fmt;}
void QDECL Com_Error(int code,const char *fmt,...){(void)code;(void)fmt;abort();}
int Q_vsnprintf(char *s,int size,const char *fmt,va_list ap){return vsnprintf(s,(size_t)size,fmt,ap);}
int trap_Argc(void){return argcValue;}
void trap_Argv(int index,char *out,int size){Q_strncpyz(out,index<argcValue?args[index]:"",size);}
void trap_SendServerCommand(int client,const char *text){if(sent>=16)abort();recipient[sent]=client;Q_strncpyz(packets[sent++],text,MAX_STRING_CHARS);}
qboolean OnSameTeam(gentity_t *a,gentity_t *b){return a->client&&b->client&&a->client->sess.sessionTeam==b->client->sess.sessionTeam;}
/* ACTUAL_MODULE */

static void reset(void)
{
    int i;memset(g_entities,0,sizeof(g_entities));memset(clients,0,sizeof(clients));
    memset(&level,0,sizeof(level));memset(weaponDef,0,sizeof(weaponDef));memset(&gearDef,0,sizeof(gearDef));
    G_TCEWeaponStatsResetAll();level.time=10000;g_gamestate.integer=GS_PLAYING;sent=0;
    for(i=0;i<MAX_CLIENTS;++i){g_entities[i].s.number=i;g_entities[i].client=&clients[i];g_entities[i].health=100;
        clients[i].pers.connected=CON_CONNECTED;clients[i].pers.connectTime=i+1;clients[i].sess.sessionTeam=i?TEAM_ALLIES:TEAM_AXIS;}
    for(i=1;i<64;++i){weaponDef[i].parsed=1;weaponDef[i].mod=MOD_STEN;strcpy(gearDef.weaponFile[i],"test");}
}
static void damage(int victim,int weapon,int head,int killed)
{
    g_entities[victim].health=killed?-1:80;
    G_TCEWeaponStatsDamage(&g_entities[victim],&g_entities[0],&g_entities[0],weaponDef[weapon].mod,20,1,head);
}
static void shots(void)
{
    g_tceStatsShot_t outer,inner;int i;
    reset();outer=G_TCEWeaponStatsBegin(&g_entities[0],8);
    for(i=0;i<9;++i)damage(1,8,0,0);damage(2,8,1,1);damage(1,8,1,1);
    CHECK(weaponStats[0].row[8].attacks==1&&weaponStats[0].row[8].hits==1);
    CHECK(weaponStats[0].row[8].kills==2&&weaponStats[0].row[8].headshots==1);
    CHECK(weaponStats[1].row[8].deaths==1&&weaponStats[2].row[8].deaths==1);
    G_TCEWeaponStatsEnd(&g_entities[0],outer);damage(1,8,1,1);CHECK(weaponStats[0].row[8].kills==2);
    outer=G_TCEWeaponStatsBegin(&g_entities[0],45);damage(1,45,0,0);G_TCEWeaponStatsEnd(&g_entities[0],outer);
    CHECK(weaponStats[0].row[45].attacks==1&&weaponStats[0].row[45].hits==1&&weaponStats[0].row[45].headshots==0);
    CHECK(weaponStats[0].row[8].attacks==1&&weaponStats[0].row[8].hits==1); /* same MOD, distinct IDs */
    outer=G_TCEWeaponStatsBegin(&g_entities[0],8);inner=G_TCEWeaponStatsBegin(&g_entities[0],45);
    damage(1,45,0,0);G_TCEWeaponStatsEnd(&g_entities[0],inner);damage(1,8,0,0);G_TCEWeaponStatsEnd(&g_entities[0],outer);
    CHECK(weaponStats[0].row[8].attacks==2&&weaponStats[0].row[8].hits==2);
    CHECK(weaponStats[0].row[45].attacks==2&&weaponStats[0].row[45].hits==2);
    outer=G_TCEWeaponStatsBegin(&g_entities[0],8);inner=G_TCEWeaponStatsBegin(&g_entities[0],45);
    G_TCEWeaponStatsReset(0);G_TCEWeaponStatsEnd(&g_entities[0],inner);damage(1,8,1,1);
    CHECK(!weaponStats[0].active.token&&!weaponStats[0].row[8].hits&&!weaponStats[0].row[45].hits);
    G_TCEWeaponStatsEnd(&g_entities[0],outer);
    reset();outer=G_TCEWeaponStatsBegin(&g_entities[0],8);clients[1].sess.sessionTeam=TEAM_AXIS;damage(1,8,1,1);
    G_TCEWeaponStatsDamage(&g_entities[0],&g_entities[0],&g_entities[0],MOD_STEN,20,1,1);
    G_TCEWeaponStatsDamage(&g_entities[2],&g_entities[0],&g_entities[0],MOD_STEN,0,1,1);
    G_TCEWeaponStatsDamage(&g_entities[2],&g_entities[0],&g_entities[0],MOD_STEN,20,0,1);
    G_TCEWeaponStatsDamage(&g_entities[2],&g_entities[0],&g_entities[0],65,20,1,1);
    CHECK(!weaponStats[0].row[8].hits&&!weaponStats[0].row[8].kills&&!weaponStats[0].row[8].headshots);
    G_TCEWeaponStatsEnd(&g_entities[0],outer);g_gamestate.integer=GS_WARMUP;outer=G_TCEWeaponStatsBegin(&g_entities[0],8);damage(2,8,1,1);
    CHECK(weaponStats[0].row[8].attacks==1&&!weaponStats[0].row[8].hits);
    reset();weaponStats[0].initialized=1;weaponStats[0].connectTime=clients[0].pers.connectTime;
    weaponStats[0].row[8].attacks=weaponStats[0].row[8].hits=weaponStats[0].row[8].headshots=INT_MAX;
    outer=G_TCEWeaponStatsBegin(&g_entities[0],8);damage(1,8,1,0);G_TCEWeaponStatsEnd(&g_entities[0],outer);
    CHECK(weaponStats[0].row[8].attacks==INT_MAX&&weaponStats[0].row[8].hits==INT_MAX&&weaponStats[0].row[8].headshots==INT_MAX);
}
static void projectiles(void)
{
    g_tceStatsShot_t old;gentity_t *p;int kind;
    reset();old=G_TCEWeaponStatsBegin(&g_entities[0],9);G_TCEWeaponStatsEnd(&g_entities[0],old);
    p=&g_entities[90];p->s.number=90;p->parent=&g_entities[0];p->r.ownerNum=0;p->s.weapon=9;p->s.pos.trTime=7000;
    p->methodOfDeath=p->splashMethodOfDeath=18;p->s.eType=ET_GENERAL; /* actual explosion changes eType first */
    old=G_TCEWeaponStatsBegin(&g_entities[0],45);g_entities[0].s.weapon=45;g_entities[0].client->ps.weapon=45;
    g_entities[1].health=g_entities[2].health=-1;
    G_TCEWeaponStatsDamage(&g_entities[1],p,&g_entities[0],18,20,1,0);
    G_TCEWeaponStatsDamage(&g_entities[2],p,&g_entities[0],18,20,1,0);
    CHECK(weaponStats[0].row[9].attacks==1&&weaponStats[0].row[9].hits==1&&weaponStats[0].row[9].kills==2);
    CHECK(weaponStats[0].row[45].attacks==1&&!weaponStats[0].row[45].hits&&!weaponStats[0].row[45].kills);
    for(kind=0;kind<3;++kind){if(kind==0)p->parent=NULL;else if(kind==1){p->parent=&g_entities[0];p->r.ownerNum=1;}else{p->r.ownerNum=0;p->s.weapon=64;}
        G_TCEWeaponStatsDamage(&g_entities[1],p,&g_entities[0],18,20,1,0);CHECK(weaponStats[0].row[9].kills==2&&!weaponStats[0].row[45].hits);}
    G_TCEWeaponStatsEnd(&g_entities[0],old);
}
static qboolean request(int owner,const char *target,const char *sequence)
{
    argcValue=4;strcpy(args[0],"wstats");Q_strncpyz(args[1],target,64);strcpy(args[2],"tce1");Q_strncpyz(args[3],sequence,64);
    return G_TCEWeaponStatsRequest(&g_entities[owner]);
}
static void protocol(void)
{
    int i,j;static const char *bad[]={"-1","2147483648","1x","","+1"," 1"};
    reset();CHECK(request(0,"0","123"));CHECK(sent==1&&!strcmp(packets[0],"tws1 123 0 0 0 0 0 0 0"));
    CHECK(request(0,"0","124"));CHECK(sent==1);level.time+=499;CHECK(request(0,"0","125"));CHECK(sent==1);
    ++level.time;CHECK(request(0,"0","126"));CHECK(sent==2);
    for(i=0;i<6;++i){sent=0;level.time+=500;CHECK(request(0,bad[i],"1"));CHECK(!sent);CHECK(request(0,"0",bad[i]));CHECK(!sent);}
    sent=0;CHECK(request(0,"64","1"));CHECK(!sent);CHECK(request(0,"0","0"));CHECK(!sent);
    argcValue=3;CHECK(G_TCEWeaponStatsRequest(&g_entities[0]));CHECK(!sent);
    strcpy(args[2],"old");CHECK(!G_TCEWeaponStatsRequest(&g_entities[0]));
    level.time+=500;CHECK(request(1,"0","1"));CHECK(!sent);
    clients[1].sess.sessionTeam=TEAM_SPECTATOR;clients[1].sess.spectatorState=SPECTATOR_FOLLOW;clients[1].sess.spectatorClient=0;
    CHECK(request(1,"0","2"));CHECK(sent==1&&recipient[0]==1);
    level.time+=500;sent=0;clients[1].sess.spectatorState=SPECTATOR_FREE;clients[1].pers.mvReferenceList=1;
    CHECK(request(1,"32","3"));CHECK(!sent);CHECK(request(1,"0","4"));CHECK(sent==1);
    reset();TCE_StatsClient(&g_entities[0]);
    for(i=1;i<64;++i){tce_weaponStatsRow_t *r=&weaponStats[0].row[i];r->attacks=r->hits=r->kills=r->deaths=r->headshots=INT_MAX;}
    CHECK(request(0,"0","2147483647"));CHECK(sent==8);
    for(i=0;i<sent;++i){int sequence,client,offset,total,rounds,a,b,c;
        CHECK(strlen(packets[i])<MAX_STRING_CHARS-1);CHECK(recipient[i]==0);
        CHECK(sscanf(packets[i],"tws1 %d %d %d %d %d %d %d %d",&sequence,&client,&offset,&total,&rounds,&a,&b,&c)==8);
        CHECK(sequence==INT_MAX&&client==0&&offset==8*i&&total==63);}
    for(j=0;j<64;++j)CHECK(weaponStats[0].row[j].attacks==(j?INT_MAX:0));
    G_TCEWeaponStatsReset(0);CHECK(!weaponStats[0].initialized);for(j=0;j<64;++j)CHECK(!weaponStats[0].row[j].attacks);
    TCE_StatsClient(&g_entities[0]);weaponStats[0].row[8].hits=5;++clients[0].pers.connectTime;
    CHECK(TCE_StatsClient(&g_entities[0])->row[8].hits==0);
    clients[0].pers.connected=CON_CONNECTING;CHECK(TCE_StatsClient(&g_entities[0])==NULL);
    G_TCEWeaponStatsReset(-1);G_TCEWeaponStatsReset(MAX_CLIENTS);G_TCEWeaponStatsResetAll();CHECK(!projectileStats[90].valid);
}
int main(void){shots();projectiles();protocol();printf("TCE per-weapon counters/protocol: %d checks, %d failures\n",checks,failures);return failures?1:0;}
