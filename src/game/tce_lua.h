#ifndef TCE_LUA_H
#define TCE_LUA_H
#ifdef FEATURE_LUA
void TCE_LuaInit(int time, int seed, int restart);
void TCE_LuaShutdown(int restart);
void TCE_LuaRunFrame(int time);
void TCE_LuaClientEvent(const char *event, int clientNum);
const char *TCE_LuaClientConnect(int clientNum, qboolean first, qboolean bot);
qboolean TCE_LuaCommand(int clientNum, qboolean console);
void TCE_LuaSpawn(int clientNum, int revived, int hostage);
qboolean TCE_LuaDamage(int target, int attacker, int damage, int dflags, int mod);
void TCE_LuaDeath(int victim, int killer, int mod);
qboolean TCE_LuaWeaponFire(int clientNum, int weapon);
const char *TCE_LuaChat(int sender, int receiver, const char *text, char *buffer, int size);
#else
#define TCE_LuaInit(t,s,r) ((void)0)
#define TCE_LuaShutdown(r) ((void)0)
#define TCE_LuaRunFrame(t) ((void)0)
#define TCE_LuaClientEvent(e,c) ((void)0)
#define TCE_LuaClientConnect(c,f,b) ((const char *)0)
#define TCE_LuaCommand(c,s) qfalse
#define TCE_LuaSpawn(c,r,h) ((void)0)
#define TCE_LuaDamage(t,a,d,f,m) qfalse
#define TCE_LuaDeath(v,k,m) ((void)0)
#define TCE_LuaWeaponFire(c,w) qfalse
#define TCE_LuaChat(s,r,t,b,n) (t)
#endif
#endif
