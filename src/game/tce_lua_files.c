/* Bounded server Lua access to the engine virtual filesystem. */
#include "g_local.h"
#include "lua.h"
#include "lauxlib.h"

#define TCE_LUA_FILE_HANDLES 32
#define TCE_LUA_FILE_IO_MAX (1024 * 1024)
#define TCE_LUA_FILE_LIST_MAX (64 * 1024)
#define TCE_LUA_FILES_META "tce.files"

typedef struct {
    fileHandle_t handle;
    int mode, remaining;
} tceLuaFile;
typedef struct { tceLuaFile files[TCE_LUA_FILE_HANDLES]; } tceLuaFiles;
static char filesRegistryKey;

static tceLuaFiles *Files(lua_State *L) {
    tceLuaFiles *files;
    lua_rawgetp(L, LUA_REGISTRYINDEX, &filesRegistryKey);
    files = (tceLuaFiles *)luaL_checkudata(L, -1, TCE_LUA_FILES_META);
    lua_pop(L, 1);
    return files;
}

static const char *Path(lua_State *L, int arg, int allowEmpty) {
    size_t size, i;
    const char *path = luaL_checklstring(L, arg, &size);
    luaL_argcheck(L, (allowEmpty || size > 0) && size < MAX_QPATH,
                  arg, "invalid VFS path length");
    luaL_argcheck(L, strlen(path) == size && path[0] != '/' &&
                  !strstr(path, "..") && !strchr(path, '\\') &&
                  !strchr(path, ':') && !strchr(path, '$'),
                  arg, "expected relative VFS path without traversal");
    for (i = 0; i < size; ++i)
        luaL_argcheck(L, (unsigned char)path[i] >= 32 && path[i] != 127,
                      arg, "control character in VFS path");
    return path;
}

static tceLuaFile *File(lua_State *L, int arg) {
    lua_Integer handle = luaL_checkinteger(L, arg);
    tceLuaFiles *files = Files(L);
    int i;
    for (i = 0; i < TCE_LUA_FILE_HANDLES; ++i)
        if (files->files[i].handle && handle == files->files[i].handle)
            return &files->files[i];
    luaL_argerror(L, arg, "file handle is not open in this Lua VM");
    return NULL;
}

static int Length(lua_State *L, int arg) {
    lua_Integer length = luaL_checkinteger(L, arg);
    luaL_argcheck(L, length >= 0 && length <= TCE_LUA_FILE_IO_MAX,
                  arg, "length must be between 0 and 1048576 bytes");
    return (int)length;
}

static int CloseAll(lua_State *L) {
    tceLuaFiles *files = (tceLuaFiles *)luaL_checkudata(L, 1, TCE_LUA_FILES_META);
    int i;
    for (i = 0; i < TCE_LUA_FILE_HANDLES; ++i) {
        if (files->files[i].handle) trap_FS_FCloseFile(files->files[i].handle);
        memset(&files->files[i], 0, sizeof(files->files[i]));
    }
    return 0;
}

static int Open(lua_State *L) {
    const char *path = Path(L, 1, 0);
    lua_Integer mode = luaL_checkinteger(L, 2);
    tceLuaFiles *files = Files(L);
    tceLuaFile *file = NULL;
    int i, length;
    luaL_argcheck(L, mode == FS_READ || mode == FS_WRITE || mode == FS_APPEND,
                  2, "unsupported filesystem mode");
    for (i = 0; i < TCE_LUA_FILE_HANDLES; ++i)
        if (!files->files[i].handle) { file = &files->files[i]; break; }
    if (!file) return luaL_error(L, "Lua VM file handle limit reached (32)");
    length = trap_FS_FOpenFile(path, &file->handle, (fsMode_t)mode);
    if (length < 0 || !file->handle) {
        if (file->handle) trap_FS_FCloseFile(file->handle);
        memset(file, 0, sizeof(*file));
        lua_pushinteger(L, 0);
        lua_pushinteger(L, -1);
    } else {
        file->mode = (int)mode;
        file->remaining = length;
        lua_pushinteger(L, file->handle);
        lua_pushinteger(L, length);
    }
    return 2;
}

