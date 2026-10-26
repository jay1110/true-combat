#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_FlameDamage(gentity_t*,gentity_t*),G_BurnMeGood(gentity_t*,gentity_t*),G_BurnTarget(gentity_t*,gentity_t*,qboolean);
static byte*base,rawClient[0x16b0];static gclient_t client;static int side,v,count,bad;
typedef struct{int k,a,b,c,d,quota,frame;float vectors[6];}event;static event calls[1030];
typedef struct{int vals[8],count;event calls[1030];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static byte*raw(int n){return base+0x226e2a0+n*0x57c;}static void*ent(int n){return side?(void*)&g_entities[n]:(void*)raw(n);}static int idx(void*p){return !p?-1:side?(gentity_t*)p-g_entities:((byte*)p-raw(0))/0x57c;}
static event*note(int k){event*e;if(count==1030)exit(3);e=&calls[count++];e->k=k;e->quota=side?g_entities[7].flameQuota:*(int*)(raw(7)+0x398);e->frame=side?g_entities[7].lastBurnedFrameNumber:*(int*)(raw(7)+0x478);return e;}
static int box(const float*a,const float*b,int*out,int max){event*e=note(1);int i,n=v%5==4?1024:v%5;e->a=max;memcpy(e->vectors,a,12);memcpy(e->vectors+3,b,12);for(i=0;i<n;i++)out[i]=i;return n;}
static void target(void*s,void*b,int direct){event*e=note(2);e->a=idx(s);e->b=idx(b);e->c=direct;}
static float damage(void*t,void*i,void*a,const float*d,const float*p,int amount,int flags,int mod){event*e=note(3);e->a=idx(t);e->b=idx(i);e->c=idx(a);e->d=mod;if(amount!=5||flags||!d||!p)bad=1;memcpy(e->vectors,d,12);memcpy(e->vectors+3,p,12);if(v&4){if(side){g_entities[7].health=-1;g_entities[7].flameQuota=-9;}else{*(int*)(raw(7)+0x2c8)=-1;*(int*)(raw(7)+0x398)=-9;}}return .5f;}
static void capture(result*r){memset(r,0,sizeof(*r));
#define E(n,f,o) r->vals[n]=side?g_entities[7].f:*(int*)(raw(7)+o)
 E(0,flameQuota,0x398);E(1,flameQuotaTime,0x39c);E(2,lastBurnedFrameNumber,0x478);E(3,s.onFireStart,0xfc);E(4,s.onFireEnd,0x100);E(5,flameBurnEnt,0x3a0);E(6,health,0x2c8);r->vals[7]=side?client.ps.onFireStart:*(int*)(rawClient+0x424);r->count=count;memcpy(r->calls,calls,sizeof(calls));
#undef E
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*flame)(void*,void*),(__cdecl*burn)(void*,void*);int phase,i,total=0,ignore;static result a,b;float origins[]={-16777216.f,-1024.25f,-1.f,0,.1f,1.f,1024.75f,16777216.f},radii[]={0,.1f,1,7.9999995f,12.345f,128,9999.75f};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;flame=(void*)(base+0x6bc60);burn=(void*)(base+0x9f150);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x8afe0,trap_EntitiesInBox,box);H(0x6b9f0,G_BurnTarget,target);H(0x589c0,G_Damage,damage);
 for(phase=0;phase<2;phase++)for(v=0;v<8192;v++)for(side=0;side<2;side++){
  count=bad=0;memset(calls,0,sizeof(calls));memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));memset(&g_entities[5],0,sizeof(gentity_t));memset(&g_entities[7],0,sizeof(gentity_t));memset(raw(5),0,0x57c);memset(raw(7),0,0x57c);level.time=*(int*)(base+0x7febe0)=100000;level.framenum=*(int*)(base+0x7febdc)=123;g_entities[5].parent=v%3?&g_entities[9]:NULL;*(void**)(raw(5)+0x218)=v%3?raw(9):NULL;g_entities[7].client=v%3?&client:NULL;*(void**)(raw(7)+0x190)=v%3?rawClient:NULL;g_entities[5].r.ownerNum=*(int*)(raw(5)+0x180)=v%64;g_entities[5].speed=*(float*)(raw(5)+0x268)=radii[(v/5)%7];for(i=0;i<3;i++)g_entities[5].r.currentOrigin[i]=*(float*)(raw(5)+0x168+4*i)=origins[(v/35+i)%8];
#define E(f,o,x) g_entities[7].f=*(int*)(raw(7)+o)=(x)
 E(flameQuota,0x398,(v/3)%13-7);E(flameQuotaTime,0x39c,111);E(lastBurnedFrameNumber,0x478,122+(v/39)%3);E(health,0x2c8,(v/117)%3-1);E(s.onFireStart,0xfc,777);E(s.onFireEnd,0x100,99999+(v/351)%3);E(flameBurnEnt,0x3a0,555);client.ps.onFireStart=*(int*)(rawClient+0x424)=666;
  if(!phase){ignore=v%4==0?-1:v%4==1?0:v%4==2?2:1023;if(side)G_FlameDamage(ent(5),ignore<0?NULL:ent(ignore));else flame(ent(5),ignore<0?NULL:ent(ignore));}else{if(side)G_BurnMeGood(ent(5),ent(7));else burn(ent(5),ent(7));}capture(side?&b:&a);
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Flame dispatch mismatch phase%d case%d bad%d calls%d/%d\n",phase,v,bad,a.count,b.count);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("byte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}return 1;}total++;}
 }
 printf("PASS whole flame bounds dispatch and burn feedback %d cases (8192 each)\n",total);return 0;
}
