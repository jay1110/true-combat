#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_flash.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
typedef struct{size_t offset;unsigned original;} mapping_t;
#define M(f,a) {offsetof(cg_t,f),a}
static mapping_t map[]={M(tceSoundSampleTime,0x40a4984),M(tceSoundEnvironment,0x40a49bc),M(tceRoomDistance,0x40a49a0),M(tceRoomSky,0x40a49a4),M(tceRoomHardness,0x40a49a8),M(tceSoundZoneActive,0x40a4b98),M(tceSoundZoneTransitionTime,0x40a4b9c),M(tceSoundLastOrigin[0],0x40a4988),M(tceSoundLastOrigin[1],0x40a498c),M(tceSoundLastOrigin[2],0x40a4990),M(tceRoomExtent[0],0x40a4994),M(tceRoomExtent[1],0x40a4998),M(tceRoomExtent[2],0x40a499c),M(tceRoomWeights[0],0x40a49ac),M(tceRoomWeights[1],0x40a49b0),M(tceRoomWeights[2],0x40a49b4),M(tceRoomWeights[3],0x40a49b8),M(tceSoundZoneVolume[0],0x40a4b90),M(tceSoundZoneVolume[1],0x40a4b94),M(tceSoundZoneSounds[0][0],0x40a4b70),M(tceSoundZoneSounds[0][1],0x40a4b74),M(tceSoundZoneSounds[1][0],0x40a4b78),M(tceSoundZoneSounds[1][1],0x40a4b7c),M(tceSoundZoneEffects[0][0],0x40a4b80),M(tceSoundZoneEffects[0][1],0x40a4b84),M(tceSoundZoneEffects[1][0],0x40a4b88),M(tceSoundZoneEffects[1][1],0x40a4b8c),M(tceTacticalScale,0x4846e6c),M(tceAdsBreathTime,0x40a4968),M(tceCoronaBlendAlpha,0x40a49e4),M(serverRespawning,0x40a2448),M(thisFrameTeleport,0x407de98),M(predictedPlayerState.weaponstate,0x407df68),M(predictedPlayerState.weapAnim,0x407e340),M(cursorHintIcon,0x4080dcc),M(cursorHintTime,0x4080dd0),M(weaponSelectTime,0x4080e44),M(cameraMode,0x408110c),M(weaponSelect,0x407ef70),M(zoomedBinoc,0x407f1c0),M(zoomedScope,0x407f1c4),M(zoomTime,0x407f1c8),M(zoomval,0x407f1d0),M(binocZoomTime,0x408a39c),M(proneMovingTime,0x408a3a4),M(tceAimActive,0x40a496c),M(tceAimComplete,0x40a4970),M(tceAimRequested,0x40a4974),M(tceAimSyncTime,0x40a4978),M(tceAimRetryTime,0x40a497c),M(tceAimWeaponLatch,0x40a49fc),M(tceShotHoldUntil,0x40a495c),M(tceActionTransitionTime,0x40a4954),M(tceStanceTime,0x40a4950),M(tceWeaponDuckTime,0x40a4948),M(tceProneTime,0x40a4958),M(tceHeartbeatNext,0x40a4960),M(tceHeartbeatUntil,0x40a4964),M(tceScopeBlocked,0x40a4b0c),M(tceEyeSampleTime,0x40a4bd0),M(tceEyeTime,0x40a4bcc),M(tceEyeFlare,0x40a4bdc),M(tceEyeSmooth,0x40a4be8),M(tceEyeLinear,0x40a4bec),M(tceScopeLightBoost,0x40a4c08),M(tceEyeFlareFrame,0x40a4c1c),M(tceFiremodeTime,0x40a49f4),M(tceFiremodeAnimationTime,0x40a49f8),M(tceSwayTime,0x40a4a04)};
static char events[1024];
static int idle(int weapon){char s[50];sprintf(s,"idle%d\n",weapon);strcat(events,s);return 17+(weapon%3);}
static void command(const char*s){strcat(events,s);}
static void cvar(const char*k,const char*v){strcat(events,k);strcat(events,"=");strcat(events,v);}
static void fog(int a,int b,int c,float d,float e,float f,float g){char s[200];sprintf(s,"fog%d/%d/%d/%g/%g/%g/%g",a,b,c,d,e,f,g);strcat(events,s);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b,raw[0x600];snapshot_t snap;void(__cdecl*original)(int);int c,i,accepted;char expected[1024];
 accepted=argc==3&&!strcmp(argv[2],"--accepted-extensions");
 if((argc!=2&&!accepted)||!(dll=LoadLibraryA(argv[1])))return 2;
 if(!accepted&&sizeof(cg.pmext)!=112){printf("STRICT_BASELINE_DIFFERENCE CG_Respawn pmext=%u original=112; freelook tail requires separate accepted-extension test\n",(unsigned)sizeof(cg.pmext));return 1;}
 if(accepted&&(sizeof(cg.pmext)!=120||offsetof(pmoveExt_t,tceFreelookRecoil)!=112||sizeof(cg.pmext.tceFreelookRecoil)!=8)){puts("Accepted extension layout contract changed");return 2;}
b=(byte*)dll;original=(void*)(b+0x56cb0);
 hook(b+0x64d60,command);hook(trap_SendConsoleCommand,command);hook(b+0x64c10,cvar);hook(trap_Cvar_Set,cvar);hook(b+0x65380,fog);hook(trap_R_SetFog,fog);
 /* A real connected TC game has parsed gear. Without this fixture state the
  * SDK compatibility branch classifies Glock39 as a rifle-grenade idle. */
 gearDef.parsed=1;
 for(c=0;c<1200;c++){memset(raw,0,sizeof(raw));memset(&snap,0,sizeof(snap));cg.snap=&snap;*(void**)(b+0x40352c4)=raw;cg.time=*(int*)(b+0x407dea4)=c*37-2000;snap.ps.weapon=*(int*)(raw+0xd0)=c%64;cg_autoReload.integer=*(int*)(b+0x40143cc)=c%5-2;cgs.limboLoadoutSelected=*(int*)(b+0x3fdecb4)=77;
  for(i=0;i<sizeof(map)/sizeof(map[0]);i++){int seed=0x3f000000+c+i;memcpy((byte*)&cg+map[i].offset,&seed,4);memcpy(b+map[i].original,&seed,4);}memset(&cg.pmext,0xa7,sizeof(cg.pmext));memset(b+0x4087ee8,0xa7,112);tceFlash.blindUntil=*(int*)(b+0x40a49c0)=71;tceFlash.blindActive=*(int*)(b+0x40a49c4)=72;tceFlash.deafUntil=*(int*)(b+0x40a49c8)=73;tceFlash.deafness=*(float*)(b+0x40a49cc)=.74f;events[0]=0;original(c%2);strcpy(expected,events);events[0]=0;CG_Respawn(c%2);
  for(i=0;i<sizeof(map)/sizeof(map[0]);i++)if(memcmp((byte*)&cg+map[i].offset,b+map[i].original,4)){printf("respawn case%d offset%x\n",c,map[i].original);return 1;}
  if(memcmp(&cg.pmext,b+0x4087ee8,112) || memcmp(&tceFlash,b+0x40a49c0,16) || strcmp(events,expected)||cgs.limboLoadoutSelected!=*(int*)(b+0x3fdecb4)||cg.pmext.bAutoReload!=*(int*)(b+0x4087ee8)||cg.pmext.sprintTime!=*(int*)(b+0x4087ef8)){printf("respawn calls/pmext case%d\n",c);return 1;}
  if(accepted){const byte zero[8]={0};if(memcmp(cg.pmext.tceFreelookRecoil,zero,8)){printf("Freelook suffix not reset case%d\n",c);return 1;}}
 }printf(accepted?"ACCEPTED_EXTENSION CG_Respawn 1200 cases: original112-byte prefix+state/calls equal; separate8-byte freelook suffix zeroed\n":"1200 whole CG_Respawn state and engine-call comparisons passed\n");return 0;
}

