#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int calls,changes,changeArgs[3],mode;
static float traces[3][12];static int traceArgs[3][2];
static void traceProne(trace_t *tr,const vec3_t a,const vec3_t b,const vec3_t c,const vec3_t d,int cl,int mask) {
 int i,index=calls++;if(index>=3)exit(9);
 memcpy(traces[index],a,12);memcpy(traces[index]+3,b,12);memcpy(traces[index]+6,c,12);memcpy(traces[index]+9,d,12);
 traceArgs[index][0]=cl;traceArgs[index][1]=mask;memset(tr,0,sizeof(*tr));
 tr->fraction=(mode==1)?1:.5f;tr->startsolid=mode==2;tr->allsolid=mode==3;
 tr->entityNum=(mode==4)?3:ENTITYNUM_WORLD;
 for(i=0;i<3;++i)tr->endpos[i]=(a[i]+d[i])*.5f;
}
static void change(int old,int next,qboolean reload){changes++;changeArgs[0]=old;changeArgs[1]=next;changeArgs[2]=reload;}
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static unsigned rng=19;static unsigned next(void){rng=rng*1664525u+1013904223u;return rng>>8;}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rp[0x200],extRaw[0x100];
 playerState_t ps;pmove_t move;pmoveExt_t ext;int n,k,j,ret,er,ec,ech,echange[3],eta[3][2];float et[3][12];
 int dt[]={-1,0,1,749,750,751,1000};int (*original)(void);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;original=(void *)(base+0xc9a0);
 patch(base+0x8e90,change);patch(PM_BeginWeaponChange,change);gearDef.parsed=1;
 for(n=0;n<131072;++n) {
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&pml,0,sizeof(pml));
  memset(raw,0,sizeof(raw));memset(rp,0,sizeof(rp));memset(extRaw,0,sizeof(extRaw));
  ps.weapon=n%67;ps.eFlags=(next()%2)?EF_PRONE:0;if(next()%3==0)ps.eFlags|=EF_PRONE_MOVING;
  ps.pm_flags=(next()%2)?PMF_DUCKED:0;ps.groundEntityNum=ENTITYNUM_WORLD;ps.pm_type=PM_NORMAL;
  ps.stats[STAT_TCE_FLAGS]=(next()%2)?0x200:0;ps.stats[STAT_TCE_WEAPON_FLAGS]=(next()%2)?0x200:0;
  ps.persistant[14]=(next()%2)?4:0;ps.weaponDelay=next()%2;
  switch(next()%16){case 0:ps.pm_flags|=PMF_LADDER;break;case 1:ps.eFlags|=EF_MOUNTEDTANK;break;case 2:ps.groundEntityNum=ENTITYNUM_NONE;break;case 3:ps.groundEntityNum=3;break;case 4:pml.tceContentRestriction=1;break;case 5:move.waterlevel=2;break;case 6:ps.stats[STAT_TCE_WEAPON_FLAGS]|=0x1000;break;case 7:ps.pm_type=PM_DEAD;break;case 8:ps.eFlags|=0x1000000;break;case 9:ps.persistant[PERS_HWEAPON_USE]=1;break;}
  VectorSet(ps.mins,-18,-18,-24);VectorSet(ps.maxs,18,18,48);ps.crouchMaxZ=24;ps.standViewHeight=40;ps.clientNum=7;
  VectorSet(ps.origin,100,200,300);ps.velocity[0]=(float)(next()%42);pml.forward[0]=.8f;pml.forward[1]=.6f;
  move.ps=&ps;move.pmext=&ext;move.trace=traceProne;move.tracemask=0x1234;move.cmd.serverTime=10000;
  move.cmd.wbuttons=(next()%2)?WBUTTON_PRONE:0;move.cmd.doubleTap=next()%5;move.cmd.upmove=(next()%3)*20-20;
  move.cmd.forwardmove=(next()%3)*10;ext.proneTime=(ps.eFlags&EF_PRONE)?10000-dt[next()%7]:dt[next()%7]-10000;
  ext.proneGroundTime=123;pm=&move;mode=next()%5;
  memcpy(raw,&ps,0x420);*(int *)(raw+0x47c)=ps.jumpTime;
  *(void **)rp=raw;*(void **)(rp+4)=extRaw;memcpy(rp+0xc,&move.cmd,sizeof(move.cmd));
  *(int *)(rp+0x44)=move.tracemask;*(int *)(rp+0x110)=move.waterlevel;*(void **)(rp+0x124)=traceProne;
  *(int *)(extRaw+0x34)=ext.proneTime;*(int *)(extRaw+0x38)=ext.proneGroundTime;
  *(void **)(base+0x4ba9edc)=rp;memcpy(base+0x4ba9e40,pml.forward,12);*(int *)(base+0x4ba9ed0)=pml.tceContentRestriction;
  calls=changes=0;er=original();ec=calls;ech=changes;memcpy(et,traces,sizeof(et));memcpy(eta,traceArgs,sizeof(eta));memcpy(echange,changeArgs,sizeof(echange));
  calls=changes=0;ret=PM_CheckProne();
  if(ret!=er || memcmp(raw,&ps,0x420) || *(int *)(raw+0x47c)!=ps.jumpTime ||
     *(int *)(extRaw+0x34)!=ext.proneTime || *(int *)(extRaw+0x38)!=ext.proneGroundTime || *(int *)(extRaw+4)!=ext.jumpTime ||
     memcmp(rp+0xf4,move.mins,12) || memcmp(rp+0x100,move.maxs,12) || calls!=ec || changes!=ech ||
     (changes && memcmp(echange,changeArgs,sizeof(echange)))) {
   printf("prone mismatch case%d weapon%d return%d/%d calls%d/%d flags%x/%x time%d/%d\n",n,n%67,ret,er,calls,ec,ps.eFlags,*(int *)(raw+0x68),ext.proneTime,*(int *)(extRaw+0x34));return 1;
  }
  for(k=0;k<calls;++k){if(memcmp(eta[k],traceArgs[k],sizeof(eta[k])))return 4;for(j=0;j<12;++j)if(fabs(et[k][j]-traces[k][j])>.0001f){printf("trace mismatch case%d %d:%d %g/%g\n",n,k,j,traces[k][j],et[k][j]);return 3;}}
 }
 puts("131072 production prone comparisons passed (weapon-change dependency intercepted)");return 0;
}
