#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
extern qboolean UI_RegisterClientModelname(playerInfo_t*,const char*);
extern void TCE_UI_PlayerInfo_SetWeapon(playerInfo_t*,int);
static char trace[8192];static int original,weaponSeed,mode,team,cls,weapon,prevTeam,prevClass,prevWeapon;
static float value(const char*n){sprintf(trace+strlen(trace),"V:%s;",n);if(!strcmp(n,"ui_Q3Model"))return mode;if(!strcmp(n,"mp_team"))return team;if(!strcmp(n,"mp_playerType"))return cls;if(!strcmp(n,"mp_weapon"))return weapon;if(!strcmp(n,"ui_prevTeam"))return prevTeam;if(!strcmp(n,"ui_prevClass"))return prevClass;return prevWeapon;}
static char*string(const char*n){sprintf(trace+strlen(trace),"S:%s;",n);return !strcmp(n,"team_model")?"multi_custom":"unused";}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"SET:%s:%s;",n,v);}
static qboolean registerModel(playerInfo_t*p,const char*n){sprintf(trace+strlen(trace),"MODEL:%s;",n);if(original)*(int*)((byte*)p+0x5810)=weaponSeed;else p->weapon=weaponSeed;return qtrue;}
static void setWeapon(playerInfo_t*p,int w){sprintf(trace+strlen(trace),"WEAPON:%d;",w);}
static void info(playerInfo_t*p,int legs,int torso,vec3_t view,vec3_t move,weapon_t w,qboolean chat){sprintf(trace+strlen(trace),"INFO:%d:%d:%.9g:%.9g:%d:%d;",legs,torso,view[1],move[1],w,chat);if(original)memcpy((byte*)p+0x57f8,move,12);else VectorCopy(move,p->moveAngles);}
static void draw(float x,float y,float w,float h,playerInfo_t*p,int time){float*mv=original?(float*)((byte*)p+0x57f8):p->moveAngles;sprintf(trace+strlen(trace),"DRAW:%.9g:%.9g:%.9g:%.9g:%.9g:%d;",x,y,w,h,mv[1],time);}
static struct{unsigned char*p,bytes[5];}hooks[14];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<14);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE lib;byte*b;int i,cases=0;char expected[8192];rectDef_t rect={3.13f,7.17f,83.2f,121.3f};playerInfo_t pi;byte raw[0x5870];
assert(argc==2);lib=LoadLibraryA(argv[1]);assert(lib);b=(byte*)lib;
patch(b+0x13860,registerModel);patch(UI_RegisterClientModelname,registerModel);patch(b+0x13c30,setWeapon);patch(TCE_UI_PlayerInfo_SetWeapon,setWeapon);patch(b+0x20550,value);patch(trap_Cvar_VariableValue,value);patch(b+0x23e0,string);patch(UI_Cvar_VariableString,string);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x14440,info);patch(UI_PlayerInfo_SetInfo,info);patch(b+0x12a60,draw);patch(UI_DrawPlayer,draw);
for(i=0;i<64;++i){weaponSeed=i;memset(&pi,0x35,sizeof(pi));memset(raw,0x35,sizeof(raw));trace[0]=0;original=1;((void(__cdecl*)(void*,const char*))(b+0x143e0))(raw,"multi");strcpy(expected,trace);trace[0]=0;original=0;UI_PlayerInfo_SetModel(&pi,"multi");assert(!strcmp(expected,trace));assert(pi.currentWeapon==*(int*)(raw+0x5804)&&pi.lastWeapon==*(int*)(raw+0x5814)&&pi.pendingWeapon==*(int*)(raw+0x5818)&&pi.weaponTimer==*(int*)(raw+0x581c)&&pi.chat==*(int*)(raw+0x5830)&&pi.newModel==*(int*)(raw+0x5834));++cases;}
q3Model=*(int*)(b+0x63f38)=0;updateModel=*(int*)(b+0x33204)=1;
for(i=0;i<512;++i){mode=(i/8)%2;team=(i/4)%2;cls=i%5;weapon=i%64;weaponSeed=weapon;prevTeam=(i&1)?team:3;prevClass=(i&2)?cls:7;prevWeapon=(i&4)?weapon:99;uiInfo.uiDC.realTime=*(int*)(b+0x292140)=i*31-100;trace[0]=0;original=1;((void(__cdecl*)(rectDef_t*))(b+0xfb40))(&rect);strcpy(expected,trace);trace[0]=0;original=0;UI_DrawPlayerModel(&rect);if(strcmp(expected,trace)){printf("model controller mismatch %d\nO%s\nS%s\n",i,expected,trace);return 1;}assert(q3Model==*(int*)(b+0x63f38)&&updateModel==*(int*)(b+0x33204));++cases;}
restore();assert(FreeLibrary(lib));printf("%d original player model-controller comparisons passed\n",cases);return 0;}
