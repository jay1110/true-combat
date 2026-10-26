#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>

int main(int argc,char **argv) {
    HMODULE dll; unsigned char *base;
    static unsigned char rawPs[4096],rawExt[256];
    static void *rawPm[2]; static playerState_t ps;
    static pmoveExt_t ext; static pmove_t movement;
    const int values[]={-1,0,1,2,15,16,49,50,51,199,200,201,1000};
    const int steps[]={0,1,2,16,50,200};
    unsigned int a,b,c,d,s,n=0; int module;
    void (__cdecl *original)(void);
    if(argc!=3) return 2;
    for(module=0;module<2;++module) {
    if(!(dll=LoadLibraryA(argv[module+1]))) return 2;
    base=(unsigned char *)dll;original=(void *)(base+(module?0x3b2e0:0x12fa0));
    rawPm[0]=rawPs;rawPm[1]=rawExt;*(void **)(base+(module?0x23cf4fc:0x4ba9edc))=rawPm;
    movement.ps=&ps;movement.pmext=&ext;pm=&movement;
    for(s=0;s<6;++s)for(a=0;a<13;++a)for(b=0;b<13;++b)for(c=0;c<13;++c)for(d=0;d<13;++d) {
        ps.pm_time=*(int *)(rawPs+16)=values[a];
        ps.legsTimer=*(int *)(rawPs+84)=values[b];
        ps.torsoTimer=*(int *)(rawPs+92)=values[c];
        ext.weapAnimTimer=*(int *)(rawExt+8)=values[d];
        ps.pm_flags=*(int *)(rawPs+12)=(int)(0xdeadbeefU^(n*2654435761U));
        pml.msec=*(int *)(base+(module?0x23cf488:0x4ba9e68))=steps[s];
        original();PM_DropTimers();
        if(ps.pm_time!=*(int *)(rawPs+16)||ps.pm_flags!=*(int *)(rawPs+12)||ps.legsTimer!=*(int *)(rawPs+84)||ps.torsoTimer!=*(int *)(rawPs+92)||ext.weapAnimTimer!=*(int *)(rawExt+8)) {
            printf("timer mismatch %u,%u,%u,%u step%d\n",a,b,c,d,steps[s]);return 1;
        }
        ++n;
    }
    FreeLibrary(dll);
    }
    printf("%u whole movement timer comparisons passed against both modules\n",n);return 0;
}
