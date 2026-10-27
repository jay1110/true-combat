/* G17 carrier lifecycle: complete extracted production functions, real game
 * structs, deterministic non-gameplay services. No player_die/BSP emulation. */
#include "src/game/g_local.h"
#include "src/game/tce_lua.h"
#include <stdio.h>
#include <string.h>

gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];
level_locals_t level;
vmCvar_t g_gametype, g_gamestate, g_landminetimeout;
gitem_t bg_itemlist[3];
int bg_numItems = 3;
mapEntityData_Team_t mapEntityData[2];
static gentity_t popup;
static int launches, checks, failures, links, deletes;

static int TCE_AddInt32(int a, int b) { return (int)((unsigned)a + (unsigned)b); }
int Q_stricmp(const char *a, const char *b) { return strcmp(a, b); }
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
    if (forward) { VectorSet(forward, 1, 0, 0); }
    if (right) { VectorSet(right, 0, -1, 0); }
    if (up) { VectorSet(up, 0, 0, 1); }
}
vec_t VectorNormalize(vec3_t value) { return 1; }
void trap_Trace(trace_t *tr, const vec3_t start, const vec3_t mins,
                const vec3_t maxs, const vec3_t end, int pass, int mask)
{ memset(tr, 0, sizeof(*tr)); tr->fraction = 1; VectorCopy(end, tr->endpos); }
gitem_t *BG_FindItemForClassName(const char *name)
{ return !strcmp(name, "ammo_dynamite") ? &bg_itemlist[1] : &bg_itemlist[2]; }
gitem_t *BG_FindItem(const char *name) { return NULL; }
gentity_t *G_PopupMessage(popupMessageType_t type) { return &popup; }
void G_Script_ScriptEvent(gentity_t *ent, char *event, char *params) { }
void SetWolfSpawnWeapons(gclient_t *client) { }
weapon_t G_GetPrimaryWeaponForClient(gclient_t *client) { return WP_NONE; }
void G_DropWeapon(gentity_t *ent, weapon_t weapon) { }
void G_TCEDropHealth(gentity_t *ent) { }
void G_LogPrintf(const char *format, ...) { }
void G_AddEvent(gentity_t *ent, int event, int parm) { }
void G_UseTargets(gentity_t *ent, gentity_t *activator) { }
gentity_t *G_TempEntity(vec3_t origin, int event) { return &popup; }
void trap_LinkEntity(gentity_t *ent) { ++links; }
void trap_UnlinkEntity(gentity_t *ent) { ent->r.linked = qfalse; }
void RespawnItem(gentity_t *ent) { }
int Pickup_Weapon(gentity_t *ent, gentity_t *other) { return 0; }
int Pickup_Health(gentity_t *ent, gentity_t *other) { return 0; }
int Pickup_Team(gentity_t *ent, gentity_t *other) { return 0; }
qboolean BG_AddMagicAmmo(playerState_t *ps, int *skill, int team, int clips) { return qfalse; }
void QDECL Com_Error(int code, const char *format, ...) { ++failures; }

gentity_t *LaunchItem(gitem_t *item, vec3_t origin, vec3_t velocity, int owner)
{
    gentity_t *drop = &g_entities[64 + launches++];
    memset(drop, 0, sizeof(*drop)); drop->inuse = qtrue;
    drop->item = item; drop->classname = item->classname;
    drop->s.modelindex = (int)(item - bg_itemlist);
    drop->flags = FL_DROPPED_ITEM; drop->r.contents = CONTENTS_ITEM;
    VectorCopy(origin, drop->r.currentOrigin);
    return drop;
}

