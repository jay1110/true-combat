/* Server-side Lua extension. et_* callback naming follows the ET mod convention.
 * Independent implementation: deliberately limited API, not full ETLegacy ABI. */
#include "g_local.h"
#include "tce_lua.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdlib.h>

#define TCE_LUA_VMS 16
#define TCE_LUA_SOURCE_MAX (1024*1024)
typedef struct { lua_State *L; char file[MAX_QPATH], name[128]; int active, budget, busy; } tceLuaVM;
extern void TCE_LuaRegisterEntities(lua_State *L);
extern void TCE_LuaRegisterFiles(lua_State *L);
extern void TCE_LuaRegisterModules(lua_State *L);
extern void TCE_LuaRegisterAdmin(lua_State *L);
extern char *TCE_LuaReadSource(const char *path, int *length);
static tceLuaVM vms[TCE_LUA_VMS];
static int luaSeed;
static qboolean reloadPending;
static char rejectReason[1024];
static tceLuaVM *VM(lua_State *L) { return *(tceLuaVM **)lua_getextraspace(L); }
static void Budget(lua_State *L, lua_Debug *ar) {
    (void)ar;
    if (--VM(L)->budget <= 0) luaL_error(L,"callback instruction budget exceeded");
}
static void Error(tceLuaVM *vm,const char *event) {
    const char *msg=lua_tostring(vm->L,-1);
    G_Printf("Lua [%s] %s: %s; module disabled\n",vm->file,event,msg?msg:"non-string error");
    vm->active=0; lua_settop(vm->L,0);
}
static int Call(tceLuaVM *vm,const char *event,int args,int results) {
    int status;
    vm->budget=100; lua_sethook(vm->L,Budget,LUA_MASKCOUNT,10000);
    vm->busy++;
    status=lua_pcall(vm->L,args,results,0);
    vm->busy--;
    if(status!=LUA_OK) { Error(vm,event); return 0; }
    return 1;
}
static int Begin(tceLuaVM *vm,const char *event) {
    /* Native setters may trigger client callbacks synchronously. Never clear
     * the stack of a VM whose callback is still running. Other VMs may listen. */
    if(!vm->L || !vm->active || vm->busy) return 0;
    lua_settop(vm->L,0);lua_getglobal(vm->L,event);
    if(!lua_isfunction(vm->L,-1)) { lua_settop(vm->L,0);return 0; }
    return 1;
}
static int Client(lua_State *L,int arg) {
    int n=(int)luaL_checkinteger(L,arg);
    luaL_argcheck(L,n>=0 && n<level.maxclients,arg,"invalid client slot");return n;
}
static int Print(lua_State *L) { G_Printf("%s",luaL_checkstring(L,1));return 0; }
static int CvarGet(lua_State *L) { char b[MAX_CVAR_VALUE_STRING];trap_Cvar_VariableStringBuffer(luaL_checkstring(L,1),b,sizeof(b));lua_pushstring(L,b);return 1; }
static int CvarSet(lua_State *L) { trap_Cvar_Set(luaL_checkstring(L,1),luaL_checkstring(L,2));return 0; }
static int Argc(lua_State *L) { lua_pushinteger(L,trap_Argc());return 1; }
static int Argv(lua_State *L) { char b[MAX_STRING_CHARS];trap_Argv((int)luaL_checkinteger(L,1),b,sizeof(b));lua_pushstring(L,b);return 1; }
static int Userinfo(lua_State *L) { char b[MAX_INFO_STRING];trap_GetUserinfo(Client(L,1),b,sizeof(b));lua_pushstring(L,b);return 1; }
static int InfoGet(lua_State *L) { lua_pushstring(L,Info_ValueForKey(luaL_checkstring(L,1),luaL_checkstring(L,2)));return 1; }
static int SendServer(lua_State *L) {
    int n=(int)luaL_checkinteger(L,1);const char *s=luaL_checkstring(L,2);
    luaL_argcheck(L,n>=-1&&n<level.maxclients,1,"invalid client slot");
    luaL_argcheck(L,strlen(s)<MAX_STRING_CHARS,2,"command too long");
    trap_SendServerCommand(n,s);return 0;
}
static int SendConsole(lua_State *L) {
    /* Always defer commands: synchronous reload/map calls can destroy this VM. */
    size_t size; const char *text;
    luaL_checkinteger(L,1); text=luaL_checklstring(L,2,&size);
    luaL_argcheck(L,size<MAX_STRING_CHARS && !memchr(text,0,size),2,"invalid console command length");
    trap_SendConsoleCommand(EXEC_APPEND,va("%s\n",text));return 0;
}
static int RegisterName(lua_State *L) { Q_strncpyz(VM(L)->name,luaL_checkstring(L,1),sizeof(VM(L)->name));return 0; }
static int FindSelf(lua_State *L) { lua_pushinteger(L,VM(L)-vms);return 1; }
static int Milliseconds(lua_State *L) { lua_pushinteger(L,trap_Milliseconds());return 1; }
static int EntityGet(lua_State *L) {
    int n=Client(L,1);const char *field=luaL_checkstring(L,2);gclient_t *c=&level.clients[n];
    if(!strcmp(field,"pers.netname")) lua_pushstring(L,c->pers.netname);
    else if(!strcmp(field,"sess.sessionTeam")) lua_pushinteger(L,c->sess.sessionTeam);
    else if(!strcmp(field,"sess.playerType")) lua_pushinteger(L,c->sess.playerType);
    else if(!strcmp(field,"pers.connected")) lua_pushinteger(L,c->pers.connected);
    else if(!strcmp(field,"health")) lua_pushinteger(L,g_entities[n].health);
    else return luaL_error(L,"unsupported gentity field: %s",field);
    return 1;
}
static const luaL_Reg api[]={
 {"G_Print",Print},{"trap_Cvar_Get",CvarGet},{"trap_Cvar_Set",CvarSet},
 {"trap_Argc",Argc},{"trap_Argv",Argv},{"trap_GetUserinfo",Userinfo},
 {"Info_ValueForKey",InfoGet},{"trap_SendServerCommand",SendServer},
 {"trap_SendConsoleCommand",SendConsole},{"RegisterModname",RegisterName},
 {"FindSelf",FindSelf},{"trap_Milliseconds",Milliseconds},{"gentity_get",EntityGet},{NULL,NULL}
};
static void Setup(tceLuaVM *vm) {
    lua_State *L=vm->L;*(tceLuaVM **)lua_getextraspace(L)=vm;
    /* Server-admin scripts. No package, io, shell or debug access. */
    luaL_requiref(L,"_G",luaopen_base,1);lua_pop(L,1);
    luaL_requiref(L,LUA_TABLIBNAME,luaopen_table,1);lua_pop(L,1);
    luaL_requiref(L,LUA_STRLIBNAME,luaopen_string,1);lua_pop(L,1);
    luaL_requiref(L,LUA_MATHLIBNAME,luaopen_math,1);lua_pop(L,1);
    luaL_requiref(L,LUA_UTF8LIBNAME,luaopen_utf8,1);lua_pop(L,1);
    lua_pushnil(L);lua_setglobal(L,"dofile");lua_pushnil(L);lua_setglobal(L,"loadfile");
    TCE_LuaRegisterModules(L);
    lua_pushcfunction(L,Print);lua_setglobal(L,"print");
    luaL_newlib(L,api);
    TCE_LuaRegisterEntities(L);
    TCE_LuaRegisterFiles(L);
    TCE_LuaRegisterAdmin(L);
#define CONST(n) lua_pushinteger(L,n);lua_setfield(L,-2,#n)
    CONST(TEAM_AXIS);CONST(TEAM_ALLIES);CONST(TEAM_SPECTATOR);CONST(EXEC_APPEND);
    CONST(CON_CONNECTED);CONST(CON_CONNECTING);CONST(CON_DISCONNECTED);
    CONST(STAT_HEALTH);CONST(MAX_WEAPONS);CONST(MAX_CLIENTS);CONST(MAX_GENTITIES);
#undef CONST
    lua_pushinteger(L,TEAM_SPECTATOR);lua_setfield(L,-2,"TEAM_SPECTATORS");
#ifdef _WIN32
    lua_pushliteral(L,"windows");
#else
    lua_pushliteral(L,"unix");
#endif
    lua_setfield(L,-2,"PLATFORM");
    lua_pushstring(L,"tce2-lua-3");lua_setfield(L,-2,"API_VERSION");lua_setglobal(L,"et");
}
void TCE_LuaShutdown(int restart) {
    int i;
    reloadPending = qfalse;
    for(i=0;i<TCE_LUA_VMS;i++) {
        tceLuaVM *vm=&vms[i];if(!vm->L)continue;
        if(Begin(vm,"et_ShutdownGame")) { lua_pushinteger(vm->L,restart);Call(vm,"et_ShutdownGame",1,0); }
        lua_close(vm->L);memset(vm,0,sizeof(*vm));
    }
}
void TCE_LuaInit(int time,int seed,int restart) {
    vmCvar_t modules, password, trustGuid;char list[MAX_CVAR_VALUE_STRING],*p,*name;int i=0;
    luaSeed = seed;
    trap_Cvar_Register(&password,"wolfadmin_password","",0);
    trap_Cvar_Register(&trustGuid,"g_wolfadminTrustGuid","0",0);
    trap_Cvar_Register(&modules,"lua_modules","",CVAR_ARCHIVE);
    Q_strncpyz(list,modules.string,sizeof(list));p=list;
    while(*p && i<TCE_LUA_VMS) {
        int size;char *code;tceLuaVM *vm;
        while(*p==' '||*p==';'||*p=='\t')p++;
        if(!*p)break;name=p;while(*p&&*p!=' '&&*p!=';'&&*p!='\t')p++;if(*p)*p++=0;
        if(strlen(name)>=MAX_QPATH || strstr(name,"..") || strchr(name,':') || *name=='/' || strchr(name,'\\')) {
            G_Printf("Lua: invalid module path %s\n",name);continue;
        }
        code=TCE_LuaReadSource(name,&size);
        if(!code) { G_Printf("Lua: cannot load %s (missing, unreadable or over 1 MiB)\n",name);continue; }
        vm=&vms[i++];memset(vm,0,sizeof(*vm));Q_strncpyz(vm->file,name,sizeof(vm->file));vm->L=luaL_newstate();
        if(!vm->L){free(code);continue;}vm->active=1;Setup(vm);
        if(luaL_loadbufferx(vm->L,code,size,name,"t")!=LUA_OK) Error(vm,"load");
        else Call(vm,"load",0,0);
        free(code);
        if(Begin(vm,"et_InitGame")) {lua_pushinteger(vm->L,time);lua_pushinteger(vm->L,seed);lua_pushinteger(vm->L,restart);Call(vm,"et_InitGame",3,0);}
        if(vm->active)G_Printf("Lua: loaded %s (%s)\n",vm->file,vm->name);
    }
}
void TCE_LuaRunFrame(int time) {
    int i, client;
    if (reloadPending) {
        /* Run only after command callbacks have returned; never close a live
         * Lua stack. Keep the native map, clients and Omni-bot intact. */
        TCE_LuaShutdown(1);
        TCE_LuaInit(time,luaSeed,1);
        for(client=0;client<level.maxclients;client++) {
            gclient_t *c = g_entities[client].client;
            if(!c || c->pers.connected == CON_DISCONNECTED) continue;
            for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],"et_LuaClientRestore")) {
                lua_pushinteger(vms[i].L,client);
                lua_pushinteger(vms[i].L,(g_entities[client].r.svFlags & SVF_BOT) != 0);
                lua_pushinteger(vms[i].L,c->pers.connected);
                Call(&vms[i],"et_LuaClientRestore",3,0);
            }
        }
        G_Printf("Lua: restart complete; use lua_status for module state.\n");
    }
    for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],"et_RunFrame")) {lua_pushinteger(vms[i].L,time);Call(&vms[i],"et_RunFrame",1,0);}
}
void TCE_LuaClientEvent(const char *event,int clientNum) {
    int i;for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],event)) {lua_pushinteger(vms[i].L,clientNum);Call(&vms[i],event,1,0);}
}
const char *TCE_LuaClientConnect(int clientNum,qboolean first,qboolean bot) {
    int i;for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],"et_ClientConnect")) {
        tceLuaVM *vm=&vms[i];lua_pushinteger(vm->L,clientNum);lua_pushinteger(vm->L,first);lua_pushinteger(vm->L,bot);
        if(Call(vm,"et_ClientConnect",3,1)&&lua_type(vm->L,-1)==LUA_TSTRING) {
            Q_strncpyz(rejectReason,lua_tostring(vm->L,-1),sizeof(rejectReason));lua_settop(vm->L,0);return rejectReason;
        }lua_settop(vm->L,0);
    }return NULL;
}
static void LuaReply(int clientNum,qboolean console,const char *text) {
    char safe[900]; int i;
    if(console) { G_Printf("%s\n",text); return; }
    Q_strncpyz(safe,text,sizeof(safe));
    for(i=0;safe[i];i++)if((unsigned char)safe[i]<32 || safe[i]=='"' || safe[i]=='\\')safe[i]=' ';
    trap_SendServerCommand(clientNum,va("print \"%s\n\"",safe));
}
qboolean TCE_LuaCommand(int clientNum,qboolean console) {
    char cmd[MAX_TOKEN_CHARS];const char *event=console?"et_ConsoleCommand":"et_ClientCommand";int i;
    trap_Argv(0,cmd,sizeof(cmd));
    if(!Q_stricmp(cmd,"lua_status")) {
        int count=0,active=0;
        for(i=0;i<TCE_LUA_VMS;i++)if(vms[i].L) {
            count++; if(vms[i].active)active++;
            LuaReply(clientNum,console,va("Lua %d: %s %s (%s)",i,vms[i].active?"active":"disabled",vms[i].file,vms[i].name));
        }
        LuaReply(clientNum,console,va("Lua: %d loaded, %d active.%s",count,active,count?"":" Check lua_modules and server load errors; then use lua_restart in server console/RCON."));
        return qtrue;
    }
    if(!Q_stricmp(cmd,"lua_restart")) {
        if(console) {
            reloadPending=qtrue;
            LuaReply(clientNum,console,"Lua: restart queued for the next server frame.");
        } else LuaReply(clientNum,console,"Lua: lua_restart requires the server console or RCON.");
        return qtrue;
    }
    for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],event)) {
        tceLuaVM *vm=&vms[i];int handled=0;if(!console)lua_pushinteger(vm->L,clientNum);lua_pushstring(vm->L,cmd);
        if(Call(vm,event,console?1:2,1))handled=lua_isnumber(vm->L,-1)&&lua_tointeger(vm->L,-1)==1;
        lua_settop(vm->L,0);if(handled)return qtrue;
    }
    if(!console&&!Q_stricmp(cmd,"wolfauth")) {
        LuaReply(clientNum,console,"WolfAdmin is not active on this server. Use lua_status; ask the server owner to check Lua load errors.");
        return qtrue;
    }
    return qfalse;
}

