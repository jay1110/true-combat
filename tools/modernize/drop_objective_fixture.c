#define GAMEDLL
#include "src/game/g_local.h"
#include "src/game/tce_numeric.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];
level_locals_t level;
vmCvar_t g_gametype,g_gamestate,g_maxclients;
gitem_t bg_itemlist[5];
int bg_numItems=5;
static int checks,failures,links,spawns,freed,scriptCount,returned,solidFirst,traceCount;
static char scriptNames[12][64];
static gentity_t popup;
#define CHECK(c) do {++checks;if(!(c)){++failures;fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);}}while(0)
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int n,const char *fmt,...) {(void)n;(void)fmt;abort();}
int Q_vsnprintf(char *d,int n,const char *fmt,va_list a) {return vsnprintf(d,(size_t)n,fmt,a);}
void G_Error(const char *fmt,...) {(void)fmt;abort();}
void G_Printf(const char *fmt,...) {(void)fmt;}
void G_AddEvent(gentity_t *ent,int event,int parm) {(void)ent;(void)event;(void)parm;}
void AddScore(gentity_t *ent,int score) {(void)ent;(void)score;}
void Bot_TeamScriptEvent(int team,char *event,char *params) {(void)team;(void)event;(void)params;}
void Touch_Item_Auto(gentity_t *ent,gentity_t *other,trace_t *trace)
{(void)ent;(void)other;(void)trace;CHECK(0);}
void PrintMsg(gentity_t *ent,const char *fmt,...) {(void)ent;(void)fmt;CHECK(0);}
gentity_t *G_PopupMessage(popupMessageType_t type) {(void)type;return &popup;}
gentity_t *G_Spawn(void)
{
    int i;for(i=MAX_CLIENTS+3;i<MAX_GENTITIES;++i)if(!g_entities[i].inuse){
        memset(&g_entities[i],0,sizeof(g_entities[i]));g_entities[i].inuse=qtrue;g_entities[i].s.number=i;
        if(level.num_entities<=i)level.num_entities=i+1;++spawns;return &g_entities[i];
    }abort();return NULL;
}
void G_FreeEntity(gentity_t *ent) {ent->inuse=qfalse;ent->r.contents=0;++freed;}
void G_SetOrigin(gentity_t *ent,vec3_t value) {VectorCopy(value,ent->s.pos.trBase);VectorCopy(value,ent->r.currentOrigin);}
void G_SetAngle(gentity_t *ent,vec3_t value) {VectorCopy(value,ent->s.apos.trBase);}
void trap_LinkEntity(gentity_t *ent) {(void)ent;++links;}
void trap_Trace(trace_t *tr,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int pass,int mask)
{(void)start;(void)mins;(void)maxs;(void)pass;(void)mask;memset(tr,0,sizeof(*tr));tr->fraction=1;VectorCopy(end,tr->endpos);if(solidFirst&&!traceCount)tr->startsolid=qtrue;++traceCount;}
gitem_t *BG_FindItemForClassName(const char *name)
{int i;for(i=1;i<5;++i)if(!strcmp(name,bg_itemlist[i].classname))return &bg_itemlist[i];return NULL;}
void G_Script_ScriptEvent(gentity_t *ent,char *event,char *params)
{
    (void)ent;CHECK(!strcmp(event,"trigger"));
    if(scriptCount<12)Q_strncpyz(scriptNames[scriptCount++],params,64);
    if(!strcmp(params,"returned"))++returned;
    if(strstr(params,"dropped")||!strcmp(params,"dropped"))
        CHECK(!g_clients[0].ps.powerups[PW_REDFLAG]&&!g_clients[0].ps.powerups[PW_BLUEFLAG]);
}
/* ACTUAL_FLAG_ENUM */
#include "src/game/etbot/tce_flag_query.h"
/* ACTUAL_FUNCTIONS */

