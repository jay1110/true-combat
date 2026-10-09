/* Whitelisted server Lua entity access. Native lifecycle functions own mutations. */
#include "g_local.h"
#include "lua.h"
#include "lauxlib.h"

static int Integer(lua_State *L, int arg, int lo, int hi) {
    lua_Integer n = luaL_checkinteger(L, arg);
    luaL_argcheck(L, n >= lo && n <= hi, arg, "integer outside allowed range");
    return (int)n;
}
static const char *Text(lua_State *L, int arg, size_t limit) {
    size_t n; const char *s = luaL_checklstring(L, arg, &n);
    luaL_argcheck(L, n < limit && !memchr(s, 0, n), arg, "text too long or contains NUL");
    return s;
}
static gentity_t *Entity(lua_State *L) {
    int n = Integer(L, 1, 0, MAX_GENTITIES - 1);
    luaL_argcheck(L, n < level.num_entities && g_entities[n].inuse, 1, "entity is not in use");
    return &g_entities[n];
}
static gclient_t *Client(lua_State *L, gentity_t *e) {
    luaL_argcheck(L, e->client && (e - g_entities) < level.maxclients &&
        e->client->pers.connected == CON_CONNECTED, 1, "connected player required");
    return e->client;
}
static void PushVector(lua_State *L, const vec3_t v) {
    int i; lua_createtable(L, 3, 0);
    for(i = 0; i < 3; ++i) { lua_pushnumber(L, v[i]); lua_rawseti(L, -2, i + 1); }
}
static void Vector(lua_State *L, int arg, vec3_t v) {
    int i; luaL_checktype(L, arg, LUA_TTABLE);
    for(i = 0; i < 3; ++i) {
        lua_Number n; lua_rawgeti(L, arg, i + 1); n = luaL_checknumber(L, -1);
        luaL_argcheck(L, n == n && n >= -1000000.0 && n <= 1000000.0, arg, "finite vector components within +/-1000000 required");
        v[i] = (float)n; lua_pop(L, 1);
    }
}
static int Get(lua_State *L) {
    gentity_t *e; gclient_t *c; const char *f = Text(L, 2, 128);
    /* Player enumeration must work before spawn and for unused slots. */
    if(!strcmp(f, "pers.connected") || !strcmp(f, "pers.netname")) {
        int n = Integer(L, 1, 0, MAX_CLIENTS - 1);
        c = n < level.maxclients ? &level.clients[n] : NULL;
        if(!strcmp(f, "pers.connected")) lua_pushinteger(L, c ? c->pers.connected : CON_DISCONNECTED);
        else if(!c || c->pers.connected == CON_DISCONNECTED) lua_pushnil(L);
        else lua_pushstring(L, c->pers.netname);
        return 1;
    }
    e = Entity(L);
    if(!strcmp(f, "health")) lua_pushinteger(L, e->health);
    else if(!strcmp(f, "classname")) lua_pushstring(L, e->classname ? e->classname : "");
    else if(!strcmp(f, "targetname")) lua_pushstring(L, e->targetname ? e->targetname : "");
    else if(!strcmp(f, "r.currentOrigin")) PushVector(L, e->r.currentOrigin);
    else if(!strcmp(f, "s.origin")) PushVector(L, e->s.origin);
    else if(!strcmp(f, "s.number")) lua_pushinteger(L, e->s.number);
    else {
        luaL_argcheck(L, e->client != NULL, 1, "player field requires client entity"); c = e->client;
        if(!strcmp(f, "pers.netname")) lua_pushstring(L, c->pers.netname);
        else if(!strcmp(f, "pers.connected")) lua_pushinteger(L, c->pers.connected);
        else if(!strcmp(f, "sess.sessionTeam")) lua_pushinteger(L, c->sess.sessionTeam);
        else if(!strcmp(f, "sess.playerType")) lua_pushinteger(L, c->sess.playerType);
        else if(!strcmp(f, "sess.muted")) lua_pushinteger(L, c->sess.muted);
        else if(!strcmp(f, "sess.referee")) lua_pushinteger(L, c->sess.referee);
        else if(!strcmp(f, "sess.spec_invite")) lua_pushinteger(L, c->sess.spec_invite);
        else if(!strcmp(f, "sess.kills")) lua_pushinteger(L, c->sess.kills);
        else if(!strcmp(f, "sess.team_kills")) lua_pushinteger(L, c->sess.team_kills);
        else if(!strcmp(f, "sess.damage_given")) lua_pushinteger(L, c->sess.damage_given);
        else if(!strcmp(f, "sess.damage_received")) lua_pushinteger(L, c->sess.damage_received);
        else if(!strcmp(f, "sess.deaths")) lua_pushinteger(L, c->sess.deaths);
        else if(!strcmp(f, "sess.team_damage")) lua_pushinteger(L, c->sess.team_damage);
        else if(!strcmp(f, "sess.suicides")) lua_pushinteger(L, c->sess.suicides);
        else if(!strcmp(f, "ps.ping")) lua_pushinteger(L, c->ps.ping);
        else if(!strcmp(f, "ps.weapon")) lua_pushinteger(L, c->ps.weapon);
        else if(!strcmp(f, "ps.origin")) PushVector(L, c->ps.origin);
        else if(!strcmp(f, "ps.viewangles")) PushVector(L, c->ps.viewangles);
        else if(!strcmp(f, "ps.ammo")) lua_pushinteger(L, c->ps.ammo[Integer(L, 3, 0, MAX_WEAPONS - 1)]);
        else if(!strcmp(f, "ps.ammoclip")) lua_pushinteger(L, c->ps.ammoclip[Integer(L, 3, 0, MAX_WEAPONS - 1)]);
        else if(!strcmp(f, "ps.stats")) lua_pushinteger(L, c->ps.stats[Integer(L, 3, 0, MAX_STATS - 1)]);
        else if(!strcmp(f, "ps.powerups")) lua_pushinteger(L, c->ps.powerups[Integer(L, 3, 0, MAX_POWERUPS - 1)]);
        else if(!strcmp(f, "ps.persistant")) lua_pushinteger(L, c->ps.persistant[Integer(L, 3, 0, MAX_PERSISTANT - 1)]);
        else return luaL_error(L, "unsupported gentity field: %s", f);
    }
    return 1;
}
static int Set(lua_State *L) {
    gentity_t *e = Entity(L); gclient_t *c; vec3_t v;
    const char *f = Text(L, 2, 128); int argc = lua_gettop(L), valueArg = 3;
    if(!strcmp(f, "ps.ammo") || !strcmp(f, "ps.ammoclip")) {
        int index, value;
        luaL_argcheck(L, argc == 4, 3, "array setter requires index and value");
        c = Client(L, e); index = Integer(L, 3, 0, MAX_WEAPONS - 1); value = Integer(L, 4, 0, 999);
        if(!strcmp(f, "ps.ammo")) c->ps.ammo[index] = value; else c->ps.ammoclip[index] = value;
    } else {
        luaL_argcheck(L, argc == 3 || (argc == 4 && Integer(L, 3, 0, 0) == 0), 3, "scalar setter accepts value or zero index and value");
        if(argc == 4) valueArg = 4;
        if(!strcmp(f, "sess.muted")) {
            c = Client(L, e); c->sess.muted = (qboolean)Integer(L, valueArg, 0, 1);
            ClientUserinfoChanged((int)(e - g_entities));
        } else if(!strcmp(f, "health")) {
            int health = Integer(L, valueArg, 1, 9999);
            if(e->client) {
                c = Client(L, e);
                luaL_argcheck(L, e->health > 0 && c->ps.pm_type != PM_DEAD, 1, "cannot resurrect dead player with health setter");
                c->ps.stats[STAT_HEALTH] = health;
            }
            e->health = health;
        } else if(!strcmp(f, "ps.origin") || !strcmp(f, "r.currentOrigin")) {
            c = Client(L, e); Vector(L, valueArg, v);
            luaL_argcheck(L, e->health > 0 && c->ps.pm_type != PM_DEAD, 1, "cannot teleport dead player");
            TeleportPlayer(e, v, c->ps.viewangles);
        } else if(!strcmp(f, "ps.viewangles")) {
            Client(L, e); Vector(L, valueArg, v); SetClientViewAngle(e, v);
        } else return luaL_error(L, "readonly or unsupported gentity field: %s", f);
    }
    return 0;
}
static int ConfigGet(lua_State *L) {
    char b[MAX_STRING_CHARS]; int n = Integer(L, 1, 0, MAX_CONFIGSTRINGS - 1);
    trap_GetConfigstring(n, b, sizeof(b)); lua_pushstring(L, b); return 1;
}
static int ConfigSet(lua_State *L) {
    int n = Integer(L, 1, 0, MAX_CONFIGSTRINGS - 1);
    trap_SetConfigstring(n, Text(L, 2, MAX_STRING_CHARS)); return 0;
}
static int UserinfoSet(lua_State *L) {
    gentity_t *e = Entity(L); const char *s = Text(L, 2, MAX_INFO_STRING);
    Client(L, e); luaL_argcheck(L, Info_Validate(s), 2, "invalid info string");
    trap_SetUserinfo((int)(e - g_entities), s); ClientUserinfoChanged((int)(e - g_entities)); return 0;
}
static int InfoSet(lua_State *L) {
    char b[MAX_INFO_STRING]; const char *s = Text(L, 1, sizeof(b));
    const char *key = Text(L, 2, MAX_INFO_KEY), *value = Text(L, 3, MAX_INFO_VALUE);
    luaL_argcheck(L, Info_Validate(s), 1, "invalid info string");
    luaL_argcheck(L, key[0] && !strpbrk(key, "\\;\""), 2, "invalid info key");
    luaL_argcheck(L, !strpbrk(value, "\\;\""), 3, "invalid info value");
    Q_strncpyz(b, s, sizeof(b)); Info_RemoveKey(b, key);
    luaL_argcheck(L, !value[0] || strlen(b) + strlen(key) + strlen(value) + 2 < sizeof(b), 3, "info string capacity exceeded");
    Info_SetValueForKey(b, key, value); lua_pushstring(L, b); return 1;
}
static int Team(lua_State *L) {
    gentity_t *e = Entity(L); char team[16]; const char *s = Text(L, 2, sizeof(team));
    Client(L, e);
    luaL_argcheck(L, !Q_stricmp(s, "axis") || !Q_stricmp(s, "allies") || !Q_stricmp(s, "spectator") || !Q_stricmp(s, "red") || !Q_stricmp(s, "blue") || !Q_stricmp(s, "s"), 2, "team must be axis, allies or spectator (red/blue/s aliases)");
    Q_strncpyz(team, s, sizeof(team)); lua_pushboolean(L, SetTeam(e, team, qfalse, -1, -1, -1, qfalse)); return 1;
}
void TCE_LuaRegisterEntities(lua_State *L) {
    static const luaL_Reg api[] = {
        {"gentity_get", Get}, {"gentity_set", Set}, {"trap_GetConfigstring", ConfigGet},
        {"trap_SetConfigstring", ConfigSet}, {"trap_SetUserinfo", UserinfoSet},
        {"Info_SetValueForKey", InfoSet}, {"G_SetTeam", Team}, {NULL, NULL}
    };
    luaL_setfuncs(L, api, 0);
}
