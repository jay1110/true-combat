#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern gentity_t *SelectSpectatorSpawnPoint(vec3_t,vec3_t);
extern void SetClientViewAnglePitch(gentity_t*,float);
extern void BotInitMovementAutonomyPos(gentity_t*);
extern void BotSpeedBonus(int);
static byte *base,rawClients[4][0x16b0];static gclient_t clients[4];
static int side,variant,number,count,bad,selectCount,disableCount,randomValue;
static gentity_t spawns[2];static byte rawSpawns[2][0x57c];
static bg_playerclass_t classInfo;static byte rawClass[128];
typedef struct{int kind,a,b,c,d;float xyz[6];char text[64];}call_t;
typedef struct{playerState_t ps;clientPersistant_t pers;int fields[80];float bounds[12];entityState_t es;char classname[16];call_t calls[64];int count;}result_t;
static call_t calls[64];
static void *ent(void){return side?(void*)&g_entities[number]:(void*)(base+0x226e2a0+number*0x57c);}
static void *client(void){return side?(void*)&clients[number]:(void*)rawClients[number];}
static void check(void*p){if(p!=ent())bad=1;}
static void note(int k,int a,int b,int c,int d,const char*s){call_t*p;if(count==64)exit(3);p=&calls[count++];memset(p,0,sizeof(*p));p->kind=k;p->a=a;p->b=b;p->c=c;p->d=d;if(s)Q_strncpyz(p->text,s,sizeof(p->text));}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void counts(void){note(1,0,0,0,0,NULL);if(variant&1){if(side)clients[number].tceLastSpawnPoint=&spawns[1];else*(void**)(rawClients[number]+0x1338)=rawSpawns[1];}}
static void *selectSpawn(int team,int state,float*o,float*a,int objective,int preferred,int hostage){int i=selectCount++%2;note(2,team,state,objective,preferred,hostage?"hostage":"normal");o[0]=10+i;o[1]=20+i;o[2]=30+i;a[0]=40+i;a[1]=50+i;a[2]=60+i;if(preferred&&(variant&2)&&!hostage)return NULL;return side?(void*)&spawns[i]:(void*)rawSpawns[i];}
static void *spectator(float*o,float*a){note(3,0,0,0,0,NULL);o[0]=101;o[1]=102;o[2]=103;a[0]=104;a[1]=105;a[2]=106;return side?(void*)&spawns[1]:(void*)rawSpawns[1];}
static void userinfo(int n,char*s,int len){int crouch;memcpy(&crouch,side?(void*)&clients[number].ps.crouchMaxZ:(void*)(rawClients[number]+0x3f4),4);note(4,n,len,crouch,0,NULL);if(variant&4)Q_strncpyz(s,"\\cg_toggleCrouch\\1\\cg_toggleAiming\\-1\\cg_freeAim\\2",len);else Q_strncpyz(s,"\\cg_toggleCrouch\\0\\cg_toggleAiming\\0\\cg_freeAim\\0",len);}
static void cmd(int n,usercmd_t*c){note(5,n,0,0,0,NULL);memset(c,0,sizeof(*c));c->weapon=17;c->serverTime=99;}
static qboolean disabled(void*p,int weapon){check(p);note(6,weapon,disableCount,0,0,NULL);return (variant>>(3+disableCount++))&1;}
static void*getClass(void*p){if(p!=client())bad=1;note(7,0,0,0,0,NULL);return side?(void*)&classInfo:(void*)rawClass;}
static void changed(int n){note(8,n,0,0,0,NULL);}
static void character(void*p){if(p!=client())bad=1;note(9,0,0,0,0,NULL);}
static void ranks(void){note(10,0,0,0,0,NULL);}
static void equipment(void*p){if(p!=client())bad=1;note(11,0,0,0,0,NULL);if(side){clients[number].ps.weapon=39;clients[number].ps.weapAnim=513;}else{*(int*)(rawClients[number]+0xa4)=39;*(int*)(rawClients[number]+0x480)=513;}}
static int rnd(void){note(12,randomValue,0,0,0,NULL);return randomValue;}
static void origin(void*p,float*o){check(p);note(13,0,0,0,0,NULL);memcpy(calls[count-1].xyz,o,12);if(side)memcpy(g_entities[number].s.pos.trBase,o,12);else memcpy((byte*)p+0x18,o,12);}
static void view(void*p,float*a){check(p);note(14,0,0,0,0,NULL);memcpy(calls[count-1].xyz,a,12);if(side)memcpy(clients[number].ps.viewangles,a,12);else memcpy(rawClients[number]+0xb0,a,12);}
static void pitch(void*p,float a){check(p);note(15,0,0,0,0,NULL);calls[count-1].xyz[0]=a;if(side)clients[number].ps.viewangles[0]=a;else*(float*)(rawClients[number]+0xb0)=a;}
static void ideal(int n,float*a){note(16,n,0,0,0,NULL);memcpy(calls[count-1].xyz,a,12);}
static void autonomy(void*p){check(p);note(17,0,0,0,0,NULL);}
static void link(void*p){check(p);note(18,0,0,0,0,NULL);}
static void intermission(void*p){check(p);note(19,0,0,0,0,NULL);}
static int spawnIndex(void*p){int i;if(!p)return -1;if(p==ent())return 2;for(i=0;i<2;i++)if(p==(side?(void*)&spawns[i]:(void*)rawSpawns[i]))return i;bad=1;return -2;}
static void targets(void*p,void*a){check(a);note(20,spawnIndex(p),0,0,0,NULL);}
static void think(int n){note(21,n,0,0,0,NULL);if(variant&32){if(side)clients[number].sess.sessionTeam=3;else*(int*)(rawClients[number]+0xb30)=3;}if(side)clients[number].ps.origin[2]+=2;else*(float*)(rawClients[number]+0x1c)+=2;}
static void convert(void*p,void*e,int snap){if(p!=client()||e!=ent())bad=1;note(22,snap,0,0,0,NULL);}
static void endframe(void*p){check(p);note(23,0,0,0,0,NULL);}
static int idle(int weapon){note(24,weapon,0,0,0,NULL);return 7;}
static void markers(void*p){check(p);note(25,0,0,0,0,NULL);}
static void speed(int n){note(26,n,0,0,0,NULL);}
static int scriptInit(int n){note(27,n,0,0,0,NULL);return 1;}
static void botEvent(int n,char*event,char*param){note(28,n,0,0,0,event);Q_strncpyz(calls[count-1].text+32,param,32);}
static void script(void*p,char*event,char*param){check(p);note(29,0,0,0,0,event);Q_strncpyz(calls[count-1].text+32,param,32);}
static void capture(result_t*r){int k=0;gclient_t*c=&clients[number];gentity_t*e=&g_entities[number];byte*q=rawClients[number],*p=ent();memset(r,0,sizeof(*r));memcpy(&r->ps,side?(void*)&c->ps:(void*)q,sizeof(r->ps));memcpy(&r->pers,side?(void*)&c->pers:(void*)(q+0x5ac),sizeof(r->pers));memcpy(&r->es,side?(void*)&e->s:(void*)p,sizeof(r->es));
#define C(field,off) r->fields[k++]=side?c->field:*(int*)(q+off)
#define E(field,off) r->fields[k++]=side?e->field:*(int*)(p+off)
 C(sess.sessionTeam,0xb30);C(sess.playerType,0xb40);C(sess.playerWeapon,0xb44);C(sess.playerWeapon2,0xb48);C(sess.spawnObjectiveIndex,0xb4c);C(sess.latchPlayerType,0xb50);C(sess.latchPlayerWeapon,0xb54);C(sess.latchPlayerWeapon2,0xb58);C(sess.playerWeapon3,0xde4);C(sess.latchPlayerWeapon3,0xde8);C(sess.tceSessionValues[1],0xdf0);C(maxlivescalced,0x12f8);C(botSlotNumber,0x1224);C(airOutTime,0xe60);C(pmext.sprintTime,0x1248);C(pmext.bAutoReload,0x1238);C(tceObjectiveActivityUntil,0x1314);C(tceDamageSpawnTime,0x1334);C(respawnTime,0xe50);C(tceRespawnNotBefore,0x1324);C(inactivityTime,0xe54);C(latched_buttons,0xe10);C(latched_wbuttons,0xe1c);C(deathTime,0x12ac);
 r->fields[k++]=spawnIndex(side?(void*)c->tceLastSpawnPoint:*(void**)(q+0x1338));E(takedamage,0x2cc);E(inuse,0x194);E(r.contents,0x14c);E(clipmask,0x1d4);E(props_frame_state,0x378);E(waterlevel,0x304);E(watertype,0x300);E(flags,0x1b0);E(health,0x2c8);r->fields[k++]=side?(e->client==&clients[number]):(*(void**)(p+0x190)==q);r->fields[k++]=side?(e->die==player_die):(*(void**)(p+0x2bc)==base+0x57570);
 memcpy(r->bounds,side?(void*)e->r.mins:(void*)(p+0x134),24);memcpy(r->bounds+6,side?(void*)e->r.currentOrigin:(void*)(p+0x168),12);Q_strncpyz(r->classname,side?e->classname:*(char**)(p+0x1a4),sizeof(r->classname));memcpy(r->calls,calls,sizeof(calls));r->count=count;
}
/* This contract deliberately differs from the original in exactly8 PS bytes.
 * Compare the complete prefix/suffix and assert both the original values and
 * the accepted new values. Captured original/source results stay unchanged. */
