#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static unsigned rng=43;static unsigned next(void){rng=rng*1664525u+1013904223u;return rng>>8;}
static void traceFail(trace_t *tr,const vec3_t a,const vec3_t b,const vec3_t c,const vec3_t d,int cl,int mask){exit(9);}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rp[0x200],ex[0x100];
 playerState_t ps;pmove_t move;pmoveExt_t ext;int w,g,i,k,n=0;
 void (__cdecl *original)(void *,void *,void *,void *,int);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;original=(void *)(base+0xa3b0);gearDef.parsed=1;
 if(offsetof(playerState_t,holdable)+5*4!=0x3a4)return 8;
 for(w=0;w<64;++w){if(w==60)continue;for(g=0;g<8;++g)for(i=0;i<512;++i) {
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&pml,0,sizeof(pml));
  memset(raw,0,sizeof(raw));memset(rp,0,sizeof(rp));memset(ex,0,sizeof(ex));
  ps.weapon=w;ps.stats[STAT_HEALTH]=100;ps.persistant[14]=(g&1)?16:0;
  ps.stats[STAT_TCE_WEAPON_FLAGS]=(g&2)?4:0;weaponDef[w].noTacMode=(g&4)!=0;
  for(k=0;k<2;++k){ps.viewangles[k]=(float)((int)(next()%20000)-10000)*.01f;move.cmd.angles[k]=(int)(next()%65536);ps.holdable[5+k]=1700+next()%601;}
  if(i%4==0){for(k=0;k<2;++k){ps.viewangles[k]=0;move.cmd.angles[k]=(int)(next()%1000)-500;ps.holdable[5+k]=1950+next()%101;}}
  move.ps=&ps;move.pmext=&ext;move.trace=traceFail;pm=&move;
  memcpy(raw,&ps,0x420);*(void **)rp=raw;*(void **)(rp+0x124)=traceFail;
  *(void **)(base+0x4ba9edc)=rp;*(int *)(base+0x4ba9e68)=0;*(int *)(base+0x4ba9ecc)=0;
  memcpy(base+0x4c1df20+w*0x1cc,&weaponDef[w],sizeof(weaponDef[w]));
  original(raw,ex,&move.cmd,traceFail,0);PM_UpdateViewAngles(&ps,&ext,&move.cmd,traceFail,0);
  if(ps.holdable[5]!=*(int *)(raw+0x3a4) || ps.holdable[6]!=*(int *)(raw+0x3a8)) {printf("offset case%d gate%d %d/%d %d/%d\n",n,g,ps.holdable[5],*(int *)(raw+0x3a4),ps.holdable[6],*(int *)(raw+0x3a8));return 1;}
  for(k=0;k<3;++k)if(fabs(ps.viewangles[k]-*(float *)(raw+0xb0+k*4))>.0002f){printf("angle case%d gate%d axis%d %g/%g\n",n,g,k,ps.viewangles[k],*(float *)(raw+0xb0+k*4));return 3;}
  if(memcmp(ps.delta_angles,raw+0x44,12))return 4;++n;
 }}
 printf("%d original tactical view comparisons passed\n",n);return 0;
}
