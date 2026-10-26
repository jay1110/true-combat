#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>
extern void P_WorldEffects(gentity_t*),P_DamageFeedback(gentity_t*);
static byte*base,rawClient[0x16b0];static gclient_t client;static int side,count,bad,v;
typedef struct{int k,a,b,c,d,health,pain;float vec[3];char text[64];}event;static event calls[16];
typedef struct{int values[16],count;event calls[16];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static byte*raw(int n){return base+0x226e2a0+n*0x57c;}static void*ent(int n){return side?(void*)&g_entities[n]:(void*)raw(n);}static int idx(void*p){return !p?-1:side?(gentity_t*)p-g_entities:((byte*)p-raw(0))/0x57c;}
static event*note(int k){event*e;if(count==16)exit(3);e=&calls[count++];e->k=k;e->health=side?g_entities[5].health:*(int*)(raw(5)+0x2c8);e->pain=side?g_entities[5].pain_debounce_time:*(int*)(raw(5)+0x2c0);return e;}
static int randomValue(void){note(1);return v%2;}
static int soundIndex(const char*s){event*e=note(2);Q_strncpyz(e->text,s,sizeof(e->text));return 71;}
static void sound(void*p,int handle){event*e=note(3);e->a=idx(p);e->b=handle;}
static float damage(void*t,void*i,void*a,void*d,void*p,int amount,int flags,int mod){event*e=note(4);e->a=idx(t);e->b=idx(i);e->c=idx(a);e->d=mod;e->vec[0]=(float)amount;e->vec[1]=(float)flags;if(d||p)bad=1;if(v&4){if(side){g_entities[5].health-=amount;client.damage_blood+=amount;client.damage_fromWorld=1;}else{*(int*)(raw(5)+0x2c8)-=amount;*(int*)(rawClient+0xe2c)+=amount;*(int*)(rawClient+0xe40)=1;}}return .5f;}
static void eventAdd(void*p,int ev,int parm){event*e=note(5);e->a=idx(p);e->b=ev;e->c=parm;if(side){if(ev!=EV_PAIN)bad=1;e->b=65;}else if(ev!=65)bad=1;}
static void angles(const float*in,float*out){static const float a[]={0,1.40625f,-1.40625f,89.99999f,90,179.99998f,180,270,359.99997f,-90,-180,123.456f};event*e=note(6);memcpy(e->vec,in,12);out[0]=a[v%12];out[1]=a[(v/12)%12];out[2]=17;}
static void capture(result*r){memset(r,0,sizeof(*r));
#define E(n,f,o) r->values[n]=side?g_entities[5].f:*(int*)(raw(5)+o)
#define C(n,f,o) r->values[n]=side?client.f:*(int*)(rawClient+o)
 E(0,health,0x2c8);E(1,damage,0x2d0);E(2,pain_debounce_time,0x2c0);C(3,airOutTime,0xe60);C(4,ps.powerups[PW_BREATHER],0x160);C(5,lastBurnTime,0xfa4);C(6,damage_blood,0xe2c);C(7,damage_knockback,0xe30);C(8,damage_fromWorld,0xe40);C(9,ps.damageEvent,0xc0);C(10,ps.damageYaw,0xc4);C(11,ps.damagePitch,0xc8);C(12,ps.damageCount,0xcc);r->count=count;memcpy(r->calls,calls,sizeof(calls));
#undef E
#undef C
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*world)(void*),(__cdecl*feedback)(void*);int phase,total=0,i;static result a,b;int health[]={-1,0,1,5,15,100},blood[]={INT_MIN,-16777217,-1,0,1,126,127,128,999,INT_MAX},delta[]={-1,0,1,398,399,400,999,1000};unsigned r;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;world=(void*)(base+0x448f0);feedback=(void*)(base+0x447c0);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0xa20e4,rand,randomValue);H(0x947f0,G_SoundIndex,soundIndex);H(0x95180,G_Sound,sound);H(0x589c0,G_Damage,damage);H(0x950e0,G_AddEvent,eventAdd);H(0xa05c0,vectoangles,angles);
 for(phase=0;phase<3;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){
  byte*p=raw(5);gentity_t*e=&g_entities[5];count=bad=0;memset(calls,0,sizeof(calls));memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));memset(e,0,sizeof(*e));memset(p,0,0x57c);e->client=&client;*(void**)(p+0x190)=rawClient;e->s.number=*(int*)p=5;level.time=*(int*)(base+0x7febe0)=100000;r=(unsigned)v*1664525u+1013904223u;
#define E(f,o,x) e->f=*(int*)(p+o)=(x)
#define C(f,o,x) client.f=*(int*)(rawClient+o)=(x)
  E(health,0x2c8,health[(v/4)%6]);E(damage,0x2d0,(v/24)%19-2);E(waterlevel,0x304,v%4);E(watertype,0x300,(v/456)%3==0?CONTENTS_LAVA:(v/456)%3==1?CONTENTS_SLIME:0);E(pain_debounce_time,0x2c0,100000+(int)((r>>4)%3)-1);E(s.onFireEnd,0x100,(r&8)?100000+(int)((r>>6)%3)-1:0);E(flameBurnEnt,0x3a0,7);E(flags,0x1b0,(r&32)?FL_GODMODE:0);E(s.powerups,0xe4,(r&64)?1:2);
  C(noclip,0xe00,(v/1368)%5==1);C(airOutTime,0xe60,100000-delta[(r>>9)%8]);C(ps.powerups[PW_BREATHER],0x160,(int)((r>>12)%3)-1);C(lastBurnTime,0xfa4,100000-delta[(r>>16)%8]);C(ps.pm_type,4,(v/6840)%5);C(damage_blood,0xe2c,phase==1?blood[(v/2)%10]:(v%128));C(damage_knockback,0xe30,99);C(damage_fromWorld,0xe40,(v/20)%2);C(ps.damageEvent,0xc0,17);C(ps.damageYaw,0xc4,91);C(ps.damagePitch,0xc8,92);C(ps.damageCount,0xcc,93);for(i=0;i<3;i++)client.damage_from[i]=*(float*)(rawClient+0xe34+i*4)=i*7.f+v%3;
  if(phase!=1){if(side)P_WorldEffects(ent(5));else world(ent(5));}if(phase!=0){if(side)P_DamageFeedback(ent(5));else feedback(ent(5));}capture(side?&b:&a);
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("World/feedback mismatch phase%d case%d bad%d calls%d/%d\n",phase,v,bad,a.count,b.count);for(i=0;i<16;i++)if(a.values[i]!=b.values[i])printf("field%d %d/%d\n",i,a.values[i],b.values[i]);for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.calls[i],&b.calls[i],sizeof(event)))printf("call%d kind%d/%d args%d,%d,%d,%d / %d,%d,%d,%d\n",i,a.calls[i].k,b.calls[i].k,a.calls[i].a,a.calls[i].b,a.calls[i].c,a.calls[i].d,b.calls[i].a,b.calls[i].b,b.calls[i].c,b.calls[i].d);return 1;}total++;}
 }
 printf("PASS whole world effects and damage feedback %d cases (32768 each isolated and connected)\n",total);return 0;
}
