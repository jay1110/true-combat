/* Bounded server Lua access to the engine virtual filesystem. */
#include "g_local.h"
#include "lua.h"
#include "lauxlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static qboolean LocalDirectory(const char *rootName,const char *path,char *full,int capacity) {
    char game[MAX_QPATH],root[MAX_OSPATH];
    trap_Cvar_VariableStringBuffer("fs_game",game,sizeof(game));
    trap_Cvar_VariableStringBuffer(rootName,root,sizeof(root));
    if(!root[0] || !game[0] || strstr(game,"..") || strchr(game,'/') || strchr(game,'\\') || strchr(game,':') ||
       strlen(root)+strlen(game)+strlen(path)+3 >= (size_t)capacity) return qfalse;
    Com_sprintf(full,capacity,"%s/%s/%s",root,game,path);
    return qtrue;
}

static void AddListedFile(lua_State *L,const char *name,const char *extension,int directory,size_t *bytes) {
    size_t n=strlen(name), e=strlen(extension), i, count;
    if(!strcmp(name,".") || !strcmp(name,"..") || *bytes+n+1>65536) return;
    if(!strcmp(extension,"/")) { if(!directory)return; }
    else if(directory || (e && (n<e || Q_stricmp(name+n-e,extension))))return;
    count=lua_rawlen(L,-1);
    for(i=1;i<=count;i++) {
        int same;
        lua_rawgeti(L,-1,i); same=!Q_stricmp(lua_tostring(L,-1),name);lua_pop(L,1);
        if(same)return;
    }
    lua_pushstring(L,name);lua_rawseti(L,-2,count+1);*bytes+=n+1;
}

static void ListLocal(lua_State *L,const char *path,const char *extension,size_t *bytes) {
    const char *roots[]={"fs_homepath","fs_basepath"};
    char full[MAX_OSPATH*2]; int i;
    for(i=0;i<2;i++) {
        if(!LocalDirectory(roots[i],path,full,sizeof(full)-3))continue;
#ifdef _WIN32
        {
            struct _finddata_t entry; intptr_t search;
            Q_strcat(full,sizeof(full),"/*");search=_findfirst(full,&entry);
            if(search == -1)continue;
            do { AddListedFile(L,entry.name,extension,(entry.attrib&_A_SUBDIR)!=0,bytes); } while(!_findnext(search,&entry));
            _findclose(search);
        }
#else
        {
            DIR *dir=opendir(full);struct dirent *entry;char child[MAX_OSPATH*2];struct stat st;
            if(!dir)continue;
            while((entry=readdir(dir))!=NULL) {
                if(strlen(full)+strlen(entry->d_name)+2>sizeof(child))continue;
                Com_sprintf(child,sizeof(child),"%s/%s",full,entry->d_name);
                if(!stat(child,&st))AddListedFile(L,entry->d_name,extension,S_ISDIR(st.st_mode),bytes);
            }
            closedir(dir);
        }
#endif
    }
}

/* A listen server shares the engine's pure-client VFS. Server-owned loose
 * scripts/data must remain readable without packaging them for clients. */
static FILE *LocalRead(const char *path, int *length) {
    const char *roots[] = { "fs_homepath", "fs_basepath" };
    char game[MAX_QPATH], root[MAX_OSPATH], full[MAX_OSPATH * 2];
    FILE *f; long size; int i;
    if (!path[0] || strlen(path) >= MAX_QPATH || path[0] == '/' ||
        strstr(path, "..") || strchr(path, '\\') || strchr(path, ':')) return NULL;
    for (i = 0; path[i]; ++i) if ((unsigned char)path[i] < 32 || path[i] == 127) return NULL;
    trap_Cvar_VariableStringBuffer("fs_game", game, sizeof(game));
    if (!game[0] || strstr(game, "..") || strchr(game, '/') ||
        strchr(game, '\\') || strchr(game, ':')) return NULL;
    for (i = 0; i < 2; ++i) {
        trap_Cvar_VariableStringBuffer(roots[i], root, sizeof(root));
        if (!root[0] || strlen(root) + strlen(game) + strlen(path) + 3 > sizeof(full)) continue;
        Com_sprintf(full, sizeof(full), "%s/%s/%s", root, game, path);
        f = fopen(full, "rb");
        if (!f) continue;
        if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || size > INT_MAX || fseek(f, 0, SEEK_SET)) {
            fclose(f); return NULL;
        }
        *length = (int)size; return f;
    }
    return NULL;
}

/* Caller owns the returned buffer. No binary Lua chunks are accepted by loaders. */
char *TCE_LuaReadSource(const char *path, int *length) {
    FILE *local; fileHandle_t handle = 0; char *data; int size = -1;
    local = LocalRead(path, &size);
    if (!local) size = trap_FS_FOpenFile(path, &handle, FS_READ);
    if ((!local && !handle) || size < 0 || size > 1024 * 1024) {
        if (local) fclose(local);
        if (handle) trap_FS_FCloseFile(handle);
        return NULL;
    }
    data = (char *)malloc((size_t)size + 1);
    if (data) {
        if (local) {
            if (fread(data, 1, size, local) != (size_t)size) { free(data); data = NULL; }
        } else trap_FS_Read(data, size, handle);
        if (data) { data[size] = 0; *length = size; }
    }
    if (local) fclose(local);
    if (handle) trap_FS_FCloseFile(handle);
    return data;
}

#define TCE_LUA_FILE_HANDLES 32
#define TCE_LUA_FILE_IO_MAX (1024 * 1024)
#define TCE_LUA_FILE_LIST_MAX (64 * 1024)
#define TCE_LUA_FILES_META "tce.files"

typedef struct {
    fileHandle_t handle;
    FILE *local;
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
        if ((files->files[i].handle || files->files[i].local) && handle == i + 1)
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
        if (files->files[i].local) fclose(files->files[i].local);
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
        if (!files->files[i].handle && !files->files[i].local) { file = &files->files[i]; break; }
    if (!file) return luaL_error(L, "Lua VM file handle limit reached (32)");
    length = -1;
    if (mode == FS_READ) file->local = LocalRead(path, &length);
    if (!file->local) length = trap_FS_FOpenFile(path, &file->handle, (fsMode_t)mode);
    if (length < 0 || (!file->handle && !file->local)) {
        if (file->handle) trap_FS_FCloseFile(file->handle);
        memset(file, 0, sizeof(*file));
        lua_pushinteger(L, 0);
        lua_pushinteger(L, -1);
    } else {
        file->mode = (int)mode;
        file->remaining = length;
        lua_pushinteger(L, i + 1);
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
    if (file->local) length = (int)fread(buffer, 1, length, file->local);
    else trap_FS_Read(buffer, length, file->handle);
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
    if (file->local) fclose(file->local);
    else trap_FS_FCloseFile(file->handle);
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
    ListLocal(L,path,extension,&offset);
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
