#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
extern void G_BounceMissile(gentity_t*,trace_t*);
extern void G_MissileImpact(gentity_t*,trace_t*,int);
static byte *base,*raw;static byte rawTemp[0x57c];static gentity_t temp;static gclient_t client;
static int side,variant,count;
typedef struct {int k,a,b,c,d,e,f;vec3_t x,y;} event;
typedef struct {int count;event calls[32];entityState_t state,tempstate;int next,owner,health,flags;vec3_t current,mins,maxs,absmin,absmax;} result;
static result out[2];
static int idx(void*p){return !p?-1:side?(int)((gentity_t*)p-g_entities):(int)(((byte*)p-raw)/0x57c);}
static event*rec(int k){event*e;if(count>=32)exit(3);e=&out[side].calls[count++];e->k=k;return e;}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void velocity(const trajectory_t*t,int time,vec3_t v,qboolean angle,int path,float scale){event*e=rec(1);int i;e->a=time;e->b=angle;e->c=path;memcpy(e->x,t->trDelta,12);e->y[0]=scale;for(i=0;i<3;i++)v[i]=(variant%7==0)?0:((variant+i*7)%97-48)*((variant/7)%2?0.125f:9.125f);}
static void explode(void*p){rec(2)->a=idx(p);}
static void freeent(void*p){rec(3)->a=idx(p);}
static void addEvent(void*p,int n,int parm){event*e=rec(4);e->a=idx(p);e->b=side&&n==EV_GRENADE_BOUNCE?50:n;e->c=parm;}
static void*tempEntity(vec3_t p,int n){event*e=rec(5);e->a=side?(n==EV_MISSILE_HIT?59:n==EV_MISSILE_MISS?60:n):n;memcpy(e->x,p,12);memset(&temp,0,sizeof(temp));memset(rawTemp,0,sizeof(rawTemp));return side?(void*)&temp:rawTemp;}
static float damage(void*t,void*i,void*a,vec3_t d,vec3_t p,int n,int flags,int mod){event*e=rec(6);e->a=idx(t);e->b=idx(i);e->c=idx(a);e->d=n;e->e=flags;e->f=mod;memcpy(e->x,d,12);memcpy(e->y,p,12);if(variant&8){if(side)((gentity_t*)t)->health-=n;else *(int*)((byte*)t+0x2c8)-=n;}return 0;}
static int accuracy(void*t,void*a){event*e=rec(7);e->a=idx(t);e->b=idx(a);return 1;}
static int radius(vec3_t p,void*i,void*a,float damage,float rad,void*ignore,int mod){event*e=rec(8);e->a=idx(i);e->b=idx(a);e->c=idx(ignore);e->d=mod;memcpy(e->x,p,12);e->y[0]=damage;e->y[1]=rad;return 1;}
static void mark(vec3_t in,vec3_t norm,vec3_t outv){event*e=rec(9);memcpy(e->x,in,12);memcpy(e->y,norm,12);VectorSet(outv,0,1,0);}
static void capture(void){result*r=&out[side];gentity_t*e=&g_entities[1];byte*p=raw+0x57c;r->count=count;memcpy(&r->state,side?(void*)&e->s:p,sizeof(entityState_t));memcpy(&r->tempstate,side?(void*)&temp.s:rawTemp,sizeof(entityState_t));r->next=side?e->nextthink:*(int*)(p+0x29c);r->owner=side?e->r.ownerNum:*(int*)(p+0x180);r->health=side?g_entities[2].health:*(int*)(raw+2*0x57c+0x2c8);r->flags=side?temp.r.svFlags:*(int*)(rawTemp+0x128);memcpy(r->current,side?(void*)e->r.currentOrigin:p+0x168,12);memcpy(r->mins,side?(void*)e->r.mins:p+0x134,12);memcpy(r->maxs,side?(void*)e->r.maxs:p+0x140,12);memcpy(r->absmin,side?(void*)e->r.absmin:p+0x150,12);memcpy(r->absmax,side?(void*)e->r.absmax:p+0x15c,12);}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*bounce)(void*,trace_t*),(__cdecl*impact)(void*,trace_t*,int);int phase,v,j,total=0,weapons[]={4,9,15,26,27,29,30,55,56,60};float fractions[]={0,0.1f,0.25f,0.5f,0.9f,1};trace_t tr;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;raw=base+0x226e2a0;bounce=(void*)(base+0x6a3d0);impact=(void*)(base+0x6a770);
 if(!hook(base+0x2ce80,velocity)||!hook(TCE_BG_EvaluateTrajectoryDelta,velocity)||!hook(base+0x6aab0,explode)||!hook(G_ExplodeMissile,explode)||!hook(base+0x94f30,freeent)||!hook(G_FreeEntity,freeent)||!hook(base+0x950e0,addEvent)||!hook(G_AddEvent,addEvent)||!hook(base+0x94fa0,tempEntity)||!hook(G_TempEntity,tempEntity)||!hook(base+0x589c0,damage)||!hook(G_Damage,damage)||!hook(base+0x9f570,accuracy)||!hook(AccuracyHit,accuracy)||!hook(base+0x5b750,radius)||!hook(G_RadiusDamage,radius)||!hook(base+0x2d170,mark)||!hook(BG_GetMarkDir,mark))return 2;
 for(phase=0;phase<2;phase++)for(v=0;v<65536;v++)for(side=0;side<2;side++){gentity_t*e=&g_entities[1],*other=&g_entities[2];byte*p=raw+0x57c,*o=raw+2*0x57c;variant=v;count=0;memset(&out[side],0,sizeof(result));memset(raw,0,5*0x57c);memset(g_entities,0,5*sizeof(gentity_t));memset(&temp,0,sizeof(temp));memset(rawTemp,0,sizeof(rawTemp));memset(&tr,0,sizeof(tr));level.time=*(int*)(base+0x7febe0)=100033+(v/1000)*10000;level.previousTime=*(int*)(base+0x7febe4)=level.time-53;tr.fraction=fractions[(v/10)%6];tr.entityNum=(v/60)%2?2:ENTITYNUM_WORLD;tr.surfaceFlags=(v/120)%2?SURF_METAL:0;VectorSet(tr.plane.normal,0.125f,-0.25f,(v/240)%3==0?0.2f:(v/240)%3==1?1.0f:-1.0f);VectorSet(tr.endpos,11.25f,-22.5f,33.75f);
  e->s.weapon=*(int*)(p+0xe8)=weapons[v%10];e->s.groundEntityNum=*(int*)(p+0x94)=(v/720)%3==0?-1:(v/720)%3==1?2:ENTITYNUM_WORLD;e->s.eFlags=*(int*)(p+8)=((v/2160)%4)*EF_BOUNCE;e->nextthink=*(int*)(p+0x29c)=level.time+3249+(v/8640)%3;e->s.effect2Time=*(int*)(p+0x110)=7;e->s.legsAnim=*(int*)(p+0xec)=31;e->r.ownerNum=*(int*)(p+0x180)=3;e->classname=(v/17280)%2?"WP":"grenade";*(char**)(p+0x1a4)=e->classname;e->damage=*(int*)(p+0x2d0)=v&1?15:0;e->splashDamage=*(int*)(p+0x2d4)=v&2?125:0;e->splashRadius=*(int*)(p+0x2d8)=250;e->methodOfDeath=*(int*)(p+0x2dc)=7;e->splashMethodOfDeath=*(int*)(p+0x2e0)=4;e->parent=&g_entities[4];*(void**)(p+0x218)=raw+4*0x57c;
  other->takedamage=*(int*)(o+0x2cc)=(v/3)%2;other->health=*(int*)(o+0x2c8)=((v/6)%4)*10-10;other->classname=(v/24)%3==0?NULL:(v/24)%3==1?"func_explosive":"other";*(char**)(o+0x1a4)=other->classname;other->client=(v/72)%2?&client:NULL;*(void**)(o+0x190)=other->client?(void*)(base+0x200):NULL;other->s.number=*(int*)o=77;if((v/144)%2){other->dmgparent=&g_entities[4];*(void**)(o+0x4b4)=raw+4*0x57c;}
  for(j=0;j<3;j++){e->r.currentOrigin[j]=*(float*)(p+0x168+j*4)=((v+j)%31-15)*0.1f;e->s.origin[j]=*(float*)(p+0x5c+j*4)=10.5f+j;e->s.pos.trDelta[j]=*(float*)(p+0x24+j*4)=30.25f-j;other->instantVelocity[j]=*(float*)(o+0x198+j*4)=((v+j)%31-15)*0.3f;}
  if(phase){tr.entityNum=2;if(side)G_MissileImpact(e,&tr,20);else impact(p,&tr,20);}else{if(side)G_BounceMissile(e,&tr);else bounce(p,&tr);}capture();if(side){if(memcmp(out,out+1,sizeof(result))){printf("Missile contact mismatch phase%d case%d calls%d/%d\n",phase,v,out[0].count,out[1].count);for(j=0;j<sizeof(result);j++)if(((byte*)out)[j]!=((byte*)(out+1))[j]){printf("byte%x %02x/%02x\n",j,((byte*)out)[j],((byte*)(out+1))[j]);break;}return 1;}total++;}
 }
 printf("PASS whole missile bounce/impact %d cases (65536 each)\n",total);return 0;
}
