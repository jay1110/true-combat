#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
extern void TCE_UI_PlayerInfo_SetWeapon(playerInfo_t *,int);
static int failMode, calls;
static char events[8192];
static int model(const char *path) {
    unsigned hash=1;const char *p=path;char line[512];int result;
    if(path)while(*p)hash=hash*33+(unsigned char)*p++;
    result=path && !(failMode==1 || (failMode==2 && calls==0) ||
        (failMode==3 && strstr(path,"_flash"))) ? (hash&0x7fffffff)|1 : 0;
    sprintf(line,"%s=%d;",path?path:"<NULL>",result);strcat(events,line);++calls;return result;
}
static void patch(void *address,void *fn) {
    unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);
    VirtualProtect(p,5,old,&tmp);
}

#define FIELD(name,off) {offsetof(playerInfo_t,name),off,sizeof(((playerInfo_t *)0)->name)}
static const struct {int src,raw,size;} fields[]={
 FIELD(legs.yawAngle,0x1c),FIELD(legs.yawing,0x20),FIELD(torso.yawAngle,0x54),FIELD(torso.yawing,0x58),
 FIELD(weaponModel,0x57d0),FIELD(barrelModel,0x57d4),FIELD(flashModel,0x57d8),FIELD(flashDlightColor,0x57dc),
 FIELD(muzzleFlashTime,0x57e8),FIELD(viewAngles,0x57ec),FIELD(moveAngles,0x57f8),FIELD(currentWeapon,0x5804),
 FIELD(legsAnim,0x5808),FIELD(torsoAnim,0x580c),FIELD(weapon,0x5810),FIELD(lastWeapon,0x5814),FIELD(pendingWeapon,0x5818),FIELD(weaponTimer,0x581c),FIELD(pendingLegsAnim,0x5820),FIELD(torsoAnimationTimer,0x5824),FIELD(pendingTorsoAnim,0x5828),FIELD(legsAnimationTimer,0x582c),FIELD(chat,0x5830),FIELD(newModel,0x5834),FIELD(realWeapon,0x5844)};
int main(int argc,char **argv){
 HMODULE dll;unsigned char *b,raw[0x6000];playerInfo_t pi;char expected[8192];int w,l,t,state,n,f,count=0;
 int weapons[]={-1,0,3,39,65,66};int anims[]={0,47,51,52,53,54,60,61,112,114,120};
 vec3_t view={10,133,-21},move={-40,37,99};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
 patch(b+0x206d0,model);patch(trap_R_RegisterModel,model);
 for(w=0;w<6;++w)for(l=0;l<11;++l)for(t=0;t<11;++t)for(state=0;state<8;++state)for(n=0;n<2;++n){
  memset(raw,0x35,sizeof(raw));memset(&pi,0x35,sizeof(pi));
  pi.newModel=n;pi.lastWeapon=(state&1)?39:0;pi.currentWeapon=(state&2)?39:0;
  pi.legsAnim=anims[(l+state)%11]|((state&4)?512:0);pi.torsoAnim=anims[(t+state)%11]|((state&4)?512:0);
  for(f=0;f<sizeof(fields)/sizeof(fields[0]);++f)memcpy(raw+fields[f].raw,(char *)&pi+fields[f].src,fields[f].size);
  *(int *)(b+0x63f54)=0;failMode=state%4;events[0]=0;calls=0;
  ((void(__cdecl *)(void *,int,int,float *,float *,int,int))(b+0x14440))(raw,anims[l],anims[t],view,move,weapons[w],state&1);strcpy(expected,events);
  events[0]=0;calls=0;UI_PlayerInfo_SetInfo(&pi,anims[l],anims[t],view,move,weapons[w],state&1);
  if(strcmp(expected,events)){printf("preview model trace mismatch %d\n",count);return 3;}
  for(f=0;f<sizeof(fields)/sizeof(fields[0]);++f)if(memcmp(raw+fields[f].raw,(char *)&pi+fields[f].src,fields[f].size)){printf("SetInfo mismatch case%d field%x w%d l%d t%d state%d new%d\n",count,fields[f].raw,weapons[w],anims[l],anims[t],state,n);return 4;}
  ++count;
 }
 printf("%d complete UI preview state/weapon/animation comparisons passed\n",count);return 0;
}
