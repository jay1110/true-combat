/* Text-only modules from the engine VFS; no native DLL search or shell access. */
#include "g_local.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdlib.h>

extern int luaopen_luasql_sqlite3(lua_State *L);
static char loadingMarker;

static int Require(lua_State *L) {
    size_t n, i; char path[MAX_QPATH]; fileHandle_t f; int size, status;
    char *source; const char *name = luaL_checklstring(L, 1, &n);
    luaL_argcheck(L, n > 0 && n + 5 < sizeof(path) && !memchr(name, 0, n)
        && !strstr(name, "..") && name[0] != '/', 1, "invalid module name");
    for(i = 0; i < n; ++i) {
        unsigned char c = name[i];
        luaL_argcheck(L, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/' || c == '.',
            1, "invalid module name");
        path[i] = c == '.' ? '/' : c;
    }
    path[n] = 0;
    lua_getfield(L, LUA_REGISTRYINDEX, "tce.modules");
    lua_getfield(L, -1, path);
    if(lua_touserdata(L, -1) == &loadingMarker)
        return luaL_error(L, "circular require: %s", name);
    if(!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);
    lua_pushlightuserdata(L, &loadingMarker); lua_setfield(L, -2, path);
    if(!strcmp(path, "luasql/sqlite3")) {
        lua_pushcfunction(L, luaopen_luasql_sqlite3);
    } else {
        char filename[MAX_QPATH]; Com_sprintf(filename, sizeof(filename), "%s.lua", path);
        size = trap_FS_FOpenFile(filename, &f, FS_READ);
        if(size < 0 || !f || size > 1024*1024) {
            if(f) trap_FS_FCloseFile(f);
            lua_pushnil(L); lua_setfield(L, -2, path);
            return luaL_error(L, "module missing or exceeds 1 MiB: %s", filename);
        }
        source = malloc((size_t)size + 1);
        if(!source) { trap_FS_FCloseFile(f); return luaL_error(L, "module allocation failed"); }
        memset(source, 0, (size_t)size + 1);
        trap_FS_Read(source, size, f); trap_FS_FCloseFile(f);
        status = luaL_loadbufferx(L, source, size, filename, "t"); free(source);
        if(status != LUA_OK) {
            lua_pushnil(L); lua_setfield(L, -3, path); return lua_error(L);
        }
    }
    status = lua_pcall(L, 0, 1, 0);
    if(status != LUA_OK) {
        lua_pushnil(L); lua_setfield(L, -3, path); return lua_error(L);
    }
    if(lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushboolean(L, 1); }
    lua_pushvalue(L, -1); lua_setfield(L, -3, path);
    return 1;
}

void TCE_LuaRegisterModules(lua_State *L) {
    static const char *allowed[] = {"time", "date", "difftime", "clock", NULL};
    int i;
    lua_newtable(L); lua_setfield(L, LUA_REGISTRYINDEX, "tce.modules");
    lua_pushcfunction(L, Require); lua_setglobal(L, "require");
    /* Copy only clock/calendar operations, never publish the full os library. */
    luaopen_os(L); lua_newtable(L);
    for(i = 0; allowed[i]; ++i) {
        lua_getfield(L, -2, allowed[i]); lua_setfield(L, -2, allowed[i]);
    }
    lua_setglobal(L, "os"); lua_pop(L, 1);
}