static int acceptedResultEqual(const result_t *original,const result_t *source){
 const size_t offset=offsetof(playerState_t,holdable[TCE_FREELOOK_PITCH]);
 const byte zero[8]={0};
 if(original->ps.holdable[TCE_FREELOOK_PITCH]!=0||original->ps.holdable[TCE_FREELOOK_YAW]!=0||
    source->ps.holdable[TCE_FREELOOK_PITCH]!=TCE_FREELOOK_CENTER||source->ps.holdable[TCE_FREELOOK_YAW]!=TCE_FREELOOK_CENTER||
    memcmp(clients[number].pmext.tceFreelookRecoil,zero,8))return 0;
 return !memcmp(original,source,offset)&&
        !memcmp((const byte*)original+offset+8,(const byte*)source+offset+8,sizeof(*original)-offset-8);
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*,int,int);result_t a,b;int v,revived,hostage,n=0,i,j,team,gt,state,accepted;gclient_t*c;gentity_t*e;byte*q,*p;
 accepted=argc==3&&!strcmp(argv[2],"--accepted-extensions");
 if((argc!=2&&!accepted)||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x4cd90);
 if(accepted&&(offsetof(playerState_t,holdable[TCE_FREELOOK_PITCH])!=0x3c8||TCE_FREELOOK_YAW!=TCE_FREELOOK_PITCH+1||sizeof(clients[0].pmext.tceFreelookRecoil)!=8)){puts("Accepted freelook layout changed");return 2;}
 if(sizeof(playerState_t)!=0x5ac||sizeof(clientPersistant_t)!=0x584||sizeof(entityState_t)!=0x120){printf("Layout assumptions PS%u pers%u ES%u\n",(unsigned)sizeof(playerState_t),(unsigned)sizeof(clientPersistant_t),(unsigned)sizeof(entityState_t));return 2;}
