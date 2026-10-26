#include "../game/tce_reload.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static tce_reloadEffects_t seen;
static tce_reloadAction_t action;
static int transfers, rounds, begins, changes;
static unsigned seed=0x543876;
static unsigned next(void) { seed=seed*1664525u+1013904223u;return seed>>8; }
static void hook(unsigned char *p,void *fn) {
    DWORD old,tmp;
    if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old)) abort();
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static void body(void *ps,void *model,int anim,int continued,int force) {
    if(continued || force!=1) abort();seen.bodyAnimation=anim;
}
static void animation(int anim) { seen.weaponAnimation=anim; }
static void event(int id) { seen.event=id; }
static int idle(int weapon) { return 6; }
static void begin(int weapon) { ++begins;action.action=TCE_RELOAD_BEGIN; }
static void roundStep(int weapon) { ++rounds;action.action=TCE_RELOAD_ROUND; }
static void transfer(int weapon) { ++transfers; }
static void change(int from,int to,int reload) {
    if(reload!=1) abort();++changes;action.action=TCE_RELOAD_CHANGE;action.alternate=to;
}
int main(int argc,char **argv) {
    HMODULE module;
    unsigned char *base,ps[0x600],pm[0x140],character[0x900];
    tce_weaponDef_t defs[64]; tce_reloadState_t s;
    tce_reloadEffects_t effects; tce_reloadAction_t expectedAction;
    int ammo[64],clip[64],limits[64],alts[64],i,j,k,w;
    void (__cdecl *finish)(void);void (__cdecl *check)(int);
    DWORD old;
    if(argc!=2 || !(module=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)module;finish=(void *)(base+0x12e70);check=(void *)(base+0x9350);
    for(i=-10;i<100;++i) {
        int (__cdecl *originalIdle)(int)=(void *)(base+0x86c0);
        if(originalIdle(i)!=TCE_PM_IdleAnimForWeapon(i)) return 1;
    }
    memset(pm,0,sizeof(pm));memset(character,0,sizeof(character));
    *(void **)pm=ps;*(void **)(pm+8)=character;*(void **)(base+0x4ba9edc)=pm;
    hook(base+0x89e0,animation);hook(base+0x86c0,idle);
    for(k=0;k<2;++k) {
        if(k) {
            hook(base+0x2520,body);hook(base+0x89a0,animation);hook(base+0x8680,event);
            hook(base+0x96d0,begin);hook(base+0x9870,roundStep);hook(base+0x9250,transfer);
            hook(base+0x8e90,change);
            if(!VirtualProtect(base+0x968b8,64*0x54,PAGE_READWRITE,&old) ||
               !VirtualProtect(base+0x106e78,256,PAGE_READWRITE,&old)) return 2;
        }
        for(i=0;i<128000;++i) {
            memset(ps,0,sizeof(ps));memset(defs,0,sizeof(defs));memset(&s,0,sizeof(s));
            for(j=0;j<64;++j) {
                ammo[j]=(int)(next()%42)-1;clip[j]=(int)(next()%12)-1;
                if(i%4==0) ammo[j]=0;
                defs[j].singleReload=next()%2;defs[j].bolt=next()%2;
                defs[j].maxclip=next()%12;defs[j].coolRate=(int)(next()%5)*100;
                defs[j].maxHeat=next()%1000;defs[j].reloadTime=next()%3000;
                limits[j]=next()%12;alts[j]=next()%64;
            }
            w=i%64;s.state=(i/64)%14;s.time=(int)(next()%7)-4;
            s.noClips=i%5==0;s.reloadRequested=next()%2;s.attack=next()%2;s.attackHeld=next()%2;
            s.idleAnimation=6;s.ammo=ammo;s.clip=clip;
            memcpy(ps+0x190,ammo,sizeof(ammo));memcpy(ps+0x290,clip,sizeof(clip));
            *(int *)(ps+0xa4)=w;*(int *)(ps+0xa8)=s.state;*(int *)(ps+0x2c)=s.time;
            *(int *)(ps+0xc)=s.attackHeld ? 0x400 : 0;
            *(int *)(pm+0x50)=s.noClips;pm[0x10]=(unsigned char)s.attack;pm[0x11]=s.reloadRequested ? 8 : 0;
            memcpy(base+0x4c1df20,defs,sizeof(defs));
            seen.bodyAnimation=seen.weaponAnimation=seen.event=-1;seen.missingItem=0;
            action.action=TCE_RELOAD_NONE;action.alternate=0;transfers=rounds=begins=changes=0;
            if(k) {
                for(j=57;j<=59;++j) {
                    *(int *)(base+0x968b8+j*0x54)=limits[j];*(int *)(base+0x106e78+j*4)=alts[j];
                }
                check(w);
                TCE_PM_CheckForReload(w,&s,defs,limits,alts,&effects,&expectedAction);
                if(memcmp(&action,&expectedAction,sizeof(action)) || transfers!=rounds || rounds>1 || begins>1 || changes>1) {
                    fprintf(stderr,"check action mismatch %d weapon%d action%d/%d\n",i,w,action.action,expectedAction.action);return 1;
                }
            } else {
                finish();TCE_PM_FinishWeaponReload(w,&s,defs,ammo,clip,&effects);
                if(memcmp(ps+0x190,ammo,sizeof(ammo)) || memcmp(ps+0x290,clip,sizeof(clip))) {
                    fprintf(stderr,"finish ammo mismatch %d weapon%d\n",i,w);return 1;
                }
            }
            if(s.state!=*(int *)(ps+0xa8) || s.time!=*(int *)(ps+0x2c) || memcmp(&seen,&effects,sizeof(seen))) {
                fprintf(stderr,"controller mismatch %d/%d weapon%d state%d/%d time%d/%d\n",k,i,w,s.state,*(int *)(ps+0xa8),s.time,*(int *)(ps+0x2c));return 1;
            }
        }
    }
    FreeLibrary(module);puts("256000 reload-check/finish and 110 idle selection comparisons passed.");return 0;
}
