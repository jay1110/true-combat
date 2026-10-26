#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern void CG_FiremodeDown_f(void);
extern void CG_FiremodeUp_f(void);
extern qboolean CG_WeaponSelectable(int weapon);
extern int CG_WeaponIndex(int weapon,int *bank,int *cycle);
extern int getEquivWeapon(int weapon);
extern void CG_FinishWeaponChange(int,int);
extern void CG_PlaySwitchSound(int,int);
extern void CG_SetSniperZoom(int,int);
extern void CG_OutOfAmmoChange(qboolean);
extern void CG_WeaponBank_f(void);
static char autoBankArg[16];
static const char *autoArgv(int n) { return autoBankArg; }
extern void CG_NextWeap(qboolean);
extern int weapAlts[];
static int autoOwned[67],autoFinish,autoOld,autoNew,autoNext;
static int autoSelectable(int w) { return w>0 && w<67 && autoOwned[w]; }
static void autoFinishSink(int a,int b) { ++autoFinish;autoOld=a;autoNew=b; }
static void autoNextSink(int b) { autoNext=b+1; }
static void noopSwitch(int a,int b) {}
static int soundCalls,soundId,soundEntity,soundChannel;
static void soundSink(vec3_t origin,int entity,int channel,int sound) {
    if(origin)abort();++soundCalls;soundId=sound;soundEntity=entity;soundChannel=channel;
}
static void patch(void *address,void *fn) {
 unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv) {
    HMODULE dll; unsigned char *base,raw[0x800]; static snapshot_t snap;
    void (__cdecl *down)(void),(__cdecl *up)(void);
    int w,mask,scenario,d,cases=0,delays[]={-1,0,749,750,1599,1600,4000};
    tce_weaponDef_t *defs;
    setbuf(stdout,NULL);
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll;down=(void *)(base+0x1bd70);up=(void *)(base+0x1be10);
    defs=(void *)(base+0x4c1df20);memset(raw,0,sizeof(raw));
    for(w=0;w<64;++w)for(mask=0;mask<32;++mask)for(scenario=0;scenario<9;++scenario)for(d=0;d<7;++d) {
        defs[w].semiauto=weaponDef[w].semiauto=mask&1;
        defs[w].fullauto=weaponDef[w].fullauto=mask&2;
        defs[w].burst=weaponDef[w].burst=(mask&4)?3:0;
        defs[w].pump=weaponDef[w].pump=mask&8;
        defs[w].bolt=weaponDef[w].bolt=mask&16;
        cg.snap=scenario==0?NULL:&snap;*(void **)(base+0x40352c4)=scenario==0?NULL:raw;
        snap.ps.pm_flags=scenario==1?PMF_FOLLOW:0;*(int *)(raw+0x38)=snap.ps.pm_flags;
        snap.ps.weapon=w;*(int *)(raw+0xd0)=w;
        snap.ps.weaponstate=scenario==2?WEAPON_FIRING:WEAPON_READY;*(int *)(raw+0xd4)=snap.ps.weaponstate;
        cg.predictedPlayerState.weapon=scenario==3?(w+1)%64:w;*(int *)(base+0x407df64)=cg.predictedPlayerState.weapon;
        cg.predictedPlayerState.weaponstate=scenario==4?WEAPON_RELOADING:WEAPON_READY;*(int *)(base+0x407df68)=cg.predictedPlayerState.weaponstate;
        cg.weaponSelect=scenario==5?(w+1)%64:w;*(int *)(base+0x407ef70)=cg.weaponSelect;
        cg.time=10000;*(int *)(base+0x407dea4)=cg.time;
        cg.weaponSelectTime=cg.time-(scenario==6?delays[d]:4000);*(int *)(base+0x4080e44)=cg.weaponSelectTime;
        cg.tceFiremodeTime=cg.time-(scenario==7?4000:delays[d]);*(int *)(base+0x40a49f4)=cg.tceFiremodeTime;
        down();CG_FiremodeDown_f();
        if(cg.weaponSelect!=*(int *)(base+0x407ef70) || cg.tceFiremodeTime!=*(int *)(base+0x40a49f4))return 1;
        up();CG_FiremodeUp_f();
        if(cg.weaponSelect!=*(int *)(base+0x407ef70) || cg.tceFiremodeTime!=*(int *)(base+0x40a49f4))return 1;
        ++cases;
    }
    printf("%d complete original firemode down/up comparisons passed\n",cases);
    {
        unsigned char (__cdecl *selectable)(int)=(void *)(base+0x74230);
        int flags,owned,ammo,casesSelectable=0;
        const int masks[]={0,0x20,0x8000,0x400000,0x408020,0x80000};
        gearDef.parsed=1;
        for(w=0;w<64;++w)for(flags=0;flags<6;++flags)
        for(owned=0;owned<2;++owned)for(ammo=0;ammo<2;++ammo) {
            memset(cg.predictedPlayerState.weapons,0,sizeof(cg.predictedPlayerState.weapons));
            memset(base+0x407e294,0,8);
            if(owned) {
                COM_BitSet(cg.predictedPlayerState.weapons,w);
                ((unsigned *)(base+0x407e294))[w/32]|=1u<<(w%32);
            }
            cg.predictedPlayerState.eFlags=*(int *)(base+0x407df28)=masks[flags];
            memset(cg.predictedPlayerState.ammo,ammo?1:0,sizeof(cg.predictedPlayerState.ammo));
            memset(cg.predictedPlayerState.ammoclip,ammo?1:0,sizeof(cg.predictedPlayerState.ammoclip));
            if(!!CG_WeaponSelectable(w)!=!!selectable(w)) {
                printf("selectability mismatch weapon%d flags%x owned%d ammo%d\n",w,masks[flags],owned,ammo);return 3;
            }
            ++casesSelectable;
        }
        printf("%d production weapon selectability results matched original\n",casesSelectable);
    }
    {
        int (__cdecl *index)(int,int *,int *)=(void *)(base+0x74260);
        int weapon,nulls,b1,b2,c1,c2,r1,r2;
        if(sizeof(weapBanksMultiPlayer)!=880 || memcmp(weapBanksMultiPlayer,base+0x964b8,880))return 4;
        for(weapon=-32;weapon<256;++weapon)for(nulls=0;nulls<4;++nulls) {
            b1=b2=999;c1=c2=777;
            r1=index(weapon,nulls&1?NULL:&b1,nulls&2?NULL:&c1);
            r2=CG_WeaponIndex(weapon,nulls&1?NULL:&b2,nulls&2?NULL:&c2);
            if(r1!=r2 || b1!=b2 || c1!=c2)return 5;
        }
        puts("220 original bank entries and1152 production index results matched");
    }
    {
        int (__cdecl *equivalent)(int)=(void *)(base+0x74300);
        int weapon;
        for(weapon=-4096;weapon<=4096;++weapon)
            if(getEquivWeapon(weapon)!=equivalent(weapon))return 6;
        if(getEquivWeapon(2147483647)!=equivalent(2147483647) ||
           getEquivWeapon(-2147483647-1)!=equivalent(-2147483647-1))return 7;
        puts("8195 original equivalent-weapon results matched");
    }
    {
        void (__cdecl *zoom)(int,int)=(void *)(base+0x743c0);
        const float defaults[]={-5,0,4,20,59,60,75,90,91,120};
        int old,newWeapon,flag,f,count=0;
        for(old=0;old<64;++old)for(newWeapon=0;newWeapon<64;++newWeapon)
        for(flag=0;flag<2;++flag)for(f=0;f<10;++f) {
            cg.predictedPlayerState.eFlags=*(int *)(base+0x407df28)=flag?EF_ZOOMING:0;
            cg.time=*(int *)(base+0x407dea4)=12345;
            cg.zoomTime=*(int *)(base+0x407f1c8)=19;
            cg.zoomedScope=*(int *)(base+0x407f1c4)=37;
            cg.zoomval=*(float *)(base+0x407f1d0)=43;
            cg_zoomDefaultSniper.value=*(float *)(base+0x488cf08)=defaults[f];
            zoom(old,newWeapon);CG_SetSniperZoom(old,newWeapon);
            if(cg.zoomTime!=*(int *)(base+0x407f1c8) || cg.zoomedScope!=*(int *)(base+0x407f1c4) ||
               cg.zoomval!=*(float *)(base+0x407f1d0))return 10;
            ++count;
        }
        printf("%d complete original switch-zoom states matched\n",count);
    }
    {
        void (__cdecl *sound)(int,int)=(void *)(base+0x74490);
        int old,newWeapon,clip,alt,expected,calls,count=0;
        patch(base+0x64fb0,soundSink);patch(trap_S_StartSound,soundSink);
        cg.snap=&snap;*(void **)(base+0x40352c4)=raw;
        snap.ps.clientNum=*(int *)(raw+0xcc)=17;
        cgs.media.selectSound=*(int *)(base+0x25886f8)=123;
        for(old=0;old<64;++old)for(newWeapon=0;newWeapon<64;++newWeapon)
        for(clip=0;clip<2;++clip)for(alt=0;alt<2;++alt) {
            weapAlts[old]=((int *)(base+0x106e78))[old]=alt?newWeapon:0;
            cg.predictedPlayerState.ammoclip[old]=((int *)(base+0x407e150))[old]=clip;
            cg_weapons[newWeapon].switchSound=*(int *)(base+0x484b04c+newWeapon*0xfd0)=1000+newWeapon;
            soundCalls=0;soundId=-1;sound(old,newWeapon);calls=soundCalls;expected=soundId;
            soundCalls=0;soundId=-1;CG_PlaySwitchSound(old,newWeapon);
            if(soundCalls!=calls || soundId!=expected || (calls && (soundEntity!=17 || soundChannel!=CHAN_WEAPON)))return 11;
            ++count;
        }
        printf("%d original switch-sound decisions matched\n",count);
    }
    {
        void (__cdecl *finish)(int,int)=(void *)(base+0x74580);
        int old,newWeapon,scenario,bank,count=0;
        patch(base+0x74490,noopSwitch);patch(base+0x743c0,noopSwitch);
        patch(CG_PlaySwitchSound,noopSwitch);patch(CG_SetSniperZoom,noopSwitch);
        cg.snap=&snap;*(void **)(base+0x40352c4)=raw;
        snap.ps.eFlags=*(int *)(raw+0x94)=0;
        for(old=1;old<64;++old)for(newWeapon=1;newWeapon<64;++newWeapon)for(scenario=0;scenario<4;++scenario) {
            cg.time=*(int *)(base+0x407dea4)=20000;
            cg.binocZoomTime=*(int *)(base+0x408a39c)=scenario==3;
            cg.weaponSelectTime=*(int *)(base+0x4080e44)=123;
            cg.mortarImpactTime=*(int *)(base+0x40a2454)=75;
            cg.newCrosshairIndex=*(int *)(base+0x4081534)=0;
            cg.weaponSelect=*(int *)(base+0x407ef70)=old;
            cg.lastFiredWeapon=*(int *)(base+0x4080f0c)=scenario==0?old:0;
            cg.switchbackWeapon=*(int *)(base+0x4080f08)=scenario==1?newWeapon:2;
            cg.pmext.silencedSideArm=3;
            memset(cg.lastWeapSelInBank,0,sizeof(cg.lastWeapSelInBank));memset(base+0x4080f28,0,40);
            finish(old,newWeapon);CG_FinishWeaponChange(old,newWeapon);
            if(cg.weaponSelect!=*(int *)(base+0x407ef70) || cg.switchbackWeapon!=*(int *)(base+0x4080f08) ||
               cg.weaponSelectTime!=*(int *)(base+0x4080e44) || cg.mortarImpactTime!=*(int *)(base+0x40a2454))return 8;
            for(bank=0;bank<10;++bank)if(cg.lastWeapSelInBank[bank]!=((int *)(base+0x4080f28))[bank])return 9;
            ++count;
        }
        printf("%d original finish-switch state transitions matched (sound/zoom intercepted)\n",count);
    }
    {
        void (__cdecl *out)(int)=(void *)(base+0x75480);
        int weapon,mask,force,mounted,ammo,bank,cycle,valid,i,count=0;
        int choices[]={1,2,7,21,28,45};
        int ef,eo,en,ex,es;
        patch(base+0x74230,autoSelectable);patch(CG_WeaponSelectable,autoSelectable);
        patch(base+0x74580,autoFinishSink);patch(CG_FinishWeaponChange,autoFinishSink);
        patch(base+0x74870,autoNextSink);patch(CG_NextWeap,autoNextSink);
        memset(weapAlts,0,64*sizeof(int));memset(base+0x106e78,0,64*sizeof(int));
        cg.snap=&snap;*(void **)(base+0x40352c4)=raw;gearDef.parsed=1;
        for(weapon=1;weapon<=65;++weapon)for(mask=0;mask<64;++mask)
        for(force=0;force<2;++force)for(mounted=0;mounted<2;++mounted)for(ammo=0;ammo<2;++ammo) {
            memset(autoOwned,0,sizeof(autoOwned));
            for(i=0;i<6;++i)autoOwned[choices[i]]=(mask>>i)&1;
            bank=cycle=0;valid=CG_WeaponIndex(weapon,&bank,&cycle);
            /* Original has uninitialized bank on index failure. Do not execute
             * its undefined fallback: a selectable knife guarantees early exit. */
            if((!valid || (force && weapon>=55 && weapon<=59)) && !(mask&1))continue;
            cg.weaponSelect=*(int *)(base+0x407ef70)=weapon;
            cg.predictedPlayerState.weapon=*(int *)(base+0x407df64)=8;
            cg.predictedPlayerState.ammo[28]=*(int *)(base+0x407e0c0)=ammo;
            snap.ps.eFlags=*(int *)(raw+0x94)=mounted?0x1000000:0;
            autoFinish=autoOld=autoNew=autoNext=0;out(force);
            ef=autoFinish;eo=autoOld;en=autoNew;ex=autoNext;es=*(int *)(base+0x407ef70);
            autoFinish=autoOld=autoNew=autoNext=0;CG_OutOfAmmoChange(force);
            if(autoFinish!=ef || autoOld!=eo || autoNew!=en || autoNext!=ex || cg.weaponSelect!=es) {
                printf("out-of-ammo mismatch ID%d mask%d force%d mount%d ammo%d selected%d/%d finish%d/%d\n",
                    weapon,mask,force,mounted,ammo,cg.weaponSelect,es,autoFinish,ef);return 10;
            }
            ++count;
        }
        printf("%d original out-of-ammo decisions matched (selectable/finish/next intercepted)\n",count);
    }
    {
        void (__cdecl *bankCommand)(void)=(void *)(base+0x75260);
        int weapon,bank,scenario,mask,cache,i,count=0,ef,eo,en,et;
        int choices[]={45,2,1,9,55,56,52,14};
        patch(base+0x47cb0,autoArgv);patch(CG_Argv,autoArgv);
        for(weapon=0;weapon<64;++weapon)for(bank=0;bank<12;++bank)
        for(scenario=0;scenario<9;++scenario)for(mask=0;mask<8;++mask)for(cache=0;cache<2;++cache) {
            if(bank==10)continue; /* Original reads beyond bank table; production rejects it. */
            sprintf(autoBankArg,"%d",bank);memset(autoOwned,0,sizeof(autoOwned));
            for(i=0;i<8;++i)autoOwned[choices[i]]=(mask>>(i%3))&1;
            cg.snap=scenario==1?NULL:&snap;*(void **)(base+0x40352c4)=scenario==1?NULL:raw;
            snap.ps.pm_type=*(int *)(raw+0x30)=scenario==2?4:0;
            snap.ps.pm_flags=*(int *)(raw+0x38)=scenario==3?0x1000:0;
            snap.ps.eFlags=*(int *)(raw+0x94)=scenario==4?0x1000000:0;
            snap.ps.weaponstate=*(int *)(raw+0xd4)=scenario==5?9:scenario==6?1:scenario==7?3:0;
            cg.time=*(int *)(base+0x407dea4)=10000;
            cg.weaponSelectTime=*(int *)(base+0x4080e44)=scenario==8?9851:9850;
            cg_weaponCycleDelay.integer=*(int *)(base+0x400f62c)=150;
            cg.weaponSelect=*(int *)(base+0x407ef70)=weapon;
            memset(cg.lastWeapSelInBank,0,sizeof(cg.lastWeapSelInBank));memset(base+0x4080f28,0,40);
            if(cache && bank>0 && bank<10) {
                cg.lastWeapSelInBank[bank]=((int *)(base+0x4080f28))[bank]=weapBanksMultiPlayer[bank][0];
            }
            autoFinish=autoOld=autoNew=0;bankCommand();
            ef=autoFinish;eo=autoOld;en=autoNew;et=*(int *)(base+0x4080e44);
            autoFinish=autoOld=autoNew=0;CG_WeaponBank_f();
            if(autoFinish!=ef || autoOld!=eo || autoNew!=en || cg.weaponSelectTime!=et) {
                printf("bank mismatch ID%d bank%d scenario%d mask%d cache%d finish%d/%d new%d/%d\n",
                    weapon,bank,scenario,mask,cache,autoFinish,ef,autoNew,en);return 11;
            }
            ++count;
        }
        printf("%d original weaponbank command decisions matched\n",count);
    }
    FreeLibrary(dll);return 0;
}
