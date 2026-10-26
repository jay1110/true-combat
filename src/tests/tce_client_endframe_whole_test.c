#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_active.c"
#include <windows.h>
#include <stdio.h>
extern void P_WorldEffects(gentity_t*),P_DamageFeedback(gentity_t*),G_SetClientSound(gentity_t*),SpectatorClientEndFrame(gentity_t*),WolfReviveBbox(gentity_t*);
extern qboolean StuckInClient(gentity_t*);
static byte *base,rawEnt[0x57c],rawClient[0x16b0],rawPopups[2][0x57c];
static gentity_t ent,popups[2];static gclient_t client;static int side,variant,count,npopup,bad;
typedef struct{int kind,a,b;char text[128];}event;static event events[32];
typedef struct{int count,npopup;event events[32];int fields[64],powerups[16],popup[2][3];float flags[3];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void note(int k,int a,int b,const char*s){event*e;if(count>=32)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=k;e->a=a;e->b=b;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static void check(void*p){if(p!=(side?(void*)&ent:(void*)rawEnt))bad=1;}
static void limboHook(void*p,int corpse){check(p);note(1,corpse,0,NULL);if(variant&1){if(side)client.ps.pm_flags|=PMF_LIMBO;else*(int*)(rawClient+0xc)|=0x4000;}}
static void spectator(void*p){check(p);note(2,0,0,NULL);}
static void world(void*p){check(p);note(3,0,0,NULL);if(variant&2){if(side)ent.health=-1;else*(int*)(rawEnt+0x2c8)=-1;}}
static void damage(void*p){check(p);note(4,0,0,NULL);}
static void bomb(void*p){check(p);note(5,0,0,NULL);if(side)level.tceBombDropCount++;else(*(int*)(base+0x2223934))++;}
static void vip(void*p){check(p);note(6,0,0,NULL);}
static void*popup(int t){int i=npopup++;if(i>=2)exit(3);note(7,t,0,NULL);return side?(void*)&popups[i]:(void*)rawPopups[i];}
static void QDECL print(const char*f,...){va_list a;char s[128];va_start(a,f);Q_vsnprintf(s,sizeof(s),f,a);va_end(a);note(8,0,0,s);}
static void QDECL debug(const char*f,...){va_list a;char s[128];va_start(a,f);Q_vsnprintf(s,sizeof(s),f,a);va_end(a);note(9,0,0,s);}
static void sound(void*p){check(p);note(10,0,0,NULL);}
static void project(void*c,void*p,int snap){check(p);if(c!=(side?(void*)&client:(void*)rawClient))bad=1;note(11,snap,0,NULL);}
static void extrapolate(void*c,void*p,int time,int snap){check(p);if(c!=(side?(void*)&client:(void*)rawClient))bad=1;note(12,time,snap,NULL);}
static int stuck(void*p){check(p);note(13,0,0,NULL);if(variant&4){if(side)ent.health=0;else*(int*)(rawEnt+0x2c8)=0;}return variant&8;}
static void revive(void*p){check(p);note(14,0,0,NULL);}
static void touch(void*p){check(p);note(15,0,0,NULL);}static int script(void*p){check(p);note(16,0,0,NULL);return 1;}static void storePosition(void*p){check(p);note(17,0,0,NULL);}
static void capture(result*r){int k=0,i;memset(r,0,sizeof(*r));r->count=count;r->npopup=npopup;memcpy(r->events,events,sizeof(events));
#define C(field,off) r->fields[k++]=side?client.field:*(int*)(rawClient+off)
#define E(field,off) r->fields[k++]=side?ent.field:*(int*)(rawEnt+off)
#define L(field,off) r->fields[k++]=side?level.field:*(int*)(base+off)
 C(ps.stats[6],0xe8);C(ps.stats[STAT_HEALTH],0xd0);C(ps.stats[STAT_TCE_WEAPON_FLAGS],0xf0);C(ps.stats[STAT_TCE_FLAGS],0xf4);C(ps.pm_flags,0xc);C(airOutTime,0xe60);C(inactivityTime,0xe54);C(lastBurnTime,0xfa4);C(pers.connectTime,0x628);C(pers.enterTime,0x624);C(ps.classWeaponTime,0x478);C(tceObjectiveActivityUntil,0x1314);C(tceBombPossessionOrder,0x1328);E(health,0x2c8);E(s.eFlags,8);E(lastHintCheckTime,0x46c);E(pain_debounce_time,0x2c0);E(s.onFireEnd,0x100);E(props_frame_state,0x378);E(r.contents,0x14c);E(count2,0x3a4);L(tceBombCarrier,0x222391c);L(tceBombDropCount,0x2223934);L(tceBombCarrierCount,0x2223938);L(tceVipCarrier,0x2223948);
 r->flags[0]=side?client.pers.teamState.lastreturnedflag:*(float*)(rawClient+0x654);r->flags[1]=side?client.pers.teamState.lasthurtcarrier:*(float*)(rawClient+0x650);r->flags[2]=side?client.pers.teamState.lastfraggedcarrier:*(float*)(rawClient+0x65c);
 for(i=0;i<16;i++)r->powerups[i]=side?client.ps.powerups[i]:*(int*)(rawClient+0x150+4*i);
 for(i=0;i<npopup;i++){r->popup[i][0]=side?popups[i].s.effect2Time:*(int*)(rawPopups[i]+0x110);r->popup[i][1]=side?popups[i].s.effect3Time:*(int*)(rawPopups[i]+0x114);r->popup[i][2]=side?popups[i].s.density:*(int*)(rawPopups[i]+0xf4);}
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*);result a,b;int mode,h,v,i,k,n=0;int modes[]={2,5,7},health[]={-176,-175,-174,-1,0,1,50,100};int ages[]={999,1000,1001,4999,5000,5001};float skills[]={-.75f,.75f,1.5f,16777216.f,1.f,-16777216.f,2.75f};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x47530);
#define H(rva,fn,repl) if(!hook(base+rva,repl)||!hook(fn,repl))return 2
 H(0x4b410,limbo,limboHook);H(0x46a70,SpectatorClientEndFrame,spectator);H(0x448f0,P_WorldEffects,world);H(0x447c0,P_DamageFeedback,damage);H(0x5cfe0,G_TCEDropBomb,bomb);H(0x5d220,G_TCEDropVip,vip);H(0x95050,G_PopupMessage,popup);H(0x5ed80,G_Printf,print);H(0x5edc0,G_DPrintf,debug);H(0x44af0,G_SetClientSound,sound);H(0x2d350,BG_PlayerStateToEntityState,project);H(0x2d7b0,BG_PlayerStateToEntityStateExtraPolate,extrapolate);H(0x46fa0,StuckInClient,stuck);H(0x47280,WolfReviveBbox,revive);H(0x45060,G_TouchTriggers,touch);H(0x7d4e0,G_Script_ScriptRun,script);H(0x47e30,G_TCEStoreFramePosition,storePosition);
 for(mode=0;mode<3;mode++)for(h=0;h<8;h++)for(v=0;v<8192;v++)for(side=0;side<2;side++){
  variant=v;count=npopup=bad=0;memset(events,0,sizeof(events));memset(&ent,0,sizeof(ent));memset(&client,0,sizeof(client));memset(rawEnt,0,sizeof(rawEnt));memset(rawClient,0,sizeof(rawClient));memset(popups,0,sizeof(popups));memset(rawPopups,0,sizeof(rawPopups));ent.client=&client;*(void**)(rawEnt+0x190)=rawClient;
#define SC(field,off,val) client.field=*(int*)(rawClient+off)=val
#define SE(field,off,val) ent.field=*(int*)(rawEnt+off)=val
#define SL(field,off,val) level.field=*(int*)(base+off)=val
  g_gametype.integer=*(int*)(base+0x6d8d4c)=modes[mode];g_smoothClients.integer=*(int*)(base+0x7fe04c)=v&1;
  SL(time,0x7febe0,10000);SL(previousTime,0x7febe4,10000-(v%3==0?-1:v%3==1?0:100));SL(match_pause,0x221bafc,v&2);SL(intermissiontime,0x7ffc60,v%17==0?1:0);
  SE(health,0x2c8,health[h]);SE(r.svFlags,0x128,v&4?SVF_BOT:0);SC(ps.pm_time,0x10,v%3-1);SC(sess.sessionTeam,0xb30,v%5==0?3:1);SC(ps.pm_flags,0xc,v%7==0?PMF_LIMBO:0);SC(ps.clientNum,0xa0,3);SC(ps.persistant[PERS_TEAM],0x11c,2);SC(ps.stats[STAT_TCE_WEAPON_FLAGS],0xf0,v&16?0x100:0);SC(ps.stats[STAT_TCE_FLAGS],0xf4,v&32?0x100:0);
  SC(lastCmdTime,0xe04,10000-ages[v%6]);SC(tceObjectiveActivityUntil,0x1314,10000+(v%3-1));SC(tceBombPossessionOrder,0x1328,5);SC(buttons,0xe08,v&64?BUTTON_ATTACK:0);
  SL(tceBombAssigned,0x2223918,v&128);SL(tceBombPlanted,0x2223914,v&256);SL(tceBombCarrier,0x222391c,v&512?3:2);SL(tceBombDropCount,0x2223934,v%3);SL(tceBombCarrierCount,0x2223938,4);SL(tceVipAssigned,0x2223944,v&1024);SL(tceVipCarrier,0x2223948,v&2048?3:2);
  SE(s.eFlags,8,0x400|(v&4096?EF_MOUNTEDTANK:0));SE(props_frame_state,0x378,v%3-1);SE(s.effect3Time,0x114,10000-(99+v%3));SE(r.contents,0x14c,v&8?CONTENTS_CORPSE:CONTENTS_BODY);SE(count2,0x3a4,7);SE(lastHintCheckTime,0x46c,123);SE(pain_debounce_time,0x2c0,234);SE(s.onFireEnd,0x100,345);
  SC(airOutTime,0xe60,1234);SC(inactivityTime,0xe54,2345);SC(lastBurnTime,0xfa4,3456);SC(pers.connectTime,0x628,4567);SC(pers.enterTime,0x624,5678);SC(ps.classWeaponTime,0x478,6789);client.pers.teamState.lastreturnedflag=*(float*)(rawClient+0x654)=123.5f;client.pers.teamState.lasthurtcarrier=*(float*)(rawClient+0x650)=234.75f;client.pers.teamState.lastfraggedcarrier=*(float*)(rawClient+0x65c)=345.25f;strcpy(client.pers.netname,"Player");strcpy((char*)rawClient+0x5f8,"Player");
  for(i=0;i<16;i++){int ts[]={0,9999,10000,10001,INT_MAX};client.ps.powerups[i]=*(int*)(rawClient+0x150+i*4)=ts[(v+i)%5];}
  for(i=0;i<7;i++)client.sess.skillpoints[i]=*(float*)(rawClient+0xb68+i*4)=skills[(i+v)%7];
  if(side)ClientEndFrame(&ent);else original(rawEnt);capture(&b);
  if(!side)a=b;else{if(bad||memcmp(&a,&b,sizeof(a))){printf("ClientEndFrame mismatch gt%d health%d case%d bad%d calls%d/%d\n",modes[mode],health[h],v,bad,a.count,b.count);for(k=0;k<64;k++)if(a.fields[k]!=b.fields[k])printf("field%d %d/%d\n",k,a.fields[k],b.fields[k]);for(k=0;k<16;k++)if(a.powerups[k]!=b.powerups[k])printf("powerup%d %d/%d\n",k,a.powerups[k],b.powerups[k]);for(k=0;k<a.count||k<b.count;k++)if(memcmp(&a.events[k],&b.events[k],sizeof(event)))printf("event%d %d,%d,%d[%s] /%d,%d,%d[%s]\n",k,a.events[k].kind,a.events[k].a,a.events[k].b,a.events[k].text,b.events[k].kind,b.events[k].a,b.events[k].b,b.events[k].text);return 1;}n++;}
 }
 printf("PASS ClientEndFrame %d whole-controller cases (external dependencies controlled)\n",n);return 0;
}
