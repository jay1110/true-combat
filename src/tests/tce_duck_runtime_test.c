/* Exercise the production static function against the original DLL. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int blocked, calls, traceClient, traceMask;
static float traceVectors[12];
static void traceDuck(trace_t *out, const vec3_t start, const vec3_t mins,
                      const vec3_t maxs, const vec3_t end, int client, int mask) {
    ++calls; traceClient=client; traceMask=mask;
    memcpy(traceVectors,start,12); memcpy(traceVectors+3,mins,12);
    memcpy(traceVectors+6,maxs,12); memcpy(traceVectors+9,end,12);
    memset(out,0,sizeof(*out)); out->allsolid=blocked;
}
int main(int argc,char **argv) {
    HMODULE dll; unsigned char *base, raw[0x700], expected[0x700], rawpm[0x200], rawext[0x200];
    playerState_t ps; pmove_t move; pmoveExt_t ext;
    int type,flags,ef,mode,latch,up,w,solid,cases=0,expectedCalls,expectedClient,expectedMask;
    int ups[]={-128,-1,0,10,11,127}, eflags[]={0,0x8000,0x1000000,0x1008000,0x80000};
    float expectedVectors[12]; void (__cdecl *original)(void);
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll; original=(void *)(base+0xf100); gearDef.parsed=1;
    for(type=0;type<6;++type)for(flags=0;flags<8;++flags)for(ef=0;ef<5;++ef)
    for(mode=0;mode<2;++mode)for(latch=0;latch<4;++latch)for(up=0;up<6;++up)
    for(w=0;w<64;++w)for(solid=0;solid<2;++solid) {
        memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));
        memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));
        ps.pm_type=type;ps.pm_flags=flags;ps.eFlags=eflags[ef];ps.weapon=w;
        ps.persistant[14]=mode?4:0;ps.stats[STAT_TCE_WEAPON_FLAGS]=0x81 | (latch<<9);
        ps.mins[0]=-17;ps.mins[1]=-19;ps.mins[2]=-23;
        ps.maxs[0]=17;ps.maxs[1]=19;ps.maxs[2]=49;
        ps.crouchMaxZ=25.5f;ps.standViewHeight=40.75f;ps.crouchViewHeight=16.25f;ps.deadViewHeight=-9.75f;
        ps.origin[0]=57;ps.origin[1]=-100;ps.origin[2]=7;ps.clientNum=13;ps.jumpTime=101;
        /* Relevant playerState offsets match the original network ABI. */
        memcpy(raw,&ps,0x420);*(int *)(raw+0x47c)=ps.jumpTime;
        move.ps=&ps;move.pmext=&ext;move.cmd.upmove=ups[up];move.cmd.serverTime=1729;
        move.tracemask=0x1234;move.trace=traceDuck;ext.jumpTime=202;pm=&move;
        *(void **)(base+0x4ba9edc)=rawpm;*(void **)rawpm=raw;*(void **)(rawpm+4)=rawext;
        *(int *)(rawpm+0xc)=1729;rawpm[0x22]=(unsigned char)ups[up];
        *(int *)(rawpm+0x44)=move.tracemask;*(void **)(rawpm+0x124)=traceDuck;*(int *)(rawext+4)=202;
        blocked=solid;calls=0;original();expectedCalls=calls;expectedClient=traceClient;expectedMask=traceMask;
        memcpy(expectedVectors,traceVectors,sizeof(traceVectors));memcpy(expected,raw,sizeof(raw));
        calls=0;PM_CheckDuck();
        /* Whole network prefix, bounds, jump clocks and callback arguments. */
        if(memcmp(expected,&ps,0x420) || memcmp(rawpm+0xf4,move.mins,12) || memcmp(rawpm+0x100,move.maxs,12) ||
           *(int *)(raw+0x47c)!=ps.jumpTime || *(int *)(rawext+4)!=ext.jumpTime || calls!=expectedCalls ||
           (calls && (memcmp(traceVectors,expectedVectors,sizeof(traceVectors)) || traceClient!=expectedClient || traceMask!=expectedMask))) {
            fprintf(stderr,"Duck mismatch type%d flags%d ef%x mode%d latch%d up%d weapon%d blocked%d\n",type,flags,eflags[ef],mode,latch,ups[up],w,solid);return 1;
        }
        ++cases;
    }
    printf("%d complete production duck comparisons matched original DLL\n",cases);
    FreeLibrary(dll);return 0;
}
