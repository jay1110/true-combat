#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base,rawsnap[0x800],manager[0x300];
    static snapshot_t snap;static centity_t sourceManager;
    int (__cdecl *original)(int *,int *,int *);
    int w,n,s,m,follow,team,gm,i,cases=0,actual[3],expected[3],a,e;
    int sizes[]={0,1,7,30},amounts[]={0,1,29,30,89,90,-1,-31};
    tce_weaponDef_t *defs;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll;original=(void *)(base+0x28ed0);
    defs=(void *)(base+0x4c1df20);gearDef.parsed=1;
    memset(rawsnap,0,sizeof(rawsnap));memset(manager,0,sizeof(manager));
    *(void **)(base+0x40352c4)=rawsnap;*(int *)(base+0x40352a4)=0;
    cg.snap=&snap;cg.clientNum=0;
    for(w=0;w<64;++w)for(n=0;n<4;++n)for(s=0;s<2;++s)for(m=0;m<3;++m)
    for(follow=0;follow<2;++follow)for(team=1;team<3;++team)for(gm=0;gm<2;++gm)for(i=0;i<8;++i) {
        int j,client=follow?3:0;
        snap.ps.clientNum=client;*(int *)(rawsnap+0xcc)=client;
        snap.ps.eFlags=m==1?EF_MG42_ACTIVE:m==2?EF_MOUNTEDTANK:0;
        *(int *)(rawsnap+0x94)=snap.ps.eFlags;
        cg.predictedPlayerEntity.currentState.weapon=w;cg_entities[client].currentState.weapon=w;
        *(int *)(base+0x407e46c+0xe8)=w;*(int *)(base+0x488d800+client*0xa94+0xe8)=w;
        defs[w].maxclip=weaponDef[w].maxclip=sizes[n];
        defs[w].singleReload=weaponDef[w].singleReload=s;
        for(j=0;j<64;++j) {
            snap.ps.ammo[j]=amounts[i];snap.ps.ammoclip[j]=j+2;
            *(int *)(rawsnap+0x1bc+j*4)=amounts[i];*(int *)(rawsnap+0x2bc+j*4)=j+2;
        }
        cgs.gameManager=gm?&sourceManager:NULL;*(void **)(base+0x3fc4270)=gm?manager:NULL;
        sourceManager.currentState.otherEntityNum=17;sourceManager.currentState.otherEntityNum2=23;
        *(int *)(manager+0x8c)=17;*(int *)(manager+0x90)=23;
        cgs.clientinfo[client].team=team;*(int *)(base+0x2580a98+client*0x1c0)=team;
        e=original(expected,expected+1,expected+2);a=CG_PlayerAmmoValue(actual,actual+1,actual+2);
        if(a!=e || memcmp(actual,expected,sizeof(actual))) {
            fprintf(stderr,"HUD mismatch w%d max%d single%d mounted%d follow%d team%d gm%d amount%d: %d,%d,%d/%d,%d,%d\n",w,sizes[n],s,m,follow,team,gm,amounts[i],actual[0],actual[1],actual[2],expected[0],expected[1],expected[2]);return 1;
        }
        ++cases;
    }
    printf("%d production ammo HUD comparisons matched original DLL\n",cases);
    FreeLibrary(dll);return 0;
}
