#ifndef TCE_LUA_H
#define TCE_LUA_H
#ifdef FEATURE_LUA
void TCE_LuaInit(int time, int seed, int restart);
void TCE_LuaShutdown(int restart);
void TCE_LuaRunFrame(int time);
void TCE_LuaClientEvent(const char *event, int clientNum);
const char *TCE_LuaClientConnect(int clientNum, qboolean first, qboolean bot);
qboolean TCE_LuaCommand(int clientNum, qboolean console);
#else
#define TCE_LuaInit(t,s,r) ((void)0)
#define TCE_LuaShutdown(r) ((void)0)
#define TCE_LuaRunFrame(t) ((void)0)
#define TCE_LuaClientEvent(e,c) ((void)0)
#define TCE_LuaClientConnect(c,f,b) ((const char *)0)
#define TCE_LuaCommand(c,s) qfalse
#endif
#endif
