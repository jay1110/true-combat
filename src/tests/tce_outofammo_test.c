#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern int getEquivWeapon(int);
extern void CG_FinishWeaponChange(int,int),CG_NextWeap(qboolean);
static int calls,from,to,nextCalls;
static void finish(int a,int z){++calls;from=a;to=z;}
static void next(int banks){nextCalls+=banks;}
static void patch(void *a,void *f){DWORD old,tmp;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){HMODULE h;unsigned char *b;static snapshot_t snap;unsigned char raw[0x800]={0};int w,mask,flag,force,gear,i,ec,ef,et,en,es,n=0,bank,cycle;
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x74580,finish);patch(CG_FinishWeaponChange,finish);patch(b+0x74870,next);patch(CG_NextWeap,next);
 cg.snap=&snap;*(void **)(b+0x40352c4)=raw;
 for(w=-32;w<256;++w)if(getEquivWeapon(w)!=((int(__cdecl *)(int))(b+0x74300))(w))return 3;
 for(i=0;i<64;++i)weapAlts[i]=((int *)(b+0x106e78))[i]=0;
 weapAlts[55]=((int *)(b+0x106e78))[55]=23;weapAlts[56]=((int *)(b+0x106e78))[56]=24;
 weapAlts[57]=((int *)(b+0x106e78))[57]=47;weapAlts[58]=((int *)(b+0x106e78))[58]=51;weapAlts[59]=((int *)(b+0x106e78))[59]=46;
 for(w=0;w<=65;++w)for(mask=0;mask<68;++mask)for(flag=0;flag<3;++flag)for(force=0;force<2;++force)for(gear=0;gear<2;++gear){
  /* Missing valid-ID bank entries leave an uninitialized bank local in the
   * original. Do not execute that undefined fallback; production starts at0. */
  if(w>0&&w<64&&!CG_WeaponIndex(w,&bank,&cycle)&&flag!=1&&mask!=0)continue;
  memset(cg.predictedPlayerState.weapons,0,8);memset(b+0x407e294,0,8);
  for(i=1;i<64;++i)if(mask==0||i==mask||mask==66&&(i%3)==0){COM_BitSet(cg.predictedPlayerState.weapons,i);((unsigned *)(b+0x407e294))[i/32]|=1u<<(i%32);}
  gearDef.parsed=gear;cg.weaponSelect=*(int *)(b+0x407ef70)=w;
  cg.predictedPlayerState.weapon=*(int *)(b+0x407df64)=w;
  cg.predictedPlayerState.eFlags=*(int *)(b+0x407df28)=0;
  snap.ps.eFlags=*(int *)(raw+0x94)=flag==1?0x1000000:0;
  cg.predictedPlayerState.ammo[28]=*(int *)(b+0x407e0c0)=flag==2?1:0;
  calls=nextCalls=0;from=to=-1;((void(__cdecl *)(int))(b+0x75480))(force);ec=calls;ef=from;et=to;en=nextCalls;es=*(int *)(b+0x407ef70);
  calls=nextCalls=0;from=to=-1;CG_OutOfAmmoChange(force);
  if(calls!=ec||from!=ef||to!=et||nextCalls!=en||cg.weaponSelect!=es){printf("outofammo mismatch w%d mask%d flag%d force%d gear%d: selection%d/%d finish%d/%d from%d/%d to%d/%d next%d/%d\n",w,mask,flag,force,gear,cg.weaponSelect,es,calls,ec,from,ef,to,et,nextCalls,en);return 1;}++n;
 }
 printf("%d full out-of-ammo comparisons and288 equivalent-ID comparisons passed\n",n);return 0;
}