static int Read(lua_State *L) {
    tceLuaFile *file = File(L, 1);
    int length = Length(L, 2);
    char *buffer;
    luaL_argcheck(L, file->mode == FS_READ, 1, "file is not open for reading");
    if (length > file->remaining) length = file->remaining;
    if (!length) { lua_pushliteral(L, ""); return 1; }
    /* Lua owns temporary storage even if string allocation raises an error.
     * The engine read trap has no byte-count return; cap at open-time EOF. */
    buffer = (char *)lua_newuserdatauv(L, (size_t)length, 0);
    memset(buffer, 0, (size_t)length);
    trap_FS_Read(buffer, length, file->handle);
    file->remaining -= length;
    lua_pushlstring(L, buffer, (size_t)length);
    return 1;
}

static int Write(lua_State *L) {
    size_t size;
    const char *data = luaL_checklstring(L, 1, &size);
    int length = Length(L, 2);
    tceLuaFile *file = File(L, 3);
    luaL_argcheck(L, (size_t)length <= size, 2, "length exceeds string size");
    luaL_argcheck(L, file->mode == FS_WRITE || file->mode == FS_APPEND,
                  3, "file is not open for writing");
    lua_pushinteger(L, length ? trap_FS_Write(data, length, file->handle) : 0);
    return 1;
}

static int Close(lua_State *L) {
    tceLuaFile *file = File(L, 1);
    trap_FS_FCloseFile(file->handle);
    memset(file, 0, sizeof(*file));
    return 0;
}

static int FileList(lua_State *L) {
    const char *path = Path(L, 1, 1);
    size_t extensionSize, offset = 0;
    const char *extension = luaL_checklstring(L, 2, &extensionSize);
    char *buffer;
    int count, i;
    luaL_argcheck(L, extensionSize < MAX_QPATH && strlen(extension) == extensionSize &&
                  !strstr(extension, "..") && !strchr(extension, ':') &&
                  !strchr(extension, '\\') &&
                  (!strchr(extension, '/') || !strcmp(extension, "/")),
                  2, "invalid file extension");
    buffer = (char *)lua_newuserdatauv(L, TCE_LUA_FILE_LIST_MAX, 0);
    memset(buffer, 0, TCE_LUA_FILE_LIST_MAX);
    count = trap_FS_GetFileList(path, extension, buffer, TCE_LUA_FILE_LIST_MAX);
    lua_newtable(L);
    for (i = 0; i < count && offset < TCE_LUA_FILE_LIST_MAX; ++i) {
        const char *end = (const char *)memchr(buffer + offset, 0,
                                             TCE_LUA_FILE_LIST_MAX - offset);
        size_t length;
        if (!end || end == buffer + offset) break;
        length = (size_t)(end - (buffer + offset));
        lua_pushlstring(L, buffer + offset, length);
        lua_rawseti(L, -2, i + 1);
        offset += length + 1;
    }
    return 1;
}

void TCE_LuaRegisterFiles(lua_State *L) {
    static const luaL_Reg api[] = {
        { "trap_FS_FOpenFile", Open }, { "trap_FS_Read", Read },
        { "trap_FS_Write", Write }, { "trap_FS_FCloseFile", Close },
        { "trap_FS_GetFileList", FileList }, { "FS_GetFileList", FileList },
        { NULL, NULL }
    };
    tceLuaFiles *files;
    lua_rawgetp(L, LUA_REGISTRYINDEX, &filesRegistryKey);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        files = (tceLuaFiles *)lua_newuserdatauv(L, sizeof(*files), 0);
        memset(files, 0, sizeof(*files));
        if (luaL_newmetatable(L, TCE_LUA_FILES_META)) {
            lua_pushcfunction(L, CloseAll);
            lua_setfield(L, -2, "__gc");
        }
        lua_setmetatable(L, -2);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &filesRegistryKey);
    } else lua_pop(L, 1);
    luaL_setfuncs(L, api, 0);
    lua_pushinteger(L, FS_READ); lua_setfield(L, -2, "FS_READ");
    lua_pushinteger(L, FS_WRITE); lua_setfield(L, -2, "FS_WRITE");
    lua_pushinteger(L, FS_APPEND); lua_setfield(L, -2, "FS_APPEND");
}
