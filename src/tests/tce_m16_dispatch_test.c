#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern void Bullet_Fire(gentity_t *,float,int,qboolean);
extern void CalcMuzzlePoints(gentity_t *,int);
extern gentity_t *weapon_mortar_fire(gentity_t *,int);
static int bullets,mortars;static float bulletSpread;
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))abort();
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static void muzzle(void *e,int w) {(void)e;(void)w;}
static void originalBullet(void *e,float spread) {++bullets;bulletSpread=spread;}
static void sourceBullet(gentity_t *e,float spread,int damage,qboolean falloff) {++bullets;bulletSpread=spread;}
static gentity_t *mortar(void *e,int w){++mortars;return NULL;}
static unsigned char raw[0x1700],ent[0x57c];static gclient_t client;static gentity_t actor;
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base;void (__cdecl *original)(void *);
 int dead,heavy,air,user,k,n=0,expected;float expectedSpread;
 const float scales[]={-1,0,0.1f,0.5f,0.85f,1,2};
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;
 original=(void *)(base+0x9f950);
 hook(base+0x9f780,muzzle);hook(base+0x9d410,originalBullet);hook(base+0x9e840,mortar);
 hook((unsigned char *)CalcMuzzlePoints,muzzle);hook((unsigned char *)Bullet_Fire,sourceBullet);
 hook((unsigned char *)weapon_mortar_fire,mortar);
 *(void **)(ent+0x190)=raw;actor.client=&client;
 *(int *)(ent+0xe8)=actor.s.weapon=45;*(int *)(raw+0xa4)=client.ps.weapon=45;
 gearDef.parsed=1;weaponDef[45].parsed=1;
 memset(base+0x24435c0,0,64*sizeof(tce_weaponDef_t));
 *(int *)(base+0x784f6c)=1;
 for(dead=0;dead<2;++dead)for(heavy=0;heavy<2;++heavy)for(air=0;air<2;++air)
 for(user=0;user<2;++user)for(k=0;k<7;++k) {
  *(int *)(raw+4)=client.ps.pm_type=dead?PM_DEAD:PM_NORMAL;
  *(int *)(raw+0x140)=client.ps.persistant[PERS_HWEAPON_USE]=heavy;
  *(int *)(ent+0x36c)=actor.active=heavy;
  *(int *)(raw+0x50)=client.ps.groundEntityNum=air?ENTITYNUM_NONE:0;
  *(int *)(base+0x23cdf0c)=g_userAim.integer=user;
  *(float *)(raw+0xe78)=client.currentAimSpreadScale=scales[k];
  bullets=mortars=0;bulletSpread=0;original(ent);
  expected=bullets;expectedSpread=bulletSpread;if(mortars)return 1;
  bullets=mortars=0;bulletSpread=0;FireWeapon(&actor);
  if(mortars || bullets!=expected || fabs(bulletSpread-expectedSpread)>0.0002) {
   fprintf(stderr,"M16 dispatch %d/%d mortar%d spread%g/%g\n",bullets,expected,mortars,bulletSpread,expectedSpread);return 1;
  }
  ++n;
 }
 printf("%d original/production M16 dispatch comparisons, no mortar calls\n",n);return 0;
}