/* Integer event payloads are bounded native game values. A numeric 1 cancels
 * only the explicitly cancellable events; notification returns are ignored. */
static qboolean IntegerEvent(const char *event,const int *values,int count,qboolean cancellable) {
    int i,j;
    for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],event)) {
        tceLuaVM *vm=&vms[i];int handled=0;
        for(j=0;j<count;j++)lua_pushinteger(vm->L,values[j]);
        if(Call(vm,event,count,cancellable?1:0)&&cancellable)
            handled=lua_isnumber(vm->L,-1)&&lua_tointeger(vm->L,-1)==1;
        lua_settop(vm->L,0);
        if(handled)return qtrue;
    }
    return qfalse;
}
void TCE_LuaSpawn(int clientNum,int revived,int hostage) {
    int args[3]={clientNum,revived,hostage};
    IntegerEvent("et_ClientSpawn",args,3,qfalse);
}
qboolean TCE_LuaDamage(int target,int attacker,int damage,int dflags,int mod) {
    int args[5]={target,attacker,damage,dflags,mod};
    return IntegerEvent("et_Damage",args,5,qtrue);
}
void TCE_LuaDeath(int victim,int killer,int mod) {
    int args[3]={victim,killer,mod};
    IntegerEvent("et_Obituary",args,3,qfalse);
}
qboolean TCE_LuaWeaponFire(int clientNum,int weapon) {
    int args[2]={clientNum,weapon};
    return IntegerEvent("et_WeaponFire",args,2,qtrue);
}
const char *TCE_LuaChat(int sender,int receiver,const char *text,char *buffer,int size) {
    int i;const char *current=text;
    if(size<1)return text;
    for(i=0;i<TCE_LUA_VMS;i++)if(Begin(&vms[i],"et_Chat")) {
        tceLuaVM *vm=&vms[i];lua_State *L=vm->L;
        lua_pushinteger(L,sender);lua_pushinteger(L,receiver);lua_pushstring(L,current);
        if(Call(vm,"et_Chat",3,1)) {
            if(lua_isboolean(L,-1)&&!lua_toboolean(L,-1)) {lua_settop(L,0);return NULL;}
            if(lua_type(L,-1)==LUA_TSTRING) {
                size_t len,k;const char *s=lua_tolstring(L,-1,&len);
                if(len>=(size_t)size)len=(size_t)size-1;
                /* G_SayTo embeds text inside a quoted engine command. */
                for(k=0;k<len;k++) {
                    unsigned char c=(unsigned char)s[k];
                    buffer[k]=(c<32||c==127)?' ':c=='"'?'\'':c=='\\'?'/':(char)c;
                }
                buffer[len]=0;current=buffer;
            }
        }
        lua_settop(L,0);
    }
    return current;
}
