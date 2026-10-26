#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_weapons.c"
#include <windows.h>
#include <stdio.h>
#include "../cgame/tce_weapon_position.h"
typedef struct {size_t offset;unsigned address;size_t size;} mapping_t;
#define M(field,address) {offsetof(tce_weaponPosition_t,field),address,sizeof(((tce_weaponPosition_t *)0)->field)}
static const mapping_t mappings[]={
    M(time,0x3407dea4),M(weapon,0x3407df64),M(eFlags,0x3407df28),M(thirdPerson,0x3407deb8),M(weaponState,0x3407df68),
    M(mountedPitch,0x34087f28),M(viewAngles,0x3407f1b0),M(postureModified,0x340a4b0c),M(proneMovingTime,0x3408a3a4),
    M(flags_3407dfb0,0x3407dfb0),M(stanceTime,0x340a4950),M(pmFlags,0x3407decc),M(duckTime,0x340a4948),
    M(lean,0x3407defc),M(aiming,0x340a496c),M(leanTime,0x3407f1c8),M(shotTime,0x3407e6b4),M(count_3407dff8,0x3407dff8),
    M(scopeEnabled,0x34849e4c),M(firemodeTime,0x340a49f8),M(speed,0x34080fb4),M(bobSin,0x34080fa4),M(bobCycle,0x34080fa8),
    M(swayTime,0x340a4a04),M(priorForward,0x340a4a08),M(swayHorizontal,0x340a4a14),M(swayVertical,0x340a4a18),
    M(velocity,0x3407dee0),M(proneTime,0x340a4958),M(stepTime,0x3407ef64),M(scale_3407dfbc,0x3407dfbc),M(scale_3407dfc0,0x3407dfc0),
    M(landTime,0x3407ef6c),M(stepChange,0x3407ef60),M(landChange,0x3407ef68),M(developer,0x32576e2c),
    M(tacticalScale,0x34846e6c),M(tacticalPitch,0x3407e264),M(tacticalYaw,0x3407e268),M(kickAngles,0x340810f8)
};

static int positionCases(unsigned char *base) {
 static const int deltas[]={0,1,100,149,150,199,200,201,300,449,450};
 tce_weaponPosition_t state; refdef_t rd; float raw[18],eo[3],ea[3],o[3],a[3];
 int w,p,d,l,k,n=0; size_t m;
 void (__cdecl *fn)(float *,float *)=(void *)(base+0x73270);
 memset(&cg,0,sizeof(cg));memset(&rd,0,sizeof(rd));gearDef.parsed=1;
 cg.refdef_current=&rd;cg.time=10000;cg.xyspeed=75;cg.bobfracsin=.35f;
 VectorSet(rd.vieworg,100,200,300);VectorSet(cg.refdefViewAngles,12,33,5);
 AnglesToAxis(cg.refdefViewAngles,rd.viewaxis);VectorSet(cg.kickAngles,1,2,3);
 for(w=1;w<64;++w)for(p=0;p<3;++p)for(d=0;d<11;++d)for(l=0;l<3;++l) {
  memset(&state,0,sizeof(state));memset(&weaponDef[w],0,sizeof(weaponDef[w]));
  cg.predictedPlayerState.weapon=w;cg.predictedPlayerState.weaponstate=0;
  cg.predictedPlayerState.eFlags=p==1?EF_PRONE_MOVING:0;
  cg.proneMovingTime=p==1?cg.time-deltas[d]:(p==2?deltas[d]-cg.time:0);
  cg.predictedPlayerState.leanf=(float)(l*7-7);cg.landTime=cg.time-deltas[d];cg.landChange=-8;
  cg.pmext.mountedWeaponAngles[PITCH]=24;
  VectorSet(tce_cg_weapons[w].gunViewAngles,-5,.2f,1);
  state.time=cg.time;state.weapon=w;state.eFlags=cg.predictedPlayerState.eFlags;
  state.proneMovingTime=cg.proneMovingTime;state.lean=cg.predictedPlayerState.leanf;
  state.landTime=cg.landTime;state.landChange=cg.landChange;state.mountedPitch=24;
  state.speed=cg.xyspeed;state.bobSin=cg.bobfracsin;
  VectorCopy(rd.vieworg,state.viewOrigin);VectorCopy(cg.refdefViewAngles,state.viewAngles);
  AxisCopy(rd.viewaxis,state.viewAxis);VectorCopy(rd.viewaxis[0],state.priorForward);
  VectorCopy(cg.kickAngles,state.kickAngles);VectorCopy(tce_cg_weapons[w].gunViewAngles,state.gunViewAngles);
  /* Reset and project persistent runtime state for each original comparison.
   * Previous fixture implicitly relied on the old adapter forcing neutral sway. */
  state.swayTime=cg.time-(n%4)*25;
  state.swayHorizontal=(n%5-2)*.25f;state.swayVertical=(n%7-3)*.125f;
  cg.tceSwayTime=state.swayTime;VectorCopy(state.priorForward,cg.tcePriorForward);
  cg.tceSwayHorizontal=state.swayHorizontal;cg.tceSwayVertical=state.swayVertical;
  cg.tceScopeBlocked=0;cg.tceStanceTime=0;cg.tceWeaponDuckTime=0;
  cg.tceAimActive=0;cg.zoomTime=0;cg.predictedPlayerEntity.muzzleFlashTime=0;
  cg.tceFiremodeTime=0;cg.tceProneTime=0;cg.tceTacticalScale=0;
  cg_portalScopes.integer=0;developer.integer=0;
  for(m=0;m<sizeof(mappings)/sizeof(mappings[0]);++m)
   memcpy(base+mappings[m].address-0x30000000,(char *)&state+mappings[m].offset,mappings[m].size);
  memset(raw,0,sizeof(raw));memcpy(raw+6,state.viewOrigin,12);memcpy(raw+9,state.viewAxis,36);
  *(void **)(base+0x40863ac)=raw;
  memcpy(base+0x484af50+w*0xfd0,state.gunViewAngles,12);
  memcpy(base+0x4c1df20+w*0x1cc,&weaponDef[w],sizeof(weaponDef[w]));
  fn(eo,ea);CG_CalculateWeaponPosition(o,a);
  for(k=0;k<3;++k)if(fabs(o[k]-eo[k])>.001f || fabs(a[k]-ea[k])>.001f) {
   printf("position mismatch w%d posture%d delta%d lean%d axis%d origin%f/%f angle%f/%f\n",w,p,deltas[d],l,k,o[k],eo[k],a[k],ea[k]);return 0;
  }
  if(cg.tceScopeBlocked!=*(int *)(base+0x40a4b0c) || cg.tceSwayTime!=*(int *)(base+0x40a4a04) ||
     fabs(cg.tceSwayHorizontal-*(float *)(base+0x40a4a14))>.001f ||
     fabs(cg.tceSwayVertical-*(float *)(base+0x40a4a18))>.001f) {
   printf("persistent weapon-position state mismatch %d\n",n);return 0;
  }
  for(k=0;k<3;++k)if(fabs(cg.tcePriorForward[k]-((float *)(base+0x40a4a08))[k])>.001f)return 0;
  ++n;
 }
 printf("%d original runtime hip-position comparisons passed (persistent sway outputs included)\n",n);
 memset(&cg,0,sizeof(cg));return 1;
}

