/* Actual qagame production AddWeaponToPlayer and its original TC dependency
 * chain. No hooks/stubs replace the ownership, ammo or skill-bonus functions. */
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include "../game/botlib.h"
#include "../game/be_aas.h"
#include "../game/be_ea.h"
#include "../game/be_ai_char.h"
#include "../game/be_ai_chat.h"
#include "../game/be_ai_gen.h"
#include "../game/be_ai_goal.h"
#include "../game/be_ai_move.h"
#include "../game/be_ai_weap.h"
#include "../botai/botai.h"
//
#include "../botai/ai_main.h"

#include <windows.h>
#include <stdio.h>

extern qboolean AddWeaponToPlayer(gclient_t *, weapon_t, int, int, qboolean);
extern void SetWolfSpawnWeapons(gclient_t *);
static gclient_t client, expected;
static unsigned char raw[0x4000], rawExpected[0x4000];


static int randomValue, randomCalls;
static int __cdecl fixedRandom(void) { ++randomCalls; return randomValue; }
static int notifyCount, notifyClient;
static void __cdecl recordUserinfo(int index) { ++notifyCount; notifyClient=index; }
static int redirect(void *from, void *to) {
    DWORD old;
    unsigned char jump[5];jump[0]=0xe9;
    *(int *)(jump+1)=(int)((unsigned char *)to-(unsigned char *)from-5);
    if(!VirtualProtect(from,5,PAGE_EXECUTE_READWRITE,&old))return 0;
    memcpy(from,jump,5);FlushInstructionCache(GetCurrentProcess(),from,5);
    VirtualProtect(from,5,old,&old);return 1;
}
extern void Cmd_ResetSetup_f(gentity_t *);