static gentity_t *carrier(void) {return &g_entities[0];}
static gentity_t *base(void) {return &g_entities[MAX_CLIENTS];}
static void setup(int team)
{
    gentity_t *ent,*home;
    memset(g_entities,0,sizeof(g_entities));memset(g_clients,0,sizeof(g_clients));memset(&level,0,sizeof(level));
    memset(bg_itemlist,0,sizeof(bg_itemlist));memset(&popup,0,sizeof(popup));
    spawns=links=freed=scriptCount=returned=solidFirst=traceCount=0;
    g_gametype.integer=2;g_gamestate.integer=GS_PLAYING;g_maxclients.integer=MAX_CLIENTS;
    level.time=5000;level.num_entities=MAX_CLIENTS+3;level.gameManager=&g_entities[MAX_CLIENTS+2];
    bg_itemlist[1].classname="team_CTF_redflag";bg_itemlist[1].giType=IT_TEAM;bg_itemlist[1].giTag=PW_REDFLAG;
    bg_itemlist[2].classname="team_CTF_blueflag";bg_itemlist[2].giType=IT_TEAM;bg_itemlist[2].giTag=PW_BLUEFLAG;
    bg_itemlist[3].classname="ammo_dynamite";bg_itemlist[3].giType=IT_AMMO;
    bg_itemlist[4].classname="ammo_VIP";bg_itemlist[4].giType=IT_AMMO;
    ent=carrier();ent->inuse=qtrue;ent->s.number=0;ent->client=&g_clients[0];ent->health=100;
    ent->client->pers.connected=CON_CONNECTED;ent->client->sess.sessionTeam=team;
    ent->client->ps.clientNum=0;ent->client->ps.pm_type=PM_NORMAL;ent->client->ps.stats[STAT_HEALTH]=100;
    ent->client->ps.persistant[PERS_TEAM]=team;ent->client->ps.viewheight=48;
    VectorSet(ent->s.pos.trBase,100,200,40);VectorCopy(ent->s.pos.trBase,ent->s.origin);
    VectorCopy(ent->s.pos.trBase,ent->client->ps.origin);
    home=base();home->inuse=qtrue;home->s.number=MAX_CLIENTS;home->s.eType=ET_ITEM;
    home->item=&bg_itemlist[team==TEAM_AXIS?2:1];home->classname=home->item->classname;
    home->s.modelindex2=31;home->message="Fixture objective";home->s.density=0;
    home->flags=FL_NODRAW;home->r.svFlags=SVF_NOCLIENT;home->entstate=STATE_DEFAULT;
    ent->client->flagParent=MAX_CLIENTS;ent->client->ps.powerups[home->item->giTag]=INT_MAX;
    ent->client->speedScale=75;ent->client->pers.teamState.flagsince=123;
    ent->message=home->message;ent->s.otherEntityNum2=31;
}
static void lifecycle(void)
{
    int team,owner,round;
    for(team=TEAM_AXIS;team<=TEAM_ALLIES;++team){
        setup(team);CHECK(TceReadFlagState(base(),&owner)==S_FLAG_CARRIED&&owner==0);
        for(round=0;round<8;++round){
            gentity_t *drop;
            qboolean dropped=G_TCEManualDropObjective(carrier());CHECK(dropped);if(!dropped)return;
            drop=&g_entities[MAX_CLIENTS+3];
            CHECK(!carrier()->client->ps.powerups[base()->item->giTag]&&carrier()->client->flagParent==-1);
            CHECK(carrier()->client->speedScale==0&&carrier()->client->pers.teamState.flagsince==0);
            CHECK(!carrier()->message&&carrier()->s.otherEntityNum2==0);
            CHECK(drop->inuse&&(drop->flags&FL_DROPPED_ITEM)&&drop->s.modelindex2==31);
            CHECK(drop->s.otherEntityNum==MAX_CLIENTS&&drop->s.density==1&&drop->nextthink==level.time+30000);
            CHECK(drop->think==Team_DroppedFlagThink&&drop->message==base()->message);
            CHECK(TceReadFlagState(base(),&owner)==S_FLAG_DROPPED&&owner==-1);
            CHECK(!G_TCEManualDropObjective(carrier()));
            CHECK(scriptCount>=2&&!strcmp(scriptNames[0],team==TEAM_AXIS?"axis_object_dropped":"allied_object_dropped"));
            CHECK(Pickup_Team(drop,carrier())==-1);G_FreeEntity(drop); /* normal pickup caller removes the consumed entity */
            CHECK(TceReadFlagState(base(),&owner)==S_FLAG_CARRIED&&owner==0);
            CHECK(carrier()->client->flagParent==MAX_CLIENTS&&carrier()->message==base()->message);
        }
        CHECK(G_TCEManualDropObjective(carrier()));
        g_entities[MAX_CLIENTS+3].think(&g_entities[MAX_CLIENTS+3]);
        CHECK(returned>0&&freed==9&&base()->s.density==1);
        CHECK(TceReadFlagState(base(),&owner)==S_FLAG_AT_BASE&&owner==-1);
    }
}
static void permissionGuards(void)
{
    int kind;
    for(kind=0;kind<20;++kind){
        setup(TEAM_AXIS);
        switch(kind){
        case 0:g_gametype.integer=7;break;case 1:g_gamestate.integer=GS_WARMUP;break;
        case 2:level.intermissiontime=1;break;case 3:carrier()->health=0;break;
        case 4:g_clients[0].ps.stats[STAT_HEALTH]=0;break;case 5:g_clients[0].ps.pm_type=PM_DEAD;break;
        case 6:g_clients[0].ps.pm_flags=PMF_LIMBO;break;case 7:g_clients[0].ps.pm_flags=PMF_FOLLOW;break;
        case 8:g_clients[0].ps.pm_flags=PMF_TCE_OBJECTIVE_ACTION;break;
        case 9:g_clients[0].sess.sessionTeam=TEAM_SPECTATOR;break;case 10:g_clients[0].pers.connected=CON_CONNECTING;break;
        case 11:g_clients[0].ps.powerups[PW_BLUEFLAG]=0;break;case 12:g_clients[0].flagParent=-1;break;
        case 13:g_clients[0].flagParent=MAX_GENTITIES;break;case 14:base()->inuse=qfalse;break;
        case 15:base()->item=NULL;break;case 16:g_clients[0].ps.powerups[PW_REDFLAG]=INT_MAX;break;
        case 17:base()->classname="ammo_VIP";break;case 18:g_clients[0].ps.clientNum=1;break;
        case 19:base()->flags|=FL_DROPPED_ITEM;break;
        }
        CHECK(!G_TCEManualDropObjective(carrier()));CHECK(spawns==0&&scriptCount==0);
    }
    CHECK(!G_TCEManualDropObjective(NULL));
    setup(TEAM_AXIS);g_gametype.integer=5;g_clients[0].ps.powerups[PW_BLUEFLAG]=0;
    level.tceBombAssigned=qtrue;level.tceBombCarrier=0;level.tceBombCarrierCount=1;
    g_clients[0].ps.stats[STAT_TCE_WEAPON_FLAGS]|=0x100;
    CHECK(G_TCEManualDropObjective(carrier())&&level.tceBombCarrier==-1&&level.tceBombCarrierCount==0);
    CHECK(!G_TCEManualDropObjective(carrier())&&spawns==1);
}
static void homeTeamReturn(void)
{
    int team,owner;
    for(team=TEAM_AXIS;team<=TEAM_ALLIES;++team){
        gentity_t *other=&g_entities[1],*drop;
        setup(team);
        { qboolean dropped=G_TCEManualDropObjective(carrier());CHECK(dropped);if(!dropped)return; }
        drop=&g_entities[MAX_CLIENTS+3];other->inuse=qtrue;other->client=&g_clients[1];other->health=100;
        other->client->sess.sessionTeam=team==TEAM_AXIS?TEAM_ALLIES:TEAM_AXIS;
        CHECK(Pickup_Team(drop,other)==0);G_FreeEntity(drop);
        CHECK(returned==1&&base()->s.density==1);
        CHECK(TceReadFlagState(base(),&owner)==S_FLAG_AT_BASE&&owner==-1);
        CHECK(!other->client->ps.powerups[base()->item->giTag]);
    }
}
int main(void)
{
    CHECK(DROP_ROUTE);CHECK(DROP_REGISTERED);lifecycle();permissionGuards();homeTeamReturn();
    printf("drop objective: %d checks, %d failures\n",checks,failures);return failures!=0;
}
