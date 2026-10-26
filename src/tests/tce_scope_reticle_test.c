#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
typedef struct {int kind,shader;float x,y,w,h,color[4];} drawCall;
static drawCall calls[80], expected[80];static int count;
static snapshot_t snapshot;static unsigned char rawSnapshot[0x1000];
static void patch(void *at,void *fn){DWORD old,t;unsigned char*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
static void fill(float x,float y,float w,float h,const float*c){drawCall*d=&calls[count++];memset(d,0,sizeof(*d));d->kind=1;d->x=x;d->y=y;d->w=w;d->h=h;memcpy(d->color,c,16);}
static void pic(float x,float y,float w,float h,int shader){drawCall*d=&calls[count++];memset(d,0,sizeof(*d));d->kind=2;d->x=x;d->y=y;d->w=w;d->h=h;d->shader=shader;}
static void color(const float*c){drawCall*d=&calls[count++];memset(d,0,sizeof(*d));d->kind=c?3:4;if(c)memcpy(d->color,c,16);}
static void light(const float*p,float*c){c[0]=0.2f;c[1]=0.5f;c[2]=0.8f;}
static void gridLight(const tce_lightGrid_t*g,const float*p,float*c){light(p,c);}
static void marker(int kind){drawCall*d=&calls[count++];memset(d,0,sizeof(*d));d->kind=kind;}
static void reticle(void){marker(10);}
static void binocular(void){marker(11);}
static void mortar(void){marker(12);}
static void change(int from,int to){marker(13);calls[count-1].x=(float)from;calls[count-1].y=(float)to;}
static void adjust(float*x,float*y,float*w,float*h){*x*=1.5f;*y*=1.5f;*w*=1.5f;*h*=1.5f;}
static void health(float*c){c[0]=.1f;c[1]=.2f;c[2]=.3f;c[3]=1;}
static void stretch(float x,float y,float w,float h,float s0,float t0,float s1,float t1,int shader){pic(x,y,w,h,shader);}
#define SETI(field,rva,value) cg.field=*(int*)(b+(rva))=(value)
int main(int argc,char**argv){HMODULE dll;unsigned char*b;int i,j,k,n,weapon;float *f,*e;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)dll;
 cg.snap=&snapshot;*(void**)(b+0x40352c4)=rawSnapshot;
 patch(b+0x29850,fill);patch(CG_FillRect,fill);patch(b+0x29ea0,pic);patch(CG_DrawPic,pic);
 patch(b+0x65420,color);patch(trap_R_SetColor,color);patch(b+0x7e850,AngleVectors);
 patch(b+0x68160,light);
 patch(TCE_CG_LightForParticleSimple,gridLight);
 for(i=0;i<50000;i++){
  weapon=i%64;SETI(predictedPlayerState.weapon,0x407df64,weapon);SETI(demoPlayback,0x40352b0,i%3==0);
  snapshot.ps.weapon=*(int*)(rawSnapshot+0xd0)=weapon;snapshot.ps.pm_flags=*(int*)(rawSnapshot+0x38)=i%5==0?PMF_FOLLOW:0;
  cg_portalScopes.integer=*(int*)(b+0x4849e4c)=i%2;
  weaponDef[weapon].scopeReticleType=*(int*)(b+0x4c1e098+weapon*0x1cc)=(i/2)%4;
  tce_cg_weapons[0].portalScopeWidth=*(int*)(b+0x484af64)=(i%7-1)*50;
  cgs.media.reticleShaderSimple=*(int*)(b+0x2588150)=i%3?41:0;
  cgs.media.tceM76ReticleShader=*(int*)(b+0x2588160)=i%5?42:0;
  cgs.media.tcePortalScopeShader=*(int*)(b+0x2588164)=i%7?43:0;
  cg.tceScopeLightBoost=*(float*)(b+0x40a4c08)=(i%9)*0.125f;
  cg.refdef.fov_x=*(float*)(b+0x407f014)=50+(i%7)*10;
  for(j=0;j<3;j++)cg.predictedPlayerState.viewangles[j]=*(float*)(b+0x407df70+j*4)=(i*(j+1)%360)-180;
  SETI(predictedPlayerState.holdable[5],0x407e264,1900+i%201);SETI(predictedPlayerState.holdable[6],0x407e268,1900+(i/3)%201);
  count=0;((void(__cdecl*)(void))(b+0x23e00))();n=count;memcpy(expected,calls,sizeof(calls));count=0;CG_DrawWeapReticle();
  if(n!=count){printf("reticle case%d calls%d/%d\n",i,n,count);return 1;}
  for(j=0;j<count;j++){
   if(calls[j].kind!=expected[j].kind||calls[j].shader!=expected[j].shader){printf("reticle case%d call%d type\n",i,j);return 1;}
   f=&calls[j].x;e=&expected[j].x;
   for(k=0;k<8;k++)if(fabs(f[k]-e[k])>0.0001+fabs(e[k])*0.000001){printf("reticle case%d call%d field%d %.9g/%.9g\n",i,j,k,f[k],e[k]);return 1;}
  }
 }
 puts("50000 whole CG_DrawWeapReticle original callback comparisons passed");
 patch(b+0x23e00,reticle);patch(CG_DrawWeapReticle,reticle);
 patch(b+0x25410,binocular);patch(CG_DrawBinocReticle,binocular);
 patch(b+0x247c0,mortar);patch(CG_DrawMortarReticle,mortar);
 patch(b+0x74580,change);patch(CG_FinishWeaponChange,change);
 patch(b+0x29760,adjust);patch(CG_AdjustFrom640,adjust);
 patch(b+0x2a7a0,health);patch(CG_ColorForHealth,health);
 patch(b+0x65440,stretch);patch(trap_R_DrawStretchPic,stretch);
 for(i=0;i<60000;i++){
  weapon=i%64;SETI(predictedPlayerState.weapon,0x407df64,weapon);SETI(demoPlayback,0x40352b0,i%3==0);
  snapshot.ps.weapon=*(int*)(rawSnapshot+0xd0)=weapon;snapshot.ps.pm_flags=*(int*)(rawSnapshot+0x38)=i%5==0?PMF_FOLLOW:0;
  SETI(renderingThirdPerson,0x407deb8,i%19==0);SETI(zoomedBinoc,0x407f1c0,i%31==0);
  SETI(tceAimActive,0x40a496c,i%3!=0);SETI(tceAimComplete,0x40a4970,i%5!=0);SETI(tceScopeBlocked,0x40a4b0c,i%7==0);
  SETI(predictedPlayerState.eFlags,0x407df28,i%23==0?EF_PRONE_MOVING:0);
  SETI(predictedPlayerState.weaponstate,0x407df68,i%4);
  SETI(predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS],0x407dfb0,i%3?0:4);
  SETI(predictedPlayerState.stats[11],0x407dfbc,i%255);SETI(predictedPlayerState.stats[12],0x407dfc0,i%251);
  snapshot.ps.eFlags=*(int*)(rawSnapshot+0x94)=i%29==0?EF_MOUNTEDTANK:0;
  snapshot.ps.persistant[PERS_TEAM]=*(int*)(rawSnapshot+0x148)=i%4;
  snapshot.ps.stats[STAT_HEALTH]=*(int*)(rawSnapshot+0xfc)=i%17?100:0;
  snapshot.ps.leanf=*(float*)(rawSnapshot+0x68)=i%13?0:1;
  snapshot.ps.serverCursorHint=*(int*)(rawSnapshot+0x454)=i%35;
  snapshot.ps.velocity[0]=*(float*)(rawSnapshot+0x4c)=(float)(i%200);
  snapshot.ps.velocity[1]=snapshot.ps.velocity[2]=*(float*)(rawSnapshot+0x50)=*(float*)(rawSnapshot+0x54)=0;
  SETI(mvTotalClients,0x4086398,i%2);
  weaponDef[weapon].scoped=*(float*)(b+0x4c1e094+weapon*0x1cc)=(float)((i/7)%5);
  weaponDef[weapon].unknown_0f8[0]=*(int*)(b+0x4c1e018+weapon*0x1cc)=i%333;
  weaponDef[weapon].unknown_0f8[3]=*(int*)(b+0x4c1e024+weapon*0x1cc)=i%777;
  developer.integer=*(int*)(b+0x2576e2c)=i%2;
  cg_drawCrosshair.integer=*(int*)(b+0x40104cc)=i%11-1;
  cg_crosshairHealth.integer=*(int*)(b+0x40105ec)=i%3==0;
  cg_crosshairSize.value=*(float*)(b+0x2577188)=24.0f;
  cg_crosshairX.integer=*(int*)(b+0x401112c)=i%13;cg_crosshairY.integer=*(int*)(b+0x488ae6c)=i%17;
  for(j=0;j<10;j++){cgs.media.crosshairShader[j]=*(int*)(b+0x2588118+j*4)=100+j;cg.crosshairShaderAlt[j]=*(int*)(b+0x4081538+j*4)=i%3?200+j:0;}
  memcpy(b+0x4087d48,cg.xhairColor,16);memcpy(b+0x4087d58,cg.xhairColorAlt,16);
  count=0;((void(__cdecl*)(void))(b+0x239f0))();n=count;memcpy(expected,calls,sizeof(calls));count=0;CG_DrawCrosshair();
  if(n!=count){printf("crosshair case%d calls%d/%d\n",i,n,count);return 1;}
  for(j=0;j<count;j++){
   if(calls[j].kind!=expected[j].kind||calls[j].shader!=expected[j].shader){printf("crosshair case%d call%d type\n",i,j);return 1;}
   f=&calls[j].x;e=&expected[j].x;for(k=0;k<8;k++)if(fabs(f[k]-e[k])>0.0001+fabs(e[k])*0.000001){printf("crosshair case%d call%d field%d %.9g/%.9g\n",i,j,k,f[k],e[k]);return 1;}
  }
 }
 puts("60000 whole CG_DrawCrosshair callback comparisons passed");
 for(i=0;i<20000;i++){
  weapon=i%64;SETI(weaponSelect,0x407ef70,weapon);
  SETI(time,0x407dea4,1000+i%1000);SETI(zoomTime,0x407f1c8,950+i%713);
  SETI(renderingThirdPerson,0x407deb8,i%7==0);
  weaponDef[weapon].scoped=*(float*)(b+0x4c1e094+weapon*0x1cc)=(float)((i/5)%5);
  snapshot.ps.eFlags=*(int*)(rawSnapshot+0x94)=i%11==0?EF_MOUNTEDTANK:(i%17==0?EF_MG42_ACTIVE:0);
  count=0;((void(__cdecl*)(void))(b+0x27460))();n=count;memcpy(expected,calls,sizeof(calls));count=0;CG_DrawFlashZoomTransition();
  if(count!=n||cg.zoomTime!=*(int*)(b+0x407f1c8)){printf("zoom fade case%d count/state\n",i);return 1;}
  if(count){f=&calls[0].x;e=&expected[0].x;for(k=0;k<8;k++)if(fabs(f[k]-e[k])>0.000001){printf("zoom fade case%d field%d %.9g/%.9g\n",i,k,f[k],e[k]);return 1;}}
 }
 puts("20000 whole CG_DrawFlashZoomTransition original comparisons passed");
 cg.renderingThirdPerson=0;cg.time=1000;cg.zoomTime=1000;snapshot.ps.eFlags=0;
 for(i=0;i<64;i++){
  cg.weaponSelect=i;weaponDef[i].scoped=0;count=0;CG_DrawFlashZoomTransition();
  if(count){printf("iron sight black flash slot%d\n",i);return 1;}
 }
 cg.weaponSelect=25;weaponDef[25].scoped=4;count=0;CG_DrawFlashZoomTransition();
 if(count!=1||calls[0].color[3]!=1){puts("scoped fade missing");return 1;}
 cg.time=1400;count=0;CG_DrawFlashZoomTransition();if(count)return 1;
 puts("64 iron-sight slots and scoped fade boundary regression checks passed");return 0;
}