int main(int argc, char **argv) {
    HMODULE dll;
    unsigned char *base;
    int (__cdecl *original)(void *, int, int, int, int);
    void (__cdecl *originalSpawn)(void *);
    int (__cdecl *originalAmmo)(int), (__cdecl *originalClip)(int);
    int w,a,c,select,pattern,k,parsed,count=0;
    const int amounts[] = {-1,0,1,3,30,90,999};
    const int selections[] = {0,1,-1};
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll;
    original=(void *)(base+0x4b950);originalSpawn=(void *)(base+0x4b9c0);
    originalClip=(void *)(base+0x2b100);originalAmmo=(void *)(base+0x2b140);
    /* Whole selectors use actual original and production class tables. */
    {
        int (__cdecl *primary)(void *)=(void *)(base+0x5c930);
        int (__cdecl *secondary)(void *)=(void *)(base+0x5ca20);
        int team,cls,skill,left,right,n=0;
        const int skills[]={-1,3,4,5};
        memset(&client,0,sizeof(client));memset(raw,0,sizeof(raw));
        for(team=0;team<4;++team)for(cls=0;cls<7;++cls)for(skill=0;skill<4;++skill)
        for(left=0;left<64;++left)for(right=0;right<64;++right) {
            client.sess.sessionTeam=*(int *)(raw+0xb30)=team;
            client.sess.playerType=*(int *)(raw+0xb40)=cls;
            client.sess.skill[SK_HEAVY_WEAPONS]=*(int *)(raw+0xbb8)=skills[skill];
            client.ps.weapons[0]=client.ps.weapons[1]=0;
            COM_BitSet(client.ps.weapons,left);COM_BitSet(client.ps.weapons,right);
            memcpy(raw+0x3d4,client.ps.weapons,8);
            if(primary(raw)!=G_GetPrimaryWeaponForClient(&client)||
               secondary(raw)!=G_GetSecondaryWeaponForClient(&client)) {
                fprintf(stderr,"weapon selector team%d class%d skill%d bits%d/%d mismatch\n",team,cls,skills[skill],left,right);return 1;
            }
            ++n;
        }
        printf("%d complete primary and secondary selector pairs matched\n",n);
    }
    gearDef.parsed=1;
    /* Neither function may touch unassigned or spectator clients. */
    for(k=0;k<2;++k) {
        memset(&client,0x5a,sizeof(client));memset(raw,0x5a,sizeof(raw));
        client.ps.clientNum=0;client.sess.playerType=0;
        client.sess.sessionTeam=k?TEAM_SPECTATOR:TEAM_FREE;
        *(int *)(raw+0xa0)=0;*(int *)(raw+0xb40)=0;
        *(int *)(raw+0xb30)=client.sess.sessionTeam;
        expected=client;memcpy(rawExpected,raw,sizeof(raw));
        originalSpawn(raw);SetWolfSpawnWeapons(&client);
        if(memcmp(raw,rawExpected,sizeof(raw)) || memcmp(&client,&expected,sizeof(client))) {
            fprintf(stderr,"Non-playing spawn changed client team%d\n",client.sess.sessionTeam);return 1;
        }
    }
    for(w=-32;w<=96;++w) {
        if(originalClip(w)!=(int)BG_FindClipForWeapon((weapon_t)w) ||
           originalAmmo(w)!=(int)BG_FindAmmoForWeapon((weapon_t)w)) {
            fprintf(stderr,"Original qagame ammo mapping mismatch: %d\n",w);return 1;
        }
    }
    for(parsed=0;parsed<2;++parsed)for(w=0;w<64;++w)for(a=0;a<7;++a)for(c=0;c<7;++c)
    for(select=0;select<3;++select)for(pattern=0;pattern<3;++pattern) {
        gearDef.parsed=parsed;
        memset(&client,0x5a,sizeof(client));
        memset(raw,0x5a,sizeof(raw));
        for(k=0;k<64;++k) {
            client.ps.ammo[k]=k+100;client.ps.ammoclip[k]=k+200;
        }
        client.ps.weapons[0]=pattern==0?0:pattern==1?-1:0x13579bdf;
        client.ps.weapons[1]=pattern==0?0:pattern==1?-1:0x2468ace0;
        client.ps.weapon=17;
        for(k=0;k<SK_NUM_SKILLS;++k)client.sess.skill[k]=(a+c+k)%5;
        memcpy(raw+0x190,client.ps.ammo,64*4);
        memcpy(raw+0x290,client.ps.ammoclip,64*4);
        memcpy(raw+0x3d4,client.ps.weapons,8);
        *(int *)(raw+0xa4)=client.ps.weapon;
        expected=client;memcpy(rawExpected,raw,sizeof(raw));
        if(original(raw,w,amounts[a],amounts[c],selections[select])!=1 ||
           AddWeaponToPlayer(&client,(weapon_t)w,amounts[a],amounts[c],selections[select])!=qtrue)return 1;
        memcpy(expected.ps.ammo,raw+0x190,64*4);
        memcpy(expected.ps.ammoclip,raw+0x290,64*4);
        memcpy(expected.ps.weapons,raw+0x3d4,8);
        expected.ps.weapon=*(int *)(raw+0xa4);
        memcpy(rawExpected+0x190,client.ps.ammo,64*4);
        memcpy(rawExpected+0x290,client.ps.ammoclip,64*4);
        memcpy(rawExpected+0x3d4,client.ps.weapons,8);
        *(int *)(rawExpected+0xa4)=client.ps.weapon;
        if(memcmp(&client,&expected,sizeof(client)) || memcmp(raw,rawExpected,sizeof(raw))) {
            fprintf(stderr,"Spawn assignment mismatch w%d ammo%d clip%d selected%d pattern%d\n",
                w,amounts[a],amounts[c],selections[select],pattern);return 1;
        }
        ++count;
    }


    {
        int (__cdecl *klass)(int)=(void *)(base+0x304c0);
        int (__cdecl *back)(int)=(void *)(base+0x2e9c0);
        int (__cdecl *sidearm)(int,int)=(void *)(base+0x30480);
        int (__cdecl *available)(int,int,int,int,int)=(void *)(base+0x2e900);
        const int values[]={(-2147483647-1),-1,0,1,3,4,2147483647};
        unsigned seed=1;
        int i,j,w2,v1,v2,wt,pt,present,helperCount=0;
        for(i=-128;i<=128;++i) {
            if(klass(i)!=BG_WolfClassToTCE(i))return 1;
            ++helperCount;
        }
        for(i=0;i<100000;++i) {
            seed=seed*1664525u+1013904223u;
            if(back((int)seed)!=BG_WeapToWeaponOnBack((int)seed) ||
               klass((int)seed)!=BG_WolfClassToTCE((int)seed))return 1;
            helperCount+=2;
        }
        for(w=0;w<64;++w)for(w2=0;w2<64;++w2)
        for(v1=0;v1<7;++v1)for(v2=0;v2<7;++v2) {
            weaponDef[w].loadoutWeight=values[v1];
            weaponDef[w2].loadoutWeight=values[v2];
            ((tce_weaponDef_t *)(base+0x24435c0))[w]=weaponDef[w];
            ((tce_weaponDef_t *)(base+0x24435c0))[w2]=weaponDef[w2];
            if(sidearm(w,w2)!=BG_SidearmAvailableForPrimary(w,w2))return 1;
            ++helperCount;
        }
        for(present=0;present<2;++present) {
            for(j=0;j<64;++j)gearDef.weaponFile[j][0]=present?'w':0;
            memcpy(base+0x23cf5a0,&gearDef,sizeof(gearDef));
            for(w=-16;w<=80;++w)for(wt=-1;wt<=3;++wt)for(pt=0;pt<=3;++pt)
            for(v1=0;v1<7;++v1)for(v2=0;v2<7;++v2) {
                if(available(w,values[v1],wt,values[v2],pt)!=
                   BG_WeaponIsAvailable(w,values[v1],wt,values[v2],pt))return 1;
                ++helperCount;
            }
        }
        printf("%d original qagame class/availability/sidearm/back comparisons matched\n",helperCount);
    }
    /* Live regression: Specops requests primary33/secondary39. Stock Gear
     * rejects terrorist-only39 and picks equivalent2; a both-team override
     * legitimately preserves39 in BOTH original and production selectors. */
    {
        int override,j;
        for(override=0;override<2;++override) {
            memset(&client,0,sizeof(client));memset(raw,0,sizeof(raw));
            memset(&gearDef,0,sizeof(gearDef));memset(weaponDef,0,sizeof(weaponDef));
            for(j=0;j<64;++j) {
                gearDef.weaponFile[j][0]='w';gearDef.team[j]=2;
                gearDef.requiredSkill[j][0]=gearDef.requiredSkill[j][1]=gearDef.requiredSkill[j][2]=1;
                weaponDef[j].startingClip=15;weaponDef[j].startingAmmo=3;
            }
            gearDef.team[2]=1;gearDef.team[39]=override?2:0;
            gearDef.equivalentWeapon[39]=2;gearDef.equivalentWeapon[2]=39;
            memcpy(base+0x24435c0,weaponDef,sizeof(weaponDef));
            memcpy(base+0x23cf5a0,&gearDef,sizeof(gearDef));
            client.sess.sessionTeam=2;client.sess.playerType=0;
            client.sess.playerWeapon=33;client.sess.playerWeapon2=39;
            *(int *)(raw+0xb30)=2;*(int *)(raw+0xb40)=0;
            *(int *)(raw+0xb44)=33;*(int *)(raw+0xb48)=39;
            g_entities[0].r.svFlags=*(int *)(base+0x226e3c8)=0;
            g_knifeonly.integer=*(int *)(base+0x22245ec)=0;
            g_gametype.integer=*(int *)(base+0x6d8d4c)=7;
            originalSpawn(raw);SetWolfSpawnWeapons(&client);
            if(client.sess.playerWeapon2!=*(int *)(raw+0xb48)||
               client.sess.playerWeapon2!=(override?39:2)||
               memcmp(client.ps.weapons,raw+0x3d4,8)||
               memcmp(client.ps.ammo,raw+0x190,256)||
               memcmp(client.ps.ammoclip,raw+0x290,256)) {
                fprintf(stderr,"Specops sidearm Gear override%d mismatch\n",override);return 1;
            }
        }
        puts("2 stock/team-override Specops secondary39 spawn comparisons matched");
    }
    /* Exercise the actual production TC selector against the entire
     * original function, including all untouched client bytes. */
    {
        int t,pc,p,s,variant,j,mode,bot,oldRandomCalls,n=0;
        const int botFlags[]={0,SVF_BOT,SVF_BOT|SVF_POW,SVF_POW};
        const int modes[]={2,5,7};
        const int flags[] = {0,0x100,0x400,0x500};
        for(j=0;j<64;++j) {
            weaponDef[j].startingAmmo=j%7;
            weaponDef[j].startingClip=j%31+1;
            weaponDef[j].loadoutWeight=j%7;
            gearDef.weaponFile[j][0]='w';
            gearDef.team[j]=j%3;
            gearDef.equivalentWeapon[j]=(j+7)%64;
            for(pc=0;pc<3;++pc)gearDef.requiredSkill[j][pc]=(j+pc)%5;
        }
        memcpy(base+0x24435c0,weaponDef,sizeof(weaponDef));
        memcpy(base+0x23cf5a0,&gearDef,sizeof(gearDef));
        *(int *)(base+0x6d8d4c)=7;
        g_gametype.integer=7;
        if(!redirect(base+0xa20e4,fixedRandom) || !redirect(rand,fixedRandom))return 2;
        for(bot=0;bot<4;++bot)for(mode=0;mode<3;++mode)for(t=1;t<=2;++t)for(pc=0;pc<7;++pc)
        for(p=0;p<64;++p)for(s=0;s<64;++s)for(variant=0;variant<4;++variant) {
            memset(&client,0x5a,sizeof(client));memset(raw,0x5a,sizeof(raw));
            client.ps.clientNum=0;client.sess.sessionTeam=t;client.sess.playerType=pc;
            client.sess.playerWeapon=p;client.sess.playerWeapon2=s;
            client.sess.playerWeapon3=(p+s)%2 ? 36 : 0;
            g_gametype.integer=modes[mode];*(int *)(base+0x6d8d4c)=modes[mode];
            client.ps.stats[STAT_TCE_FLAGS]=flags[variant];
            for(j=0;j<3;++j)client.sess.skillpoints[j]=(float)((p+s+j)%6)-1.25f;
            g_knifeonly.integer=((p+s)%13==0);
            *(int *)(base+0x22245ec)=g_knifeonly.integer;
            *(int *)(base+0x226e3c8)=botFlags[bot];
            g_entities[0].r.svFlags=botFlags[bot];
            *(int *)(base+0x2458a1c)=0x5a;botstates[0].isPOW=0x5a;
            randomValue=(p*997+s*511+mode*32767)&32767;
            *(int *)(raw+0xa0)=0;*(int *)(raw+0xb30)=t;*(int *)(raw+0xb40)=pc;
            *(int *)(raw+0xb44)=p;*(int *)(raw+0xb48)=s;
            *(int *)(raw+0xf4)=flags[variant];
            *(int *)(raw+0xde4)=client.sess.playerWeapon3;
            memcpy(raw+0xb68,client.sess.skillpoints,12);
            expected=client;memcpy(rawExpected,raw,sizeof(raw));
            randomCalls=0;originalSpawn(raw);oldRandomCalls=randomCalls;
            randomCalls=0;SetWolfSpawnWeapons(&client);
            if(randomCalls!=oldRandomCalls || botstates[0].isPOW!=*(int *)(base+0x2458a1c)) {
                fprintf(stderr,"Bot side effect mismatch flags%d\n",botFlags[bot]);return 1;
            }
#define FIELD(offset,member) \
            memcpy(&expected.member,raw+offset,sizeof(client.member)); \
            memcpy(rawExpected+offset,&client.member,sizeof(client.member))
            FIELD(0x478,ps.classWeaponTime);FIELD(0xf4,ps.stats[STAT_TCE_FLAGS]);
            FIELD(0xe4,ps.stats[STAT_PLAYER_CLASS]);FIELD(0x420,ps.teamNum);
            FIELD(0x190,ps.ammo);FIELD(0x290,ps.ammoclip);
            FIELD(0x3d4,ps.weapons);FIELD(0x10c,ps.stats[15]);
            FIELD(0xa4,ps.weapon);FIELD(0xa8,ps.weaponstate);
            FIELD(0x3b4,ps.holdable[9]);FIELD(0x3b8,ps.holdable[10]);
            FIELD(0xb44,sess.playerWeapon);FIELD(0xb48,sess.playerWeapon2);
#undef FIELD
            if(memcmp(&client,&expected,sizeof(client)) || memcmp(raw,rawExpected,sizeof(raw))) {
                fprintf(stderr,"TC spawn mismatch team%d class%d primary%d secondary%d flags%x\n",t,pc,p,s,flags[variant]);
                for(j=0;j<sizeof(raw);++j)if(raw[j]!=rawExpected[j]) {
                    fprintf(stderr,"raw offset%x original%02x production%02x\n",j,raw[j],rawExpected[j]);break;
                }
                return 1;
            }
            ++n;
        }
        printf("%d complete human/bot/POW three-gametype spawn comparisons matched\n",n);
    }
    printf("%d complete production spawn assignments and258 original qagame mappings matched\n",count);

    {
        void (__cdecl *set)(void *,int,int,int,int)=(void *)(base+0x50830);
        void (__cdecl *reset)(void *)=(void *)(base+0x50ae0);
        unsigned char *ent=base+0x226e2a0;
        int team,mask,update,primary,secondary,third,which,n=0,oldCount,oldClient;
        if(!redirect(base+0x4bf70,recordUserinfo) || !redirect(ClientUserinfoChanged,recordUserinfo))return 2;
        g_heavyWeaponRestriction.integer=100;*(int *)(base+0x7e22cc)=100;
        level.numConnectedClients=0;*(int *)(base+0x7fec0c)=0;
        g_entities[0].client=&client;
        memset(ent,0,0x57c);*(void **)(ent+0x190)=raw;
        for(which=0;which<2;++which)for(team=1;team<=3;++team)
        for(mask=0;mask<8;++mask)for(update=0;update<2;++update)
        for(primary=0;primary<64;++primary)for(secondary=0;secondary<64;secondary+=7)
        for(third=0;third<=36;third+=18) {
            memset(&client,0x5a,sizeof(client));memset(raw,0x5a,sizeof(raw));
            client.sess.sessionTeam=team;*(int *)(raw+0xb30)=team;
            client.sess.playerType=2;*(int *)(raw+0xb40)=2;
            client.sess.playerWeapon=primary;*(int *)(raw+0xb44)=primary;
            client.sess.playerWeapon2=secondary;*(int *)(raw+0xb48)=secondary;
            client.sess.playerWeapon3=third;*(int *)(raw+0xde4)=third;
            client.sess.latchPlayerWeapon=primary+(mask&1?1:0);
            client.sess.latchPlayerWeapon2=secondary+(mask&2?1:0);
            client.sess.latchPlayerWeapon3=third+(mask&4?1:0);
            *(int *)(raw+0xb54)=client.sess.latchPlayerWeapon;
            *(int *)(raw+0xb58)=client.sess.latchPlayerWeapon2;
            *(int *)(raw+0xde8)=client.sess.latchPlayerWeapon3;
            expected=client;memcpy(rawExpected,raw,sizeof(raw));
            notifyCount=0;notifyClient=-1;
            if(which)reset(ent);else set(ent,primary,secondary,third,update);
            oldCount=notifyCount;oldClient=notifyClient;notifyCount=0;notifyClient=-1;
            if(which)Cmd_ResetSetup_f(&g_entities[0]);
            else G_SetClientWeapons(&g_entities[0],primary,secondary,third,update);
#define FIELD(offset,member) \
            memcpy(&expected.member,raw+offset,sizeof(client.member)); \
            memcpy(rawExpected+offset,&client.member,sizeof(client.member))
            FIELD(0xb50,sess.latchPlayerType);FIELD(0xb54,sess.latchPlayerWeapon);
            FIELD(0xb58,sess.latchPlayerWeapon2);FIELD(0xde8,sess.latchPlayerWeapon3);
#undef FIELD
            if(memcmp(raw,rawExpected,sizeof(raw)) || memcmp(&client,&expected,sizeof(client)) ||
               oldCount!=notifyCount || oldClient!=notifyClient) {
                fprintf(stderr,"Equipment selection mismatch fn%d team%d mask%d update%d p%d s%d third%d\n",which,team,mask,update,primary,secondary,third);return 1;
            }
            ++n;
        }
        notifyCount=0;reset(NULL);Cmd_ResetSetup_f(NULL);
        *(void **)(ent+0x190)=NULL;g_entities[0].client=NULL;
        reset(ent);Cmd_ResetSetup_f(&g_entities[0]);
        if(notifyCount)return 1;
        printf("%d complete equipment selection/reset and notification comparisons matched\n",n);
    }
    FreeLibrary(dll);return 0;
}
