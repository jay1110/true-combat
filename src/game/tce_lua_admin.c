/* Narrow server-administration Lua API; all actions use native game paths. */
#include "g_local.h"
#include "lua.h"
#include "lauxlib.h"
#include <limits.h>

extern qboolean IsHeadShotWeapon(int mod);

static int Integer(lua_State *L, int arg, int low, int high) {
    lua_Integer n = luaL_checkinteger(L, arg);
    luaL_argcheck(L, n >= low && n <= high, arg, "integer outside allowed range");
    return (int)n;
}
static const char *Text(lua_State *L, int arg, size_t limit) {
    size_t n;
    const char *s = luaL_checklstring(L, arg, &n);
    luaL_argcheck(L, n < limit && !memchr(s, 0, n), arg, "text too long or contains NUL");
    return s;
}
static int Client(lua_State *L, int arg) {
    int n = Integer(L, arg, 0, level.maxclients - 1);
    luaL_argcheck(L, level.clients[n].pers.connected != CON_DISCONNECTED,
                  arg, "client is disconnected");
    return n;
}
static int ClientNumber(lua_State *L) {
    const char *s = Text(L, 1, MAX_STRING_CHARS);
    char wanted[MAX_STRING_CHARS], name[MAX_NETNAME];
    int i, result = -1, numeric = s[0] != 0, slot = 0;
    for (i = 0; s[i]; ++i) {
        if (s[i] < '0' || s[i] > '9') { numeric = 0; break; }
        if (slot < MAX_CLIENTS) slot = slot * 10 + s[i] - '0';
    }
    if (numeric) {
        if (slot < level.maxclients && level.clients[slot].pers.connected == CON_CONNECTED)
            result = slot;
    } else {
        Q_strncpyz(wanted, s, sizeof(wanted)); Q_CleanStr(wanted);
        if (wanted[0]) for (i = 0; i < level.maxclients; ++i) {
            if (level.clients[i].pers.connected != CON_CONNECTED) continue;
            Q_strncpyz(name, level.clients[i].pers.netname, sizeof(name)); Q_CleanStr(name);
            if (!Q_stricmp(name, wanted)) {
                if (result != -1) { result = -1; break; }
                result = i;
            }
        }
    }
    lua_pushinteger(L, result);
    return 1;
}
static int Drop(lua_State *L) {
    int client = Client(L, 1);
    const char *reason = Text(L, 2, MAX_STRING_CHARS);
    int seconds = lua_isnoneornil(L, 3) ? 0 : Integer(L, 3, 0, INT_MAX);
    trap_DropClient(client, reason, seconds);
    return 0;
}
static int Log(lua_State *L) {
    G_LogPrintf("%s", Text(L, 1, MAX_STRING_CHARS));
    return 0;
}
static int UserinfoChanged(lua_State *L) {
    ClientUserinfoChanged(Client(L, 1));
    return 0;
}
static gentity_t *Source(lua_State *L, int arg) {
    int n = Integer(L, arg, -1, MAX_GENTITIES);
    /* WolfAdmin uses 1024 for world damage. Never index that sentinel. */
    if (n == -1 || n == MAX_GENTITIES || n == ENTITYNUM_NONE || n == ENTITYNUM_WORLD)
        return NULL;
    luaL_argcheck(L, n < level.num_entities && g_entities[n].inuse,
                  arg, "damage source entity is not in use");
    return &g_entities[n];
}
static int Damage(lua_State *L) {
    int target = Client(L, 1);
    gentity_t *inflictor = Source(L, 2), *attacker = Source(L, 3);
    int amount = Integer(L, 4, 0, 10000);
    int flags = Integer(L, 5, 0, 127);
    int mod = Integer(L, 6, 0, MOD_NUM_MODS - 1);
    luaL_argcheck(L, g_entities[target].inuse &&
                  level.clients[target].pers.connected == CON_CONNECTED,
                  1, "connected player entity required");
    luaL_argcheck(L, !(flags & ~(DAMAGE_RADIUS | DAMAGE_HALF_KNOCKBACK |
                  DAMAGE_NO_KNOCKBACK | DAMAGE_NO_PROTECTION |
                  DAMAGE_NO_TEAM_PROTECTION | DAMAGE_DISTANCEFALLOFF)),
                  5, "unsupported damage flags");
    /* TC bullet damage requires a real ray for body-volume intersection;
     * the six-argument administrative API carries no direction or point. */
    luaL_argcheck(L, !IsHeadShotWeapon(mod), 6,
                  "bullet damage requires native hit geometry");
    G_Damage(&g_entities[target], inflictor, attacker, NULL, NULL, amount, flags, mod);
    return 0;
}
void TCE_LuaRegisterAdmin(lua_State *L) {
    static const luaL_Reg api[] = {
        { "ClientNumberFromString", ClientNumber }, { "trap_DropClient", Drop },
        { "G_LogPrint", Log }, { "ClientUserinfoChanged", UserinfoChanged },
        { "G_Damage", Damage }, { NULL, NULL }
    };
    luaL_setfuncs(L, api, 0);
}
