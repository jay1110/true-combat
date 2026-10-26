#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern float G_GetWeaponSpread(int);
extern int G_GetWeaponDamage(int);
extern void Bullet_Fire(gentity_t *,float,int,qboolean);
extern void CalcMuzzlePoints(gentity_t *,int);
extern void Weapon_Shotgun_Fire(gentity_t *);
extern void Weapon_Knife(gentity_t *);
extern void Weapon_Medic(gentity_t *);
extern void Weapon_Engineer(gentity_t *);
extern void Weapon_Syringe(gentity_t *);
extern void Weapon_AdrenalineSyringe(gentity_t *);
extern void Weapon_MagicAmmo(gentity_t *);
extern void Weapon_Artillery(gentity_t *);
extern void Weapon_Panzerfaust_Fire(gentity_t *);
extern void Weapon_FlamethrowerFire(gentity_t *);
extern gentity_t *weapon_gpg40_fire(gentity_t *,int);
extern gentity_t *weapon_mortar_fire(gentity_t *,int);
extern gentity_t *weapon_grenadelauncher_fire(gentity_t *,int);
extern qboolean G_PlayerCanBeSeenByOthers(gentity_t *);
extern qboolean G_ExplodeSatchels(gentity_t *);
static unsigned char raw[0x2000],entity[0x57c];
static gclient_t client; static gentity_t actor;
static int calls[32],count,seen,explode; static float spread;
static char printText[128];static int printCalls;
static void hook(void *address,void *fn) {
    unsigned char *p=address; DWORD old,tmp;
    if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old)) abort();
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static void record(int id) { if(count>=32)abort();calls[count++]=id; }
static void muzzle(void *e,int w) {record(1000+w);}
static void bullet(void *e,float value) {record(200);spread=value;}
static void sdkBullet(gentity_t *e,float value,int d,qboolean f) {bullet(e,value);}
#define SINK(n,id) static void n(void *e) {record(id);}
SINK(knife,1) SINK(medic,19) SINK(engineer,21) SINK(syringe,11)
SINK(adrenaline,61) SINK(ammo,12) SINK(arty,63) SINK(panzer,65)
SINK(flame,66) SINK(shotgun,201) SINK(tripmine,29)
static void *grenade(void *e,int w){record(300+w);return NULL;}
static void *rifle(void *e,int w){record(400+w);return NULL;}
static void *mortar(void *e,int w){record(500+w);return NULL;}
static int visible(void *e){record(600);return seen;}
static int satchels(void *e){record(601);return explode;}
static void event(void *e,int v,int parm){record(700+v);record(parm);}
static int weaponStatSink(int w){record(800+w);return 0;}
static void print(const char *fmt,...){va_list ap;va_start(ap,fmt);Q_vsnprintf(printText,sizeof(printText),fmt,ap);va_end(ap);++printCalls;}
#define PAIR(off,source,sink) hook(base+(off),(void *)(sink));hook((void *)(source),(void *)(sink))
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base; tce_weaponDef_t *defs;
    int (__cdecl *damageFn)(int);float (__cdecl *spreadFn)(int);void (__cdecl *fire)(void *);
    int w,state,skill,expected[32],expectedCount,expectedFlags,expectedPower,expectedTime,expectedAttacks;
    int i,n=0;float expectedSpread;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll;damageFn=(void *)(base+0x9cf40);spreadFn=(void *)(base+0x9d020);fire=(void *)(base+0x9f950);
    PAIR(0x5ed80,G_Printf,print);
    for(w=-128;w<256;++w) {
        char expectedText[128];float expectedValue,actualValue;int logged;
        printCalls=0;printText[0]=0;expectedValue=spreadFn(w);logged=printCalls;strcpy(expectedText,printText);
        printCalls=0;printText[0]=0;actualValue=G_GetWeaponSpread(w);
        if(damageFn(w)!=G_GetWeaponDamage(w) || expectedValue!=actualValue || logged!=printCalls || strcmp(expectedText,printText)) {
            fprintf(stderr,"weapon lookup mismatch %d output=%s/%s\n",w,printText,expectedText);return 1;
        }
    }
    PAIR(0x9f780,CalcMuzzlePoints,muzzle);hook(base+0x9d410,bullet);hook(Bullet_Fire,sdkBullet);
    PAIR(0x9f070,Weapon_Shotgun_Fire,shotgun);PAIR(0x97d70,Weapon_Knife,knife);
    PAIR(0x98040,Weapon_Medic,medic);PAIR(0x99d80,Weapon_Engineer,engineer);
    PAIR(0x98ca0,Weapon_Syringe,syringe);PAIR(0x98ef0,Weapon_AdrenalineSyringe,adrenaline);
    PAIR(0x98590,Weapon_MagicAmmo,ammo);PAIR(0x9c730,Weapon_Artillery,arty);
    PAIR(0x9f130,Weapon_Panzerfaust_Fire,panzer);PAIR(0x9f210,Weapon_FlamethrowerFire,flame);
    PAIR(0x983b0,G_PlaceTripmine,tripmine);PAIR(0x9ea00,weapon_grenadelauncher_fire,grenade);
    PAIR(0x9e620,weapon_gpg40_fire,rifle);PAIR(0x9e840,weapon_mortar_fire,mortar);
    PAIR(0x9f810,G_PlayerCanBeSeenByOthers,visible);PAIR(0x6c590,G_ExplodeSatchels,satchels);
    PAIR(0x950e0,G_AddEvent,event);PAIR(0x3db90,BG_WeapStatForWeapon,weaponStatSink);
    defs=(void *)(base+0x24435c0);
    for(w=0;w<=67;++w)for(state=0;state<1024;++state)for(skill=0;skill<5;++skill) {
        memset(raw,0,sizeof(raw));memset(entity,0,sizeof(entity));memset(&client,0,sizeof(client));memset(&actor,0,sizeof(actor));
        *(void **)(entity+0x190)=raw;actor.client=&client;
        *(int *)(entity+0xe8)=actor.s.weapon=w;*(int *)(raw+0xa4)=client.ps.weapon=w;
        *(int *)(raw+4)=client.ps.pm_type=(state&1)?PM_DEAD:PM_NORMAL;
        *(int *)(raw+0x140)=client.ps.persistant[PERS_HWEAPON_USE]=!!(state&2);
        *(int *)(entity+0x36c)=actor.active=!!(state&2);
        *(int *)(raw+0x50)=client.ps.groundEntityNum=(state&4)?ENTITYNUM_NONE:0;
        *(int *)(base+0x23cdf0c)=g_userAim.integer=!!(state&8);
        *(float *)(raw+0xe78)=client.currentAimSpreadScale=(float)(skill-1)*.4f;
        *(int *)(raw+0x170)=client.ps.powerups[PW_OPS_DISGUISED]=!!(state&16);
        seen=!!(state&32);explode=!!(state&64);
        *(int *)(raw+0xf0)=client.ps.stats[STAT_TCE_WEAPON_FLAGS]=skill*5;
        *(int *)(raw+0x148)=client.ps.persistant[14]=(state&64)?8:0;
        *(int *)(raw+0xc)=client.ps.pm_flags=(state&64)?PMF_DUCKED:0;
        *(int *)(raw+0x68)=client.ps.eFlags=(state&32)?EF_PRONE:0;
        if(state&128) *(int *)(raw+0x68)=client.ps.eFlags|=EF_ZOOMING;
        *(int *)(raw+0xd4)=client.ps.stats[STAT_KEYS]=(state&256)?1<<INV_BINOCS:0;
        *(int *)(raw+0xb40)=client.sess.playerType=skill;
        *(float *)(raw+0x3c)=client.ps.leanf=(state&512)?.5f:0;
        *(int *)(raw+0xb30)=client.sess.sessionTeam=TEAM_AXIS;
        *(int *)(raw+0x478)=client.ps.classWeaponTime=(state&32)?9900:0;
        *(int *)(base+0x7febe0)=level.time=10000;
        *(int *)(base+0x784f6c)=g_gamestate.integer=state&1;
        for(i=0;i<7;++i) *(int *)(raw+0xba4+i*4)=client.sess.skill[i]=skill;
        *(int *)(base+0x221a464)=level.soldierChargeTime[0]=1201;
        *(int *)(base+0x221a474)=level.engineerChargeTime[0]=1301;
        *(int *)(base+0x221a47c)=level.lieutenantChargeTime[0]=1401;
        *(int *)(base+0x221a484)=level.covertopsChargeTime[0]=1501;
        if(w<64) {
            memset(&defs[w],0,sizeof(*defs));memset(&weaponDef[w],0,sizeof(*defs));
            defs[w].pelletCount=weaponDef[w].pelletCount=!!(state&64);
            defs[w].unknown_1a8=weaponDef[w].unknown_1a8=!!(state&32);
        }
        count=0;spread=0;fire(entity);expectedCount=count;memcpy(expected,calls,sizeof(calls));expectedSpread=spread;
        expectedFlags=*(int *)(raw+0xf0);expectedPower=*(int *)(raw+0x170);expectedTime=*(int *)(raw+0x478);expectedAttacks=*(int *)(raw+0xc14);
        count=0;spread=0;FireWeapon(&actor);
        if(count!=expectedCount || memcmp(expected,calls,count*sizeof(int)) || fabs(spread-expectedSpread)>.0003 ||
           client.ps.stats[STAT_TCE_WEAPON_FLAGS]!=expectedFlags || client.ps.powerups[PW_OPS_DISGUISED]!=expectedPower ||
           client.ps.classWeaponTime!=expectedTime || client.sess.aWeaponStats[0].atts!=expectedAttacks ||
           client.ps.ammo[28]!=*(int *)(raw+0x200) || client.ps.ammoclip[28]!=*(int *)(raw+0x300) ||
           client.ps.ammoclip[27]!=*(int *)(raw+0x2fc)) {
            fprintf(stderr,"dispatch w%d state%d skill%d calls%d/%d spread%g/%g flags%d/%d time%d/%d attacks%d/%d\n",
                w,state,skill,count,expectedCount,spread,expectedSpread,client.ps.stats[STAT_TCE_WEAPON_FLAGS],expectedFlags,
                client.ps.classWeaponTime,expectedTime,client.sess.aWeaponStats[0].atts,expectedAttacks);return 1;
        }
        for(i=0;i<3;++i) if(fabs(client.ps.velocity[i]-*(float *)(raw+0x20+i*4))>.00001) return 1;
        ++n;
    }
    printf("768 damage/spread lookups and %d whole-dispatch original comparisons passed\n",n);
    return 0;
}