void G_TCEPresentationUserinfo(int client, const char *text) { }
void G_TCEPresentationResetClient(gentity_t *ent) { }
#ifdef FEATURE_LUA
void TCE_LuaClientEvent(const char *callback, int client) { }
#endif
void G_RemoveClientFromFireteams(int client, qboolean update, qboolean print) { }
void G_RemoveFromAllIgnoreLists(int client) { }
void G_LeaveTank(gentity_t *ent, qboolean position) { }
void StopFollowing(gentity_t *ent) { }
void Cmd_FollowCycle_f(gentity_t *ent, int dir) { }
void G_ExplodeMines(gentity_t *ent) { }
void G_FadeItems(gentity_t *ent, int mod) { }
mapEntityData_t *G_FindMapEntityData(mapEntityData_Team_t *team, int client) { return NULL; }
mapEntityData_t *G_FindMapEntityDataSingleClient(mapEntityData_Team_t *team,
    mapEntityData_t *start, int client, int single) { return NULL; }
mapEntityData_t *G_FreeMapEntityData(mapEntityData_Team_t *team, mapEntityData_t *ent) { return NULL; }
void trap_SendServerCommand(int client, const char *text) { }
char * QDECL va(const char *format, ...) { return ""; }
char *G_createStats(gentity_t *ent) { return ""; }
void trap_SetConfigstring(int index, const char *value) { }
void CalculateRanks(void) { }
int BotAIShutdownClient(int client) { return 0; }
void G_verifyMatchState(int team) { }
void G_smvAllRemoveSingleClient(int client) { }

/* PRODUCTION_FUNCTIONS */