static vec3_t poseAngles;
static refEntity_t captured;
static int calls;
static void position(vec3_t origin,vec3_t angles) {
 VectorSet(origin,100,200,300);VectorCopy(poseAngles,angles);
}
static void capture(refEntity_t *parent,playerState_t *ps,centity_t *cent) {
 captured=*parent;++calls;
}
static void patch(void *address,void *fn){
 unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv){
 HMODULE dll;unsigned char *base;unsigned char raw[0x800]={0};
 playerState_t ps;refEntity_t expected;int w,a,o,f,k,j,n=0;
 float shortening[]={0,.5f,1,1.3f,-.25f};
 void (__cdecl *original)(void *);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;
 original=(void *)(base+0x727f0);
 if(!positionCases(base))return 8;
 patch(base+0x73270,position);patch(CG_CalculateWeaponPosition,position);
 patch(base+0x6f4b0,capture);patch(CG_AddPlayerWeapon,capture);
 memset(&ps,0,sizeof(ps));gearDef.parsed=1;cg_drawGun.integer=1;cg_gun_frame.integer=1;
 *(int *)(base+0x4034d0c)=1;*(int *)(base+0x488b52c)=1;
 *(int *)(base+0x407deb8)=0;*(int *)(base+0x40a2444)=0;*(int *)(base+0x40810e8)=0;
 *(int *)(base+0x4889a28)=0;
 for(w=1;w<64;++w)for(a=0;a<5;++a)for(o=0;o<3;++o)for(f=0;f<5;++f){
  ps.weapon=*(int *)(raw+0xa4)=w;
  VectorSet(poseAngles,a*12-25,a*33-80,a*5-9);
  cg_gun_x.value=*(float *)(base+0x4010828)=(float)(o*2-1);
  cg_gun_y.value=*(float *)(base+0x4010948)=(float)(o*3-2);
  cg_gun_z.value=*(float *)(base+0x40116c8)=(float)(o*4-3);
  cg_fov.integer=90+a*10;
  tce_cg_weapons[w].foreShorten=*(float *)(base+0x484af5c+w*0xfd0)=shortening[f];
  cg_weapons[w].handsModel=*(int *)(base+0x484a634+w*0xfd0)=123;
  calls=0;original(raw);if(calls!=1)return 3;expected=captured;
  calls=0;CG_AddViewWeapon(&ps);if(calls!=1)return 4;
  for(k=0;k<3;++k){
   if(fabs(captured.origin[k]-expected.origin[k])>.0001f){
    printf("origin mismatch w%d angle%d off%d short%d axis%d %f/%f\n",w,a,o,f,k,captured.origin[k],expected.origin[k]);return 5;
   }
   for(j=0;j<3;++j)if(fabs(captured.axis[k][j]-expected.axis[k][j])>.0001f)return 6;
  }
  if(captured.nonNormalizedAxes!=expected.nonNormalizedAxes || captured.hModel!=expected.hModel ||
     captured.frame!=expected.frame || captured.oldframe!=expected.oldframe || captured.renderfx!=expected.renderfx)return 7;
  ++n;
 }
 printf("%d original ordinary AddViewWeapon transforms matched (position and child renderer intercepted)\n",n);
 return 0;
}
