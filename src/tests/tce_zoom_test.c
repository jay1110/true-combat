#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_view.c"
#include <windows.h>
#include <stdio.h>
static snapshot_t snap;static unsigned char rawSnap[0x1000];static int water;static char events[512];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static int contents(const vec3_t p,int n){return water;}
static void print(const char *s,...){strcat(events,s);}
#define I(field,rva,value) cg.field=*(int*)(b+rva)=(value)
#define S(field,off,value) snap.ps.field=*(int*)(rawSnap+off)=(value)
#define CV(field,rva,value) field.integer=*(int*)(b+rva)=(value)
int main(int argc,char **argv){HMODULE dll;unsigned char*b;int i,j,mode,count=0,expectedReturn,actualReturn;refdef_t original;char expected[512];
 if(argc==2 && !strcmp(argv[1],"--offsets")) {
  printf("{\"snap\":%u,\"refdef\":%u,\"aim\":%u,\"requested\":%u,\"complete\":%u,\"weapon\":%u,\"flags\":%u,\"zoom\":%u,\"snapflags\":%u,\"snapweapon\":%u,\"snapredflag\":%u,\"snapblueflag\":%u,\"snapobjectiveflags\":%u,\"snaphostageflags\":%u,\"limbooverlay\":%u,\"showgameview\":%u,\"time\":%u,\"levelshotfade\":%u}\n",
   (unsigned)offsetof(cg_t,snap),(unsigned)offsetof(cg_t,refdef_current),(unsigned)offsetof(cg_t,tceAimActive),(unsigned)offsetof(cg_t,tceAimRequested),(unsigned)offsetof(cg_t,tceAimComplete),
   (unsigned)offsetof(cg_t,predictedPlayerState.weapon),(unsigned)offsetof(cg_t,predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS]),(unsigned)offsetof(cg_t,zoomval),
   (unsigned)offsetof(snapshot_t,ps.stats[STAT_TCE_WEAPON_FLAGS]),(unsigned)offsetof(snapshot_t,ps.weapon),(unsigned)offsetof(snapshot_t,ps.powerups[PW_REDFLAG]),(unsigned)offsetof(snapshot_t,ps.powerups[PW_BLUEFLAG]),(unsigned)offsetof(snapshot_t,ps.stats[8]),(unsigned)offsetof(snapshot_t,ps.stats[9]),(unsigned)offsetof(cg_t,tceShowLimboPanel),(unsigned)offsetof(cg_t,showGameView),(unsigned)offsetof(cg_t,time),(unsigned)offsetof(cg_t,levelshotFadeStart));return 0;
 }
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)dll;cg.snap=&snap;*(void**)(b+0x40352c4)=rawSnap;cg.refdef_current=&cg.refdef;*(void**)(b+0x40863ac)=&original;
 patch(b+0x587d0,contents);patch(CG_PointContents,contents);patch(b+0x47b10,print);patch(CG_Printf,print);
 for(i=0;i<20000;++i){
  I(time,0x407dea4,1000+i%1900);I(zoomTime,0x407f1c8,1100);I(tceAimRequested,0x40a4974,i%3);I(tceAimActive,0x40a496c,(i/3)%2);I(tceAimComplete,0x40a4970,2);I(tceAimSyncTime,0x40a4978,900);I(tceAimRetryTime,0x40a497c,(i%5)?0:1200);I(tceAimWeaponLatch,0x40a49fc,77);
  ((void(__cdecl*)(void))(b+0x67e40))();CG_ToggleAiming();
  if(cg.zoomTime!=*(int*)(b+0x407f1c8)||cg.tceAimRequested!=*(int*)(b+0x40a4974)||cg.tceAimActive!=*(int*)(b+0x40a496c)||cg.tceAimSyncTime!=*(int*)(b+0x40a4978)||cg.tceAimComplete||cg.tceAimRetryTime||cg.tceAimWeaponLatch)return 1;++count;
 }
 for(i=0;i<50000;++i){
  int weapon=i%64, scoped=(i/7)%6;
  memset(&original,0,sizeof(original));memset(&cg.refdef,0,sizeof(cg.refdef));original.width=cg.refdef.width=1280;original.height=cg.refdef.height=720;
  I(time,0x407dea4,1000+i%1000);I(zoomTime,0x407f1c8,950+(i%13)*10);I(tceAimRequested,0x40a4974,(i/2)%2);I(tceAimActive,0x40a496c,i%2);I(tceAimComplete,0x40a4970,0);I(tceAimSyncTime,0x40a4978,1100);I(tceAimRetryTime,0x40a497c,(i%5)?0:1200);
  I(demoPlayback,0x40352b0,(i/3)%2);I(predictedPlayerState.weapon,0x407df64,weapon);I(predictedPlayerState.eFlags,0x407df28,0);I(predictedPlayerState.stats[STAT_HEALTH],0x407df90,i%17?100:0);I(predictedPlayerState.pm_type,0x407dec4,i%6);I(renderingThirdPerson,0x407deb8,(i/5)%2);I(zoomedBinoc,0x407f1c0,(i/11)%2);I(showGameView,0x4081fa8,(i/19)%2);
  S(pm_flags,0x38,(i%7==0?PMF_FOLLOW:0)|(i%31==0?PMF_TIME_LOCKPLAYER:0));S(weapon,0xd0,weapon);S(eFlags,0x94,i%41==0?EF_MOUNTEDTANK:0);S(stats[STAT_TCE_WEAPON_FLAGS],0x11c,i%3?4:0);S(pm_type,0x30,(i/17)%6);S(persistant[PERS_HWEAPON_USE],0x16c,i%43==0);
  CV(cg_portalScopes,0x4849e4c,(i/7)%2);CV(developer,0x2576e2c,(i/11)%2);CV(cg_aspectMode,0x401496c,(i/23)%3);CV(cg_aspectFovMode,0x25772ac,(i/31)%3);
  cg_fov.value=*(float*)(b+0x488a208)=80+(i%7)*10;cg.zoomval=*(float*)(b+0x407f1d0)=i%3?0:40;
  weaponDef[weapon].scoped=*(float*)(b+0x4c1e094+weapon*0x1cc)=(float)scoped;weaponDef[weapon].unknown_1ac=*(int*)(b+0x4c1e0cc+weapon*0x1cc)=i%5?65:-1;
  water=i%3==0?CONTENTS_WATER:0;events[0]=0;expectedReturn=((int(__cdecl*)(void))(b+0x6aaf0))();strcpy(expected,events);events[0]=0;actualReturn=CG_CalcFov();
  if(expectedReturn!=actualReturn||strcmp(expected,events)||fabs(original.fov_x-cg.refdef.fov_x)>0.0001||fabs(original.fov_y-cg.refdef.fov_y)>0.0001||fabs(cg.zoomSensitivity-*(float*)(b+0x407f1cc))>0.00001||cg.zoomval!=*(float*)(b+0x407f1d0)||cg.tceAimActive!=*(int*)(b+0x40a496c)||cg.tceAimComplete!=*(int*)(b+0x40a4970)||cg.tceAimRequested!=*(int*)(b+0x40a4974)||cg.tceAimSyncTime!=*(int*)(b+0x40a4978)||cg.tceAimRetryTime!=*(int*)(b+0x40a497c)||cg.zoomTime!=*(int*)(b+0x407f1c8)||original.rdflags!=cg.refdef.rdflags||cg.predictedPlayerState.weapon!=*(int*)(b+0x407df64)||cg.predictedPlayerState.eFlags!=*(int*)(b+0x407df28)||cg.zoomedBinoc!=*(int*)(b+0x407f1c0)){printf("FOV mismatch %d orig%.9g %.9g ours%.9g %.9g sens%.9g %.9g zoom%.9g %.9g ret%d %d\n%s / %s\n",i,original.fov_x,original.fov_y,cg.refdef.fov_x,cg.refdef.fov_y,*(float*)(b+0x407f1cc),cg.zoomSensitivity,*(float*)(b+0x407f1d0),cg.zoomval,expectedReturn,actualReturn,expected,events);return 1;}++count;
 }
 printf("%d complete aim-toggle/FOV frame comparisons passed\n",count);return 0;
}
