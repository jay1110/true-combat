#include "../cgame/tce_weapon_cycle.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
static unsigned char *base;
static int owned[64],calls,nfinish,from,to;
typedef struct {int kind,weapon;float x,y,w,h,color[4];} draw_t;
static draw_t draws[16];
static int selectable(int w){return w>0 && w<64 && owned[w];}
static int width(int w){return 17+w;}
static int originalWidth(const char *s,float scale,int limit,void *font){
 int w=(int)((const unsigned char *)s-(base+0x484af6c))/0xfd0;
 if(scale!=.2f || limit || font!=base+0x2588f8c)abort();return width(w);
}
static void border(float x,float y,float w,float h,const float *c){
 draw_t *d=&draws[calls++];memset(d,0,sizeof(*d));d->kind=1;
 d->x=x;d->y=y;d->w=w;d->h=h;memcpy(d->color,c,16);
}
static void originalBorder(float x,float y,float w,float h,int size,const float *c){
 if(size!=1)abort();border(x,y,w,h,c);
}
static void text(float x,float y,const float *c,int weapon){
 draw_t *d=&draws[calls++];memset(d,0,sizeof(*d));d->kind=2;d->weapon=weapon;
 d->x=x;d->y=y;memcpy(d->color,c,16);
}
static void originalText(float x,float y,float sx,float sy,const float *c,const char *s,
 float adjust,int limit,int style,void *font){
 int w=(int)((const unsigned char *)s-(base+0x484af6c))/0xfd0;
 if(sx!=.2f || sy!=.2f || adjust || limit || style!=3 || font!=base+0x2588f8c)abort();
 text(x,y,c,w);
}
static void finish(int a,int b){++nfinish;from=a;to=b;}
static void patch(void *address,void *fn){
 unsigned char *p=address;DWORD old,tmp;
 VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;
 *(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
int testCycleHud(HMODULE dll){
 static unsigned char snap[0x800];
 tce_weaponCycleServices_t api={selectable,width,border,text,finish};
 tce_weaponCycle_t s;
 void (__cdecl *original)(float *,int,int,float *);
 int mask,scenario,offset,t,attack,cases=0,bank,expectedCalls,expectedFinish,expectedFrom,expectedTo,k,j;
 int elapsed[]={1999,2000,2001,4000};
 int primary[]={8,2,1,9,4,30};
 draw_t expected[16];
 float rect[]={563.25f,457.5f,60,32},color[]={.25f,.75f,1,.1f};
 base=(unsigned char *)dll;original=(void *)(base+0x4e7b0);
 patch(base+0x74230,selectable);patch(base+0x20910,originalWidth);
 patch(base+0x29df0,originalBorder);patch(base+0x20ba0,originalText);
 patch(base+0x74580,finish);*(void **)(base+0x40352c4)=snap;
 for(mask=0;mask<128;++mask)for(scenario=0;scenario<9;++scenario)
 for(offset=-13;offset<=13;++offset)for(t=0;t<4;++t)for(attack=0;attack<2;++attack){
  memset(owned,0,sizeof(owned));for(bank=0;bank<6;++bank)owned[primary[bank]]=(mask>>bank)&1;
  owned[45]=(mask>>6)&1; /* Multiple entries in one bank, last one wins. */
  memset(&s,0,sizeof(s));s.hasSnapshot=1;
  s.time=*(int *)(base+0x407dea4)=10000;
  s.weaponSelectTime=*(int *)(base+0x4080e44)=scenario==1?8401:8400;
  s.cycleTime=*(int *)(base+0x40a4ba0)=10000-elapsed[t];
  s.cycleOffset=*(int *)(base+0x40a4ba4)=offset;
  s.cycleSelected=*(int *)(base+0x40a4ba8)=7;
  s.weaponSelect=*(int *)(base+0x407ef70)=scenario==2?8:56;
  s.snapshotPmFlags=*(int *)(snap+0x38)=scenario==3?0x1000:0;
  s.snapshotWeaponState=*(int *)(snap+0xd4)=scenario==4?1:0;
  s.predictedWeaponState=*(int *)(base+0x407df68)=scenario==5?1:0;
  s.predictedWeapon=*(int *)(base+0x407df64)=primary[(mask+scenario)%6];
  s.snapshotWeapon=*(int *)(snap+0xd0)=scenario==6?60:s.predictedWeapon;
  s.buttons=*(int *)(base+0x1dc5fd0)=attack;
  s.cycleDelay=*(int *)(base+0x400f62c)=scenario==7?1600:scenario==8?1601:1500;
  *(float *)(base+0x4087d54)=.625f;
  calls=nfinish=from=to=0;original(rect,1,2,color);
  expectedCalls=calls;expectedFinish=nfinish;expectedFrom=from;expectedTo=to;
  memcpy(expected,draws,sizeof(expected));calls=nfinish=from=to=0;
  TCE_CG_DrawWeaponCycle(&s,(const int (*)[22])(base+0x964b8),rect,color,.625f,&api);
  if(calls!=expectedCalls || nfinish!=expectedFinish || from!=expectedFrom || to!=expectedTo ||
   s.weaponSelect!=*(int *)(base+0x407ef70) || s.weaponSelectTime!=*(int *)(base+0x4080e44) ||
   s.cycleTime!=*(int *)(base+0x40a4ba0) || s.cycleSelected!=*(int *)(base+0x40a4ba8)){
   printf("HUD mismatch mask%d scenario%d offset%d timeout%d attack%d calls%d/%d selected%d/%d\n",
    mask,scenario,offset,elapsed[t],attack,calls,expectedCalls,s.cycleSelected,*(int *)(base+0x40a4ba8));return 1;
  }
  for(k=0;k<calls;++k){
   if(draws[k].kind!=expected[k].kind || draws[k].weapon!=expected[k].weapon)return 1;
   for(j=0;j<8;++j)if(fabs(((float *)&draws[k].x)[j]-((float *)&expected[k].x)[j])>.0001){
    printf("draw mismatch mask%d row%d field%d got%f expected%f\n",mask,k,j,
      ((float *)&draws[k].x)[j],((float *)&expected[k].x)[j]);return 1;
   }
  }
  ++cases;
 }
 printf("%d original HUD state/draw/confirmation comparisons matched\n",cases);
 original=(void *)(base+0x4e6c0);cases=0;
 for(mask=0;mask<64;++mask)for(t=-1;t<1603;++t) {
  s.time=*(int *)(base+0x407dea4)=10000;
  s.weaponSelectTime=*(int *)(base+0x4080e44)=10000-t;
  s.weaponSelect=*(int *)(base+0x407ef70)=mask;
  calls=0;original(rect,1,2,color);expectedCalls=calls;memcpy(expected,draws,sizeof(expected));
  calls=0;TCE_CG_DrawSelectedWeapon(&s,rect,color,&api);
  if(calls!=expectedCalls || memcmp(draws,expected,calls*sizeof(draw_t))) {
   printf("label mismatch ID%d elapsed%d\n",mask,t);return 1;
  }
  ++cases;
 }
 printf("%d original selected-weapon labels matched\n",cases);return 0;
}
