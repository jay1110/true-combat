#define main unusedProneTestMain
#include "tce_prone_runtime_test.c"
#undef main
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rp[0x200],ex[0x100];
 playerState_t ps;pmove_t move;pmoveExt_t ext;int w,sc,a,p,t,m,k,j,ec,n=0;
 int weapons[]={45,49,62};float et[3][12];int eta[3][2];
 void (__cdecl *original)(void *,void *,void *,void *,int);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;original=(void *)(base+0xa3b0);gearDef.parsed=1;
 for(w=0;w<3;++w)for(sc=0;sc<2;++sc)for(a=0;a<16;++a)for(p=0;p<7;++p)for(t=0;t<4;++t)for(m=0;m<5;++m) {
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&pml,0,sizeof(pml));
  memset(raw,0,sizeof(raw));memset(rp,0,sizeof(rp));memset(ex,0,sizeof(ex));
  ps.weapon=weapons[w];ps.eFlags=EF_PRONE;ps.stats[STAT_HEALTH]=100;ps.stats[STAT_TCE_FLAGS]=sc?0x200:0;
  ps.clientNum=7;VectorSet(ps.origin,100,200,300);VectorSet(ps.viewangles,10,a*23-170,0);
  ps.delta_angles[YAW]=123;ext.proneLegsOffset=17;ext.mountedWeaponAngles[0]=10;ext.mountedWeaponAngles[1]=20;
  move.ps=&ps;move.pmext=&ext;move.trace=traceProne;move.tracemask=0x1234;
  move.cmd.angles[YAW]=ANGLE2SHORT(175-a*29);move.cmd.angles[PITCH]=ANGLE2SHORT(p*30-90);
  pml.frametime=(t+1)*.016f;pm=&move;mode=m;
  memcpy(raw,&ps,0x420);*(float *)(ex+0x3c)=ext.proneLegsOffset;memcpy(ex+0x40,ext.mountedWeaponAngles,12);
  *(void **)rp=raw;*(void **)(rp+0x124)=traceProne;*(int *)(rp+0x44)=move.tracemask;
  *(void **)(base+0x4ba9edc)=rp;*(float *)(base+0x4ba9e64)=pml.frametime;
  calls=0;original(raw,ex,&move.cmd,traceProne,move.tracemask);ec=calls;memcpy(et,traces,sizeof(et));memcpy(eta,traceArgs,sizeof(eta));
  calls=0;PM_UpdateViewAngles(&ps,&ext,&move.cmd,traceProne,move.tracemask);
  for(k=0;k<3;++k)if(fabs(ps.viewangles[k]-*(float *)(raw+0xb0+k*4))>.001f){printf("angle case%d axis%d %g/%g\n",n,k,ps.viewangles[k],*(float *)(raw+0xb0+k*4));return 1;}
  if(memcmp(ps.delta_angles,raw+0x44,12) || fabs(ext.proneLegsOffset-*(float *)(ex+0x3c))>.001f || calls!=ec){printf("state case%d weapon%d delta%d/%d traces%d/%d offset%g/%g\n",n,ps.weapon,ps.delta_angles[1],*(int *)(raw+0x48),calls,ec,ext.proneLegsOffset,*(float *)(ex+0x3c));return 3;}
  for(k=0;k<calls;++k){if(memcmp(eta[k],traceArgs[k],sizeof(eta[k])))return 4;for(j=0;j<12;++j)if(fabs(et[k][j]-traces[k][j])>.001f){printf("trace case%d %d:%d %g/%g\n",n,k,j,traces[k][j],et[k][j]);return 5;}}
  ++n;
 }
 printf("%d original prone view-angle comparisons passed\n",n);return 0;
}
