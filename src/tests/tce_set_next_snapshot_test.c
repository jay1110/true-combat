#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_snapshot.c"
#include <windows.h>
#include <stdio.h>
static byte *base,*input;static int originalRun,order,error,expectedClient;
static void convert(playerState_t *ps,entityState_t *es,qboolean snap){void*wantPs=originalRun?(void*)(input+44):&cg.activeSnapshots[1].ps;void*wantEs=originalRun?(void*)(base+0x488d920+expectedClient*0xa94):&cg_entities[expectedClient].nextState;if(order++!=0||ps!=wantPs||es!=wantEs||snap)error=1;memset(es,0x6a,0x120);}
static void solid(void){if(order++!=1)error=1;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*original)(void*);static byte old[0x24600],next[0x24600];int k,i,j,n,oldClient,teleport,expectedInterp[8];static byte expectedState[8][0x120];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=base=(byte*)m;original=(void*)(b+0x60490);hook(b+0x52b0,convert);hook(BG_PlayerStateToEntityState,convert);hook(b+0x58310,solid);hook(CG_BuildSolidList,solid);
 if(sizeof(entityState_t)!=0x120){puts("entity ABI requires explicit normalization");return 2;}
 for(k=0;k<8192;k++){
  memset(old,0,sizeof(old));memset(next,0,sizeof(next));memset(cg.activeSnapshots,0,sizeof(cg.activeSnapshots));input=next;cg.snap=&cg.activeSnapshots[0];oldClient=k%4;expectedClient=(k/4)%4;n=(k/16)%9;
  *(int*)(old+0xcc)=cg.snap->ps.clientNum=oldClient;*(int*)(next+0xcc)=cg.activeSnapshots[1].ps.clientNum=expectedClient;
  *(int*)(old+0x94)=cg.snap->ps.eFlags=(k/144)%8;*(int*)(next+0x94)=cg.activeSnapshots[1].ps.eFlags=(k/1152)%8;
  *(int*)old=cg.snap->snapFlags=(k/32)%8;*(int*)next=cg.activeSnapshots[1].snapFlags=(k/256)%8;*(int*)(next+0x5d8)=cg.activeSnapshots[1].numEntities=n;
  *(void**)(b+0x40352c4)=old;*(void**)(b+0x40352c8)=NULL;cg.nextSnap=NULL;
  for(i=0;i<8;i++){
   byte*raw=b+0x488d800+i*0xa94;memset(raw,0,0xa94);memset(&cg_entities[i],0,sizeof(centity_t));
   *(int*)(raw+8)=cg_entities[i].currentState.eFlags=(k+i)%8;*(int*)(raw+0x244)=cg_entities[i].currentValid=(k>>(i%7))&1;
   *(int*)(raw+0x240)=cg_entities[i].interpolate=37;memset(raw+0x120,0x35,0x120);memset(&cg_entities[i].nextState,0x35,0x120);
   if(i<n){byte*es=next+0x5dc+i*0x120;for(j=0;j<0x120;j++)es[j]=(byte)(k+j+i);*(int*)es=i;*(int*)(es+8)=(k/8+i)%8;memcpy(&cg.activeSnapshots[1].entities[i],es,0x120);}
  }
  originalRun=1;order=error=0;original(next);if(error||order!=2)return 3;teleport=*(int*)(b+0x407de9c);for(i=0;i<8;i++){expectedInterp[i]=*(int*)(b+0x488da40+i*0xa94);memcpy(expectedState[i],b+0x488d920+i*0xa94,0x120);}
  originalRun=0;order=error=0;CG_SetNextSnap(&cg.activeSnapshots[1]);if(error||order!=2||cg.nextSnap!=&cg.activeSnapshots[1]||*(void**)(b+0x40352c8)!=next||cg.nextFrameTeleport!=teleport){printf("next snapshot control case%d\n",k);return 1;}
  for(i=0;i<8;i++)if(cg_entities[i].interpolate!=expectedInterp[i]||memcmp(&cg_entities[i].nextState,expectedState[i],0x120)){printf("next snapshot entity case%d entity%d\n",k,i);return 1;}
 }puts("8192 whole CG_SetNextSnap comparisons passed");return 0;
}