static void check(int ok, const char *label)
{ ++checks; if (!ok) { ++failures; printf("FAIL %s\n", label); } }
static int active_bombs(void)
{
    int i, count = 0;
    for (i = 64; i < 64 + launches; ++i)
        if (g_entities[i].inuse && !g_entities[i].freeAfterEvent &&
            !strcmp(g_entities[i].classname, "ammo_dynamite")) ++count;
    return count;
}
static void reset(void)
{
    memset(g_entities, 0, sizeof(g_entities)); memset(g_clients, 0, sizeof(g_clients));
    memset(&level, 0, sizeof(level)); memset(&popup, 0, sizeof(popup));
    level.clients = g_clients; level.gentities = g_entities;
    level.maxclients = 64; level.time = 50000; level.numConnectedClients = 1;
    level.tceBombAssigned = qtrue; level.tceBombCarrier = -1;
    level.tceDemolitionTeam = TEAM_AXIS;
    level.sortedClients[0] = 0;
    g_gametype.integer = 5; g_gamestate.integer = GS_PLAYING;
    launches = links = deletes = 0;
    bg_itemlist[1].classname = "ammo_dynamite"; bg_itemlist[1].giType = IT_AMMO;
    bg_itemlist[2].classname = "ammo_VIP"; bg_itemlist[2].giType = IT_AMMO;
}
static void connect_player(int id)
{
    gentity_t *ent = &g_entities[id]; gclient_t *client = &g_clients[id];
    memset(ent, 0, sizeof(*ent)); memset(client, 0, sizeof(*client));
    ent->client = client; ent->inuse = qtrue; ent->s.number = id; ent->health = 100;
    client->pers.connected = CON_CONNECTED; client->ps.clientNum = id;
    client->sess.sessionTeam = TEAM_AXIS; client->ps.persistant[PERS_TEAM] = TEAM_AXIS;
    client->ps.stats[STAT_HEALTH] = 100; client->ps.stats[STAT_MAX_HEALTH] = 100;
    client->pers.cmd.buttons = BUTTON_ACTIVATE;
}
static void assign(int id)
{
    level.tceBombCarrier = id; level.tceBombCarrierCount = 1;
    g_clients[id].ps.stats[STAT_TCE_WEAPON_FLAGS] |= 0x100;
    g_clients[id].tceBombPossessionOrder = 1;
}
static void recover(int id, gentity_t *drop)
{
    drop->active = qtrue;
    Touch_Item(drop, &g_entities[id], NULL);
    check(level.tceBombCarrier == id && level.tceBombCarrierCount == 1 &&
          g_clients[id].ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x100,
          "actual Touch_Item/Pickup_Ammo restores single carrier");
    check(drop->freeAfterEvent && !drop->r.contents &&
          (drop->r.svFlags & SVF_NOCLIENT) && !active_bombs(),
          "actual Touch_Item consumes and hides dropped objective");
    drop->inuse = qfalse; ++deletes; /* engine event completion service only */
}
static void repeated_death(void)
{
    int i, before;
    reset(); connect_player(0); assign(0);
    for (i = 0; i < 8; ++i) {
        before = launches;
        TossClientItems(&g_entities[0]);
        check(launches == before + 1 && level.tceBombDropCount == i + 1,
              "actual TossClientItems drops on each of eight deaths");
        check(level.tceBombCarrier == -1 && !level.tceBombCarrierCount &&
              !(g_clients[0].ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x100) &&
              !g_clients[0].tceBombPossessionOrder && active_bombs() == 1,
              "death releases carrier once and retains one ground bomb");
        TossClientItems(&g_entities[0]);
        check(launches == before + 1, "repeat death dispatch cannot duplicate objective");
        /* Revived human state comes from native ClientSpawn in live test;
         * this focused fixture starts the next pickup with the same client. */
        if (launches == before + 1) recover(0, &g_entities[64 + before]);
        else assign(0); /* continue baseline counterexamples without bad access */
    }
}
static void repeated_disconnect(void)
{
    int i, before;
    reset(); connect_player(0); assign(0);
    for (i = 0; i < 8; ++i) {
        before = launches;
        ClientDisconnect(0);
        check(launches == before + 1 && level.tceBombDropCount == i + 1,
              "actual ClientDisconnect drops on each of eight carrier disconnects");
        check(g_clients[0].pers.connected == CON_DISCONNECTED &&
              !g_entities[0].inuse && level.tceBombCarrier == -1 &&
              !level.tceBombCarrierCount && active_bombs() == 1,
              "disconnect removes client and preserves single unowned objective");
        if (launches == before + 1) {
            check(g_entities[64 + before].s.effect2Time == i + 1,
                  "disconnect retains legacy drop ordinal metadata");
            connect_player(0); recover(0, &g_entities[64 + before]);
        } else { connect_player(0); assign(0); }
    }
}
int main(void)
{
    gentity_t *drop;
    repeated_death(); repeated_disconnect();
    reset(); connect_player(0); assign(0); TossClientItems(&g_entities[0]);
    drop = &g_entities[64];
    g_clients[0].pers.cmd.buttons = 0; drop->active = qtrue;
    Touch_Item(drop, &g_entities[0], NULL);
    check(level.tceBombCarrier == -1 && active_bombs() == 1,
          "ground bomb cannot be recovered without ordinary activate input");
    g_clients[0].pers.cmd.buttons = BUTTON_ACTIVATE;
    g_clients[0].ps.persistant[PERS_TEAM] = TEAM_ALLIES; drop->active = qtrue;
    Touch_Item(drop, &g_entities[0], NULL);
    check(level.tceBombCarrier == -1 && active_bombs() == 1,
          "actual BG_CanItemBeGrabbed rejects opposing-team pickup");
    reset(); connect_player(0); assign(0); g_clients[0].pers.connected = CON_CONNECTING;
    ClientDisconnect(0);
    check(!launches, "actual disconnect gate rejects incompletely connected client");
    reset(); connect_player(0); assign(0); g_clients[0].sess.sessionTeam = TEAM_SPECTATOR;
    ClientDisconnect(0);
    check(!launches, "actual disconnect gate rejects spectator client");
    reset(); connect_player(0); assign(0); g_clients[0].ps.pm_flags |= PMF_FOLLOW;
    ClientDisconnect(0);
    check(!launches, "actual disconnect gate rejects following client");
    reset(); connect_player(0); assign(0); level.tceBombPlanted = qtrue;
    TossClientItems(&g_entities[0]); ClientDisconnect(0);
    check(!launches, "planted bomb is never duplicated by death or disconnect");
    reset(); connect_player(0); assign(0); g_gametype.integer = 7;
    TossClientItems(&g_entities[0]); ClientDisconnect(0);
    check(!launches, "other gametypes do not create ROBJ drops");
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
