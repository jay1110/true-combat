/* LuaSQL host policy. Database files belong to the active mod's home directory.
 * Scripts and filesystem administrators are trusted; this lexical path policy
 * is not a sandbox against administrator-created symlinks/junctions. */
#include "g_local.h"
#include "lua.h"
#include "lauxlib.h"
#include "sqlite3.h"

static int SqlPath(char *out, size_t size, const char *in) {
    size_t i, start = 0, len = strlen(in);
    if (!len || len >= size) return 0;
    for (i = 0; i <= len; ++i) {
        char c = in[i] == '\\' ? '/' : in[i];
        if ((unsigned char)c < 32 && c) return 0;
        out[i] = c;
        if (!c || c == '/') {
            size_t n = i - start;
            if ((n == 1 && out[start] == '.') ||
                (n == 2 && out[start] == '.' && out[start + 1] == '.')) return 0;
            start = i + 1;
        }
    }
    while (len > 1 && out[len - 1] == '/') out[--len] = 0;
    return 1;
}

static int SqlAuthorize(void *unused, int operation, const char *arg1,
                        const char *arg2, const char *database, const char *trigger) {
    (void)unused; (void)arg2; (void)database; (void)trigger;
    /* ATTACH also guards VACUUM INTO. Temp databases remain memory-only. */
    if (operation == SQLITE_ATTACH) return SQLITE_DENY;
    if (operation == SQLITE_PRAGMA && arg1 &&
        (!Q_stricmp(arg1, "temp_store_directory") ||
         !Q_stricmp(arg1, "data_store_directory"))) return SQLITE_DENY;
    return SQLITE_OK;
}

int TCE_LuaSqlOpen(lua_State *L, const char *source, sqlite3 **connection, int flags) {
    char home[MAX_OSPATH], game[MAX_QPATH], root[MAX_OSPATH * 2];
    char path[MAX_OSPATH * 4], normalized[MAX_OSPATH * 4];
    size_t rootlen;
    int result;
    *connection = NULL;
    if (!strcmp(source, ":memory:")) {
        result = sqlite3_open_v2(source, connection,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, NULL);
    } else {
        trap_Cvar_VariableStringBuffer("fs_homepath", home, sizeof(home));
        trap_Cvar_VariableStringBuffer("fs_game", game, sizeof(game));
        if (!home[0] || !game[0]) return luaL_error(L, "LuaSQL: fs_homepath/fs_game unavailable");
        Com_sprintf(root, sizeof(root), "%s/%s", home, game);
        if (!SqlPath(normalized, sizeof(normalized), root) ||
            !SqlPath(path, sizeof(path), source))
            return luaL_error(L, "LuaSQL: invalid database path");
        Q_strncpyz(root, normalized, sizeof(root));
        rootlen = strlen(root);
        if (path[0] == '/' || strchr(path, ':')) {
#ifdef _WIN32
            if (Q_stricmpn(path, root, (int)rootlen) || path[rootlen] != '/')
#else
            if (strncmp(path, root, rootlen) || path[rootlen] != '/')
#endif
                return luaL_error(L, "LuaSQL: database must be under fs_homepath/fs_game");
            Q_strncpyz(normalized, path, sizeof(normalized));
        } else {
            if (rootlen + strlen(path) + 2 > sizeof(normalized))
                return luaL_error(L, "LuaSQL: database path too long");
            Com_sprintf(normalized, sizeof(normalized), "%s/%s", root, path);
        }
        /* No URI filenames, alternate NTFS streams, or wildcard characters. */
        if (strpbrk(normalized + rootlen + 1, ":?*<>|\""))
            return luaL_error(L, "LuaSQL: invalid database filename");
        flags &= SQLITE_OPEN_READONLY | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        result = sqlite3_open_v2(normalized, connection, flags, NULL);
    }
    if (result == SQLITE_OK) {
        sqlite3_set_authorizer(*connection, SqlAuthorize, NULL);
        sqlite3_db_config(*connection, SQLITE_DBCONFIG_DEFENSIVE, 1, NULL);
        sqlite3_busy_timeout(*connection, 1000);
    }
    return result;
}
