#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern void DynaSink(gentity_t*),DynaFree(gentity_t*),G_FreeSatchel(gentity_t*);
extern void AddKillScore(gentity_t*,int);
static byte *base,*raw,rc[0x16b0];static gclient_t client;static int side,variant,count,getcalls;
typedef struct{int k,a,b;vec3_t x,y;char text[128];}event;
typedef struct{int count;event calls[32];entityState_t bolt,sound,popup;int fields[25];vec3_t current,mins,maxs,absmin,absmax;char classname[40];}result;
static result out[2];
static int idx(void*p){return !p?-1:side?(int)((gentity_t*)p-g_entities):(int)(((byte*)p-raw)/0x57c);}
static event*rec(int k){if(count==32)exit(3);out[side].calls[count].k=k;return &out[side].calls[count++];}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void*spawn(void){rec(1);return side?(void*)&g_entities[1]:raw+0x57c;}
static int weaponDamage(int w){event*e=rec(2);e->a=w;return 100+w+(getcalls++);}
static int box(vec3_t a,vec3_t b,int*list,int max){event*e=rec(3);e->a=max;memcpy(e->x,a,12);memcpy(e->y,b,12);list[0]=4;list[1]=6;return (variant/64)%3;}
static void*temp(vec3_t p,int ev){event*e=rec(4);e->a=side&&ev==EV_GLOBAL_TEAM_SOUND?53:ev;memcpy(e->x,p,12);return side?(void*)&g_entities[2]:raw+2*0x57c;}
static int sound(const char*s){Q_strncpyz(rec(5)->text,s,128);return 31;}
static void*popup(int type){rec(6)->a=type;return side?(void*)&g_entities[3]:raw+3*0x57c;}
static void script(void*p,char*a,char*b){event*e=rec(7);e->a=idx(p);Q_strncpyz(e->text,a,128);e->b=(int)strlen(b);}
static void score(void*p,int n){event*e=rec(8);e->a=idx(p);e->b=n;}
static void killscore(void*p,int n){event*e=rec(9);e->a=idx(p);e->b=n;}
static void logmsg(const char*fmt,...){event*e=rec(10);va_list ap;va_start(ap,fmt);Q_vsnprintf(e->text,128,fmt,ap);va_end(ap);}
static int thinkid(void*p){if(!p)return 0;if(p==(side?(void*)DynaSink:base+0x6c240))return 1;if(p==(side?(void*)G_ExplodeMissile:base+0x6aab0))return 2;if(p==(side?(void*)G_FreeEntity:base+0x94f30))return 3;return -99;}
static int freeid(void*p){if(!p)return 0;if(p==(side?(void*)DynaFree:base+0x6c2a0))return 1;if(p==(side?(void*)G_FreeSatchel:base+0x6c660))return 2;return -99;}
static void capture(void){gentity_t*e=&g_entities[1];byte*p=raw+0x57c;result*r=&out[side];char*c;r->count=count;memcpy(&r->bolt,side?(void*)&e->s:p,sizeof(entityState_t));memcpy(&r->sound,side?(void*)&g_entities[2].s:raw+2*0x57c,sizeof(entityState_t));memcpy(&r->popup,side?(void*)&g_entities[3].s:raw+3*0x57c,sizeof(entityState_t));
#define F(n,field,offset) r->fields[n]=side?e->field:*(int*)(p+offset)
F(0,nextthink,0x29c);F(1,timestamp,0x24c);F(2,damage,0x2d0);F(3,splashDamage,0x2d4);F(4,splashRadius,0x2d8);F(5,methodOfDeath,0x2dc);F(6,splashMethodOfDeath,0x2e0);F(7,clipmask,0x1d4);F(8,r.contents,0x14c);F(9,r.snapshotCallback,0x18c);F(10,takedamage,0x2cc);F(11,health,0x2c8);F(12,accuracy,0x420);F(13,awaitingHelpTime,0x4c8);
#undef F
r->fields[14]=thinkid(side?(void*)e->think:*(void**)(p+0x2a4));r->fields[15]=freeid(side?(void*)e->free:*(void**)(p+0x2a0));r->fields[16]=side?client.ps.grenadeTimeLeft:*(int*)(rc+0x34);r->fields[17]=side?client.ps.stats[STAT_TCE_WEAPON_FLAGS]:*(int*)(rc+0xf0);r->fields[18]=side?level.tceBombPlanted:*(int*)(base+0x2223914);r->fields[19]=side?level.tceBombCarrierCount:*(int*)(base+0x2223938);r->fields[20]=idx(side?(void*)e->parent:*(void**)(p+0x218));r->fields[21]=side?e->r.ownerNum:*(int*)(p+0x180);r->fields[22]=side?e->r.svFlags:*(int*)(p+0x128);r->fields[23]=side?g_entities[2].r.svFlags:*(int*)(raw+2*0x57c+0x128);r->fields[24]=side?client.tceBombPossessionOrder:*(int*)(rc+0x1328);memcpy(r->current,side?(void*)e->r.currentOrigin:p+0x168,12);memcpy(r->mins,side?(void*)e->r.mins:p+0x134,12);memcpy(r->maxs,side?(void*)e->r.maxs:p+0x140,12);memcpy(r->absmin,side?(void*)e->r.absmin:p+0x150,12);memcpy(r->absmax,side?(void*)e->r.absmax:p+0x15c,12);c=side?e->classname:*(char**)(p+0x1a4);if(c)Q_strncpyz(r->classname,c,40);}
int main(int argc,char**argv){HMODULE dll;void*(__cdecl*fire)(void*,vec3_t,vec3_t,int);void(__cdecl*sink)(void*);int phase,v,j,total=0;vec3_t start={12.25f,-7.75f,64.5f},dir={111.3f,-99.7f,130.8f};
if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;raw=base+0x226e2a0;fire=(void*)(base+0x6cc30);sink=(void*)(base+0x6c240);
if(!hook(base+0x94e30,spawn)||!hook(G_Spawn,spawn)||!hook(base+0x9cf40,weaponDamage)||!hook(G_GetWeaponDamage,weaponDamage)||!hook(base+0x8afe0,box)||!hook(trap_EntitiesInBox,box)||!hook(base+0x94fa0,temp)||!hook(G_TempEntity,temp)||!hook(base+0x947f0,sound)||!hook(G_SoundIndex,sound)||!hook(base+0x95050,popup)||!hook(G_PopupMessage,popup)||!hook(base+0x7d4b0,script)||!hook(G_Script_ScriptEvent,script)||!hook(base+0x571d0,score)||!hook(AddScore,score)||!hook(base+0x57230,killscore)||!hook(AddKillScore,killscore)||!hook(base+0x62600,logmsg)||!hook(G_LogPrintf,logmsg))return 2;
for(phase=0;phase<2;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){gentity_t*self=&g_entities[0],*e=&g_entities[1];byte*p=raw+0x57c;variant=v;count=getcalls=0;memset(&out[side],0,sizeof(result));memset(raw,0,8*0x57c);memset(g_entities,0,8*sizeof(gentity_t));memset(rc,0,sizeof(rc));memset(&client,0,sizeof(client));level.time=*(int*)(base+0x7febe0)=100000;g_newbbox.integer=*(int*)(base+0x7e208c)=(v/192)%2;g_gametype.integer=*(int*)(base+0x6d8d4c)=(v/384)%2?5:2;level.tceBombPlanted=*(int*)(base+0x2223914)=0;level.tceBombCarrierCount=*(int*)(base+0x2223938)=3;self->client=&client;*(void**)(raw+0x190)=rc;self->s.number=*(int*)raw=19;self->s.clientNum=*(int*)(raw+0xac)=27;self->s.groundEntityNum=*(int*)(raw+0x94)=(v/768)%2?7:ENTITYNUM_WORLD;client.sess.sessionTeam=*(int*)(rc+0xb30)=1+(v/1536)%2;client.ps.grenadeTimeLeft=*(int*)(rc+0x34)=(v/3072)%3*431;client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(rc+0xf0)=0x100|((v/6144)%2?4:0);client.tceBombPossessionOrder=*(int*)(rc+0x1328)=9;
for(j=0;j<3;j++){self->r.currentOrigin[j]=*(float*)(raw+0x168+j*4)=start[j];g_entities[7].instantVelocity[j]=*(float*)(raw+7*0x57c+0x198+j*4)=3.5f+j;}
for(j=4;j<=6;j+=2){gentity_t*h=&g_entities[j];byte*q=raw+j*0x57c;h->classname=j==4&&((v/12288)%2)?"TRIGGER_OBJECTIVE_INFO":"trigger_objective_info";*(char**)(q+0x1a4)=h->classname;h->r.contents=*(int*)(q+0x14c)=CONTENTS_TRIGGER;h->spawnflags=*(int*)(q+0x1a8)=1+(v/48)%3;h->accuracy=*(int*)(q+0x420)=8;h->s.teamNum=*(int*)(q+0x108)=17;h->target_ent=&g_entities[5];*(void**)(q+0x264)=raw+5*0x57c;}g_entities[5].tceObjectiveScore=*(int*)(raw+5*0x57c+0x2e4)=(v/24)%2?10:0;
if(!phase){void*r=side?(void*)fire_grenade(self,start,dir,v%64):fire(raw,start,dir,v%64);if(r!=(side?(void*)e:p))return 3;}else{e->timestamp=*(int*)(p+0x24c)=99999+v%3;e->clipmask=*(int*)(p+0x1d4)=123;e->r.contents=*(int*)(p+0x14c)=456;e->s.pos.trBase[2]=*(float*)(p+0x20)=(v-16384)*0.125f;e->think=DynaSink;*(void**)(p+0x2a4)=base+0x6c240;if(side)DynaSink(e);else sink(p);}capture();if(side){if(memcmp(out,out+1,sizeof(result))){printf("Grenade constructor mismatch phase%d case%d calls%d/%d\n",phase,v,out[0].count,out[1].count);for(j=0;j<sizeof(result);j++)if(((byte*)out)[j]!=((byte*)(out+1))[j]){printf("byte%x %02x/%02x\n",j,((byte*)out)[j],((byte*)(out+1))[j]);break;}return 1;}total++;}}
printf("PASS whole fire_grenade/DynaSink %d cases (32768 each)\n",total);return 0;}