#define H(rva,source,replacement) if(!hook(base+rva,replacement)||!hook(source,replacement))return 2
 H(0x53d70,G_UpdateSpawnCounts,counts);H(0x8e2c0,SelectCTFSpawnPoint,selectSpawn);H(0x4ad40,SelectSpectatorSpawnPoint,spectator);H(0x8ae00,trap_GetUserinfo,userinfo);H(0x8b040,trap_GetUsercmd,cmd);H(0x50790,G_IsWeaponDisabled,disabled);H(0x2afa0,BG_PlayerClassForPlayerState,getClass);H(0x4bf70,ClientUserinfoChanged,changed);H(0x4a870,G_UpdateCharacter,character);H(0x61b60,CalculateRanks,ranks);H(0x4b9c0,SetWolfSpawnWeapons,equipment);H(0xa20e4,rand,rnd);H(0x951d0,G_SetOrigin,origin);H(0x4b300,SetClientViewAngle,view);H(0x4b3b0,SetClientViewAnglePitch,pitch);H(0x207c0,BotSetIdealViewAngles,ideal);H(0x20800,BotInitMovementAutonomyPos,autonomy);H(0x8afa0,trap_LinkEntity,link);H(0x62130,MoveClientToIntermission,intermission);H(0x94b90,G_UseTargets,targets);H(0x46980,ClientThink,think);H(0x2d350,BG_PlayerStateToEntityState,convert);H(0x47530,ClientEndFrame,endframe);H(0x30a30,PM_IdleAnimForWeapon,idle);H(0x48370,G_TCEResetFrameMarkers,markers);H(0x1b750,BotSpeedBonus,speed);H(0x211a0,Bot_ScriptInitBot,scriptInit);H(0x21780,Bot_ScriptEvent,botEvent);H(0x7d4b0,G_Script_ScriptEvent,script);
 classInfo.classWeapons[0]=33;*(int*)(rawClass+0x10)=33;
 for(v=0;v<4096;v++)for(revived=0;revived<2;revived++)for(hostage=0;hostage<2;hostage++){
  team=v%4;gt=(v/4)%3==0?2:(v/4)%3==1?5:7;state=(v/12)%3;if(hostage&&!revived&&(gt!=5||state==1||state==2))continue;
  for(side=0;side<2;side++){
   variant=v;number=(v/36)%4;randomValue=v*19;count=bad=selectCount=disableCount=0;memset(calls,0,sizeof(calls));memset(clients,0x5a,sizeof(clients));memset(rawClients,0x5a,sizeof(rawClients));memset(spawns,0,sizeof(spawns));memset(rawSpawns,0,sizeof(rawSpawns));c=&clients[number];e=&g_entities[number];q=rawClients[number];p=base+0x226e2a0+number*0x57c;memset(e,0,sizeof(*e));memset(p,0,0x57c);
   level.clients=clients;*(void**)(base+0x7feb80)=rawClients;level.time=*(int*)(base+0x7febe0)=100000;level.timeCurrent=*(int*)(base+0x221bc08)=90000;level.intermissiontime=*(int*)(base+0x7ffc60)=(v/72)%2;
   g_gametype.integer=*(int*)(base+0x6d8d4c)=gt;g_gamestate.integer=*(int*)(base+0x784f6c)=state;g_currentRound.integer=*(int*)(base+0x2223cec)=(v/144)%2;g_fastres.integer=*(int*)(base+0x6d9acc)=(v/288)%2;g_inactivity.integer=*(int*)(base+0x7fe94c)=v%3;g_newbbox.integer=*(int*)(base+0x7e208c)=(v/576)%2;g_realism.integer=*(int*)(base+0x23ce5cc)=(v/1152)%3;
   for(i=0;i<2;i++){spawns[i].s.number=*(int*)rawSpawns[i]=70+i;spawns[i].targetname=(v&64)?"spawn_test":NULL;*(void**)(rawSpawns[i]+0x258)=spawns[i].targetname;}
   memcpy(&c->pers,q+0x5ac,sizeof(c->pers));c->tceLastSpawnPoint=&spawns[0];*(void**)(q+0x1338)=rawSpawns[0];e->s.number=*(int*)p=number;e->client=c;*(void**)(p+0x190)=q;e->r.svFlags=*(int*)(p+0x128)=(v/144)%2?SVF_BOT:0;e->s.eFlags=*(int*)(p+8)=EF_MOUNTEDTANK|8;
#define INIT(field,off,val) c->field=*(int*)(q+off)=(val)
   INIT(sess.sessionTeam,0xb30,team);INIT(sess.playerType,0xb40,v%3);INIT(sess.playerWeapon,0xb44,35);INIT(sess.playerWeapon2,0xb48,2);INIT(sess.spawnObjectiveIndex,0xb4c,7);INIT(sess.latchPlayerType,0xb50,(v/3)%3);INIT(sess.latchPlayerWeapon,0xb54,39);INIT(sess.latchPlayerWeapon2,0xb58,2);INIT(sess.playerWeapon3,0xde4,4);INIT(sess.latchPlayerWeapon3,0xde8,30);INIT(sess.tceSessionValues[1],0xdf0,v%2?71:0);INIT(sess.skill[SK_BATTLE_SENSE],0xba4,v%5);INIT(pers.teamState.state,0x62c,0);INIT(pers.maxHealth,0x620,120);INIT(pers.bAutoReloadAux,0x67c,v%2);INIT(ps.eFlags,0x68,(v%4)*4|EF_VOTED);INIT(ps.ping,0x468,91);INIT(ps.teamNum,0x420,2);INIT(botSlotNumber,0x1224,3);INIT(maxlivescalced,0x12f8,1);
   for(i=0;i<3;i++){e->r.currentOrigin[i]=*(float*)(p+0x168+i*4)=i*10+5;e->s.angles[i]=*(float*)(p+0x74+i*4)=i*20+3;}
   if(side)ClientSpawn(e,revived,hostage);else original(p,revived,hostage);capture(side?&b:&a);
   if(side){if(bad||(accepted?!acceptedResultEqual(&a,&b):memcmp(&a,&b,sizeof(a)))){if(!accepted&&acceptedResultEqual(&a,&b))puts("STRICT_BASELINE_DIFFERENCE ClientSpawn: original holdable14/15=0; source accepted freelook center=8000; remaining result equal");printf("ClientSpawn mismatch case%d revived%d hostage%d team%d gt%d state%d bad%d calls%d/%d\n",v,revived,hostage,team,gt,state,bad,a.count,b.count);for(j=0;j<sizeof(a.ps);j++)if(((byte*)&a.ps)[j]!=((byte*)&b.ps)[j]){printf("PS byte%x %02x/%02x\n",j,((byte*)&a.ps)[j],((byte*)&b.ps)[j]);break;}for(j=0;j<sizeof(a.pers);j++)if(((byte*)&a.pers)[j]!=((byte*)&b.pers)[j]){printf("pers byte%x %02x/%02x\n",j,((byte*)&a.pers)[j],((byte*)&b.pers)[j]);break;}for(j=0;j<80;j++)if(a.fields[j]!=b.fields[j])printf("field%d %d/%d\n",j,a.fields[j],b.fields[j]);for(j=0;j<a.count||j<b.count;j++)if(memcmp(&a.calls[j],&b.calls[j],sizeof(call_t)))printf("call%d kind%d/%d args%d,%d,%d,%d / %d,%d,%d,%d\n",j,a.calls[j].kind,b.calls[j].kind,a.calls[j].a,a.calls[j].b,a.calls[j].c,a.calls[j].d,b.calls[j].a,b.calls[j].b,b.calls[j].c,b.calls[j].d);return 1;}n++;}
  }
 }
 printf(accepted?"ACCEPTED_EXTENSION ClientSpawn %d cases: PS prefix/suffix and entire remaining result equal; exact freelook center8000 and recoil suffix zero\n":"PASS whole defined ClientSpawn paths %d cases; unsupported original uninitialized-pose path excluded\n",n);return 0;
}

