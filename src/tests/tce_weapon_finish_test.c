#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern void CG_FinishWeaponChange(int,int);
extern int weapAlts[];
static char events[4096];
static void cvar(const char *name,const char *value){strcat(events,name);strcat(events,"=");strcat(events,value);strcat(events,";");}
static void command(const char *value){cvar("cmd",value);}
static void sound(vec3_t origin,int entity,int channel,int handle){char s[128];if(origin)abort();sprintf(s,"%d,%d,%d",entity,channel,handle);cvar("sound",s);}
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){
    HMODULE dll;unsigned char *b,raw[0x800];static snapshot_t snap;char expected[4096];
    int old,w,s,parsed,i,count=0;const float fov[]={0,20,60,75,90,120};
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
    /* Compare complete callback chain. Only engine services are intercepted. */
    patch(b+0x64c10,cvar);patch(trap_Cvar_Set,cvar);
    patch(b+0x64d60,command);patch(trap_SendConsoleCommand,command);
    patch(b+0x64fb0,sound);patch(trap_S_StartSound,sound);
    memset(raw,0,sizeof(raw));memset(&snap,0,sizeof(snap));cg.snap=&snap;*(void **)(b+0x40352c4)=raw;
    for(parsed=0;parsed<2;++parsed)for(old=0;old<64;++old)for(w=0;w<64;++w)for(s=0;s<16;++s){
        gearDef.parsed=parsed;
        memset(weapAlts,0,256);memset(b+0x106e78,0,256);
        if(s&1)weapAlts[old]=((int *)(b+0x106e78))[old]=w;
        cg.time=*(int *)(b+0x407dea4)=20000;
        cg.binocZoomTime=*(int *)(b+0x408a39c)=s==15;
        cg.weaponSelectTime=*(int *)(b+0x4080e44)=12;
        cg.mortarImpactTime=*(int *)(b+0x40a2454)=75;
        cg.newCrosshairIndex=*(int *)(b+0x4081534)=(s%4)-1;
        cg.weaponSelect=*(int *)(b+0x407ef70)=old;
        cg.lastFiredWeapon=*(int *)(b+0x4080f0c)=s&2?old:0;
        cg.switchbackWeapon=*(int *)(b+0x4080f08)=s&4?w:2;
        snap.ps.eFlags=*(int *)(raw+0x94)=s&8?EF_ZOOMING:0;
        snap.ps.clientNum=*(int *)(raw+0xcc)=17;
        cg.predictedPlayerState.eFlags=*(int *)(b+0x407df28)=s&4?EF_ZOOMING:0;
        cg.predictedPlayerState.ammoclip[old]=((int *)(b+0x407e150))[old]=s&2?7:0;
        cgs.media.selectSound=*(int *)(b+0x25886f8)=321;
        cg_weapons[w].switchSound=*(int *)(b+0x484b04c+w*0xfd0)=1000+w;
        cg.zoomTime=*(int *)(b+0x407f1c8)=19;cg.zoomedScope=*(int *)(b+0x407f1c4)=37;
        cg.zoomval=*(float *)(b+0x407f1d0)=43;
        cg_zoomDefaultSniper.value=*(float *)(b+0x488cf08)=fov[s%6];
        cg.pmext.silencedSideArm=3;
        memset(cg.lastWeapSelInBank,0xa5,40);memset(b+0x4080f28,0xa5,40);
        events[0]=0;((void (__cdecl *)(int,int))(b+0x74580))(old,w);strcpy(expected,events);
        events[0]=0;CG_FinishWeaponChange(old,w);
        if(strcmp(events,expected) || cg.weaponSelect!=*(int *)(b+0x407ef70) ||
           cg.switchbackWeapon!=*(int *)(b+0x4080f08) || cg.weaponSelectTime!=*(int *)(b+0x4080e44) ||
           cg.mortarImpactTime!=*(int *)(b+0x40a2454) || cg.newCrosshairIndex!=*(int *)(b+0x4081534) ||
           cg.zoomTime!=*(int *)(b+0x407f1c8) || cg.zoomedScope!=*(int *)(b+0x407f1c4) ||
           cg.zoomval!=*(float *)(b+0x407f1d0)){
            printf("finish mismatch gear%d old%d new%d scenario%d\n%s\n%s\n",parsed,old,w,s,expected,events);return 1;}
        for(i=0;i<10;++i)if(cg.lastWeapSelInBank[i]!=((int *)(b+0x4080f28))[i])return 3;
        ++count;
    }
    printf("%d whole finish/zoom/sound/index chain comparisons passed before/after Gear\n",count);return 0;
}
