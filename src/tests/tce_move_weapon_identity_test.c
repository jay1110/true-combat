/* Compare the production PmoveSingle weapon transition block, not a copy of it. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf boundary;
static int count, args[3];
static int noop(void) { return 0; }
static void stop(void) { longjmp(boundary, 1); }
static void change(int from, int to, qboolean reload) {
    ++count; args[0]=from; args[1]=to; args[2]=reload;
}
static void hook(void *at, void *fn) {
    unsigned char *p=at; DWORD old,tmp;
    VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
    p[0]=0xe9; *(int *)(p+1)=(unsigned char *)fn-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5);
    VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv) {
    HMODULE dll; unsigned char *base;
    static unsigned char raw[0x700],rpm[0x200],ext[0x200];
    static playerState_t ps; static pmove_t move; static pmoveExt_t px;
    int w,dead,prone,clip,i,cases=0,expectedCount,expectedArgs[3],expectedWeapon;
    const int hooks[]={0x28b0,0x99e0,0xa3b0,0xc9a0,0xe210,0xe490,0xef60,0xf100};
    if(argc!=2 || !(dll=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)dll;
    for(i=0;i<sizeof(hooks)/sizeof(hooks[0]);++i) hook(base+hooks[i],noop);
    hook(base+0x8e90,change); hook(base+0xb120,stop);
    hook(BG_AnimUpdatePlayerStateConditions,noop); hook(PM_WeaponAmmoAvailable,noop);
    hook(PM_UpdateViewAngles,noop); hook(PM_CheckProne,noop); hook(PM_CheckDuck,noop);
    hook(PM_SetWaterLevel,noop); hook(PM_GroundTrace,noop); hook(PM_DeadMove,noop);
    hook(PM_BeginWeaponChange,change); hook(PM_CheckLadderMove,stop);
    gearDef.parsed=1;
    for(w=0;w<64;++w) for(dead=0;dead<2;++dead)
    for(prone=0;prone<2;++prone) for(clip=0;clip<2;++clip) {
        memset(raw,0,sizeof(raw)); memset(rpm,0,sizeof(rpm)); memset(ext,0,sizeof(ext));
        memset(&ps,0,sizeof(ps)); memset(&move,0,sizeof(move)); memset(&px,0,sizeof(px));
        move.ps=&ps; move.pmext=&px;
        *(void **)rpm=raw; *(void **)(rpm+4)=ext;
        ps.pm_type=*(int *)(raw+4)=dead?PM_DEAD:PM_NORMAL;
        ps.weapon=*(int *)(raw+0xa4)=w;
        ps.eFlags=*(int *)(raw+0x68)=prone?EF_PRONE:0;
        ps.stats[STAT_HEALTH]=*(int *)(raw+0xd0)=100;
        ps.ammoclip[28]=*(int *)(raw+0x300)=clip;
        move.cmd.serverTime=*(int *)(rpm+0xc)=1000;
        count=0; memset(args,0,sizeof(args));
        if(!setjmp(boundary)) ((void (__cdecl *)(void *))(base+0xc180))(rpm);
        expectedCount=count; memcpy(expectedArgs,args,sizeof(args)); expectedWeapon=*(int *)(raw+0xa4);
        count=0; memset(args,0,sizeof(args));
        if(!setjmp(boundary)) PmoveSingle(&move);
        if(count!=expectedCount || memcmp(args,expectedArgs,sizeof(args)) || ps.weapon!=expectedWeapon) {
            printf("weapon%d dead%d prone%d clip%d: change%d/%d args%d,%d,%d/%d,%d,%d weapon%d/%d\n",
                w,dead,prone,clip,count,expectedCount,args[0],args[1],args[2],
                expectedArgs[0],expectedArgs[1],expectedArgs[2],ps.weapon,expectedWeapon); return 1;
        }
        ++cases;
    }
    printf("%d production movement weapon identity cases passed\n",cases); return 0;
}
