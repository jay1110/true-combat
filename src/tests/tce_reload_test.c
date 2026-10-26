#include "../game/tce_reload.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static tce_reloadEffects_t seen;
static int order[3], count;
static unsigned seed=0x852479;
static unsigned next(void) { seed=seed*1664525u+1013904223u; return seed>>8; }
static void body(void *ps, void *model, int anim, int continued, int force) {
    if(continued || force!=1 || count>=3) abort();
    seen.bodyAnimation=anim; order[count++]=1;
}
static void animation(int anim) { seen.weaponAnimation=anim; order[count++]=2; }
static void event(int id) { seen.event=id; order[count++]=3; }
static int emptyAnim(int weapon) { return 71; }
static int nonemptyAnim(void) { return 72; }
static void missing(int code, const char *format, int weapon) {
    if(code!=2 || weapon!=36) abort();
    seen.missingItem=1;
}
/* In-memory dependency interception only; original DLL on disk is immutable. */
static void hook(unsigned char *entry,void *fn) {
    DWORD old,tmp;
    if(!VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&old)) abort();
    entry[0]=0xe9; *(int *)(entry+1)=(unsigned char *)fn-entry-5;
    FlushInstructionCache(GetCurrentProcess(),entry,5);
    VirtualProtect(entry,5,old,&tmp);
}
int main(int argc,char **argv) {
    HMODULE module;
    unsigned char *base, ps[0x600], move[0x140], character[0x900], ext[0x90];
    int ammo[64],clip[64],i,j,k,weapon,expectedState,expectedTime;
    tce_weaponDef_t defs[64];
    tce_reloadState_t s;
    tce_reloadEffects_t effects;
    void (__cdecl *original[2])(int);
    if(argc!=2 || !(module=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)module;
    memset(ps,0,sizeof(ps));memset(move,0,sizeof(move));memset(ext,0,sizeof(ext));
    *(void **)move=ps;*(void **)(move+4)=ext;*(void **)(base+0x4ba9edc)=move;
    original[0]=(void *)(base+0x89e0);original[1]=(void *)(base+0x89a0);
    for(k=0;k<2;++k) for(i=0;i<64000;++i) {
        int anim=(int)(next()%1100)-20,current=(int)(next()%1100)-20;
        int type=(int)(next()%7)-1,timer=(int)(next()%5)-2,cmd=next()%256;
        if(i%3==0) anim=current & ~512;
        if(i%5==0) cmd=0;
        *(int *)(ps+4)=type;*(int *)(ps+0x480)=current;
        *(int *)(ext+8)=timer;move[0x12]=(unsigned char)cmd;
        original[k](anim);
        j=k ? TCE_PM_ContinueWeaponAnim(anim,current,type,timer,cmd) : TCE_PM_StartWeaponAnim(anim,current,type,timer,cmd);
        if(j!=*(int *)(ps+0x480)) { fprintf(stderr,"animation transition mismatch %d/%d\n",k,i);return 1; }
    }
    original[0]=(void *)(base+0x96d0);original[1]=(void *)(base+0x9870);
    hook(base+0x2520,body);hook(base+0x89a0,animation);hook(base+0x8680,event);
    hook(base+0x8870,emptyAnim);hook(base+0x8820,nonemptyAnim);
    hook(base+0x47c30,missing);
    memset(move,0,sizeof(move));memset(character,0,sizeof(character));
    *(void **)move=ps;*(void **)(move+8)=character;*(void **)(base+0x4ba9edc)=move;
    for(k=0;k<2;++k) for(i=0;i<64000;++i) {
        memset(ps,0,sizeof(ps)); memset(defs,0,sizeof(defs));
        for(j=0;j<64;++j) {
            ammo[j]=(int)(next()%70)-1;clip[j]=(int)(next()%40)-1;
            defs[j].maxclip=next()%40;defs[j].reloadTime=next()%5000;
            defs[j].maxHeat=next()%1000;defs[j].singleReload=next()%3;
        }
        weapon=k ? i%64 : i%70-3;
        s.state=(i/70)%14; s.time=(int)(next()%6000)-100;
        s.prone=next()%2;s.noClips=next()%3;
        s.emptyAnimation=71;s.nonemptyAnimation=72;s.ammo=ammo;s.clip=clip;
        *(int *)(ps+0xa8)=s.state;*(int *)(ps+0x2c)=s.time;
        *(int *)(ps+0x68)=s.prone ? 0x80000 : 0;
        *(int *)(ps+0xa4)=8;*(int *)(move+0x50)=s.noClips;
        memcpy(ps+0x190,ammo,sizeof(ammo));memcpy(ps+0x290,clip,sizeof(clip));
        memcpy(base+0x4c1df20,defs,sizeof(defs));
        seen.bodyAnimation=seen.weaponAnimation=seen.event=-1; seen.missingItem=0; count=0;
        original[k](weapon);
        expectedState=*(int *)(ps+0xa8);expectedTime=*(int *)(ps+0x2c);
        if(k) TCE_PM_ReloadSingleRound(weapon,&s,defs,&effects);
        else TCE_PM_BeginWeaponReload(weapon,&s,defs,&effects);
        if(s.state!=expectedState || s.time!=expectedTime || memcmp(&effects,&seen,sizeof(seen))) {
            fprintf(stderr,"reload transition %d case%d weapon%d state%d/%d time%d/%d effects%d,%d,%d/%d,%d,%d\n",
                k,i,weapon,s.state,expectedState,s.time,expectedTime,effects.bodyAnimation,effects.weaponAnimation,effects.event,seen.bodyAnimation,seen.weaponAnimation,seen.event);return 1;
        }
        for(j=1;j<count;++j) if(order[j]<=order[j-1]) return 1;
    }
    FreeLibrary(module);puts("128000 reload and 128000 weapon-animation transition comparisons passed.");return 0;
}
