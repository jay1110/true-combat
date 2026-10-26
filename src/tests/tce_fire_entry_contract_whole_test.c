#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern vec3_t muzzleTrace;extern void Bullet_Fire(gentity_t*,float,int,qboolean),Bullet_Endpos(gentity_t*,float,vec3_t*),Weapon_Shotgun_Fire(gentity_t*),ShotgunPattern(vec3_t,vec3_t,int,gentity_t*);
extern qboolean Bullet_Fire_Extended(gentity_t*,gentity_t*,vec3_t,vec3_t,float,int,int,int,int,int,int,int,int,int);
static byte*base,rawClient[0x16b0];static gclient_t client;static int side,v,count,bad;
typedef struct{int k,args[12];float vec[7];}event;static event calls[8];
typedef struct{int count,seed,weapon,flags,lag;entityState_t temp;event calls[8];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static byte*raw(int n){return base+0x226e2a0+n*0x57c;}static void*ent(int n){return side?(void*)&g_entities[n]:(void*)raw(n);}static int idx(void*p){return !p?-1:side?(gentity_t*)p-g_entities:((byte*)p-raw(0))/0x57c;}
static event*note(int k){event*e;if(count==8)exit(3);e=&calls[count++];e->k=k;return e;}
static void begin(void*p,int scale){event*e=note(1);e->args[0]=idx(p);e->args[1]=scale;if(v&16){if(side)client.ps.stats[STAT_TCE_SHOT_SEED]+=7;else*(int*)(rawClient+0xec)+=7;}}
static void end(void*p){note(2)->args[0]=idx(p);}
static void endpoint(void*p,float spread,float*out){event*e=note(3);int i;e->args[0]=idx(p);e->vec[0]=spread;for(i=0;i<3;i++)out[i]=(v%101-50)*(i+1)*.33333334f;if(v&32){if(side){client.ps.stats[STAT_TCE_SHOT_SEED]+=11;client.ps.weapon=(client.ps.weapon+1)%64;client.ps.stats[STAT_TCE_WEAPON_FLAGS]^=4;}else{*(int*)(rawClient+0xec)+=11;*(int*)(rawClient+0xa4)=(*(int*)(rawClient+0xa4)+1)%64;*(int*)(rawClient+0xf0)^=4;}}}
static int extended(void*s,void*a,float*start,float*endp,float spread,int damage,int penetration,int maxdist,int dist,int walls,int bodies,int passes,int seed,int suppress){event*e=note(4);e->args[0]=idx(s);e->args[1]=idx(a);e->args[2]=damage;e->args[3]=penetration;e->args[4]=maxdist;e->args[5]=dist;e->args[6]=walls;e->args[7]=bodies;e->args[8]=passes;e->args[9]=seed;e->args[10]=suppress;memcpy(e->vec,start,12);memcpy(e->vec+3,endp,12);e->vec[6]=spread;if(v&64){if(side)g_antilag.integer^=1;else*(int*)(base+0x23cda8c)^=1;}return v&1;}
static void*temp(const float*origin,int ev){event*e=note(5);e->args[0]=ev;memcpy(e->vec,origin,12);if(side){if(ev!=EV_TCE_SHOTGUN)bad=1;e->args[0]=161;memcpy(g_entities[70].s.pos.trBase,origin,12);}else{if(ev!=161)bad=1;memcpy(raw(70)+0x18,origin,12);}return ent(70);}
static void pattern(float*origin,float*dir,int seed,void*p){event*e=note(6);e->args[0]=seed;e->args[1]=idx(p);memcpy(e->vec,origin,12);memcpy(e->vec+3,dir,12);}
static void capture(result*r){memset(r,0,sizeof(*r));r->count=count;r->seed=side?client.ps.stats[STAT_TCE_SHOT_SEED]:*(int*)(rawClient+0xec);r->weapon=side?client.ps.weapon:*(int*)(rawClient+0xa4);r->flags=side?client.ps.stats[STAT_TCE_WEAPON_FLAGS]:*(int*)(rawClient+0xf0);r->lag=side?g_antilag.integer:*(int*)(base+0x23cda8c);memcpy(&r->temp,side?(void*)&g_entities[70].s:(void*)raw(70),sizeof(r->temp));memcpy(r->calls,calls,sizeof(calls));}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*bullet)(void*,float),(__cdecl*shotgun)(void*);int phase,i,total=0;float spread;static result a,b;int damages[]={-101,-1,0,1,3,73,999};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;bullet=(void*)(base+0x9d410);shotgun=(void*)(base+0x9f070);if(sizeof(tce_weaponDef_t)!=0x1cc||sizeof(entityState_t)!=0x120)return 2;
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x4ebf0,G_TimeShiftAllClients,begin);H(0x4ee70,G_UnTimeShiftAllClients,end);H(0x9d1a0,Bullet_Endpos,endpoint);H(0x9d500,Bullet_Fire_Extended,extended);H(0x94fa0,G_TempEntity,temp);H(0x9ee90,ShotgunPattern,pattern);
 for(phase=0;phase<2;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){
  count=bad=0;memset(calls,0,sizeof(calls));memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));memset(&g_entities[5],0,sizeof(gentity_t));memset(raw(5),0,0x57c);memset(&g_entities[70],0,sizeof(gentity_t));memset(raw(70),0,0x57c);g_entities[5].client=&client;*(void**)(raw(5)+0x190)=rawClient;g_entities[5].s.number=*(int*)raw(5)=53;client.ps.clientNum=*(int*)(rawClient+0xa0)=17;g_entities[5].s.weapon=*(int*)(raw(5)+0xe8)=(v+9)%64;client.ps.weapon=*(int*)(rawClient+0xa4)=v%64;client.ps.stats[STAT_TCE_SHOT_SEED]=*(int*)(rawClient+0xec)=v*521-19001;client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(rawClient+0xf0)=(v&2)?4:0;g_entities[5].r.svFlags=*(int*)(raw(5)+0x128)=(v&4)?SVF_BOT:0;g_antilag.integer=*(int*)(base+0x23cda8c)=(v&8)?1:0;spread=(v%5-2)*123.25f;for(i=0;i<3;i++)muzzleTrace[i]=*(float*)(base+0x66bb90+i*4)=(v%97-48)*(i+1)*.1875f;
  for(i=0;i<64;i++){tce_weaponDef_t def;memset(&def,0,sizeof(def));def.unknown_12c=damages[(v/128+i)%7];def.unknown_134=(v/896+i)%201-100;def.unknown_1c0=(v/1792+i)%3-1;weaponDef[i]=def;memcpy(base+0x24435c0+i*0x1cc,&def,sizeof(def));}
  if(!phase){if(side)Bullet_Fire(ent(5),spread,-777,qtrue);else bullet(ent(5),spread);}else{if(side)Weapon_Shotgun_Fire(ent(5));else shotgun(ent(5));}capture(side?&b:&a);
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Fire entry mismatch phase%d case%d bad%d calls%d/%d\n",phase,v,bad,a.count,b.count);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("byte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}return 1;}total++;}
 }
 printf("PASS whole Bullet_Fire and Weapon_Shotgun_Fire %d cases (32768 each), all extended arguments/event payload\n",total);return 0;
}
