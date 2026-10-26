#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_main.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static byte*base,rawClients[64][0x16b0],rawPopup[0x57c];static gclient_t testClients[64];static gentity_t popup;static jmp_buf stop;
static int randomValue,randomCount,spawnCount,popupCount,selected,side,bad;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void noop(void){}static int millis(void){return 123;}static void endpoint(void){longjmp(stop,1);}static int rng(void){randomCount++;return randomValue;}
static void spawn(void*p){int i;spawnCount++;selected=-2;for(i=0;i<64;i++)if(p==(side?(void*)&testClients[i]:(void*)rawClients[i]))selected=i;if(selected<0)bad=1;}
static void*pop(int type){popupCount++;if(type!=4)bad=1;return side?(void*)&popup:(void*)rawPopup;}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*frame)(int);int n,r,v,i,total=0;int values[]={0,1,4680,4681,4682,9362,14043,18724,23405,28086,32766,32767};int expected[8],got[8];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;frame=(void*)(base+0x646b0);
 if(!hook(base+0x8ab50,millis)||!hook(base+0x60270,noop)||!hook(base+0x64d50,noop)||!hook(base+0x47530,noop)||!hook(base+0x64c78,endpoint)||!hook(base+0xa20e4,rng)||!hook(rand,rng)||!hook(base+0x4b9c0,spawn)||!hook(SetWolfSpawnWeapons,spawn)||!hook(base+0x95050,pop)||!hook(G_PopupMessage,pop))return 2;
 for(n=1;n<=64;n++)for(r=0;r<12;r++)for(v=0;v<8;v++)for(side=0;side<2;side++){
 randomValue=values[r];randomCount=spawnCount=popupCount=bad=0;selected=-1;memset(rawClients,0,sizeof(rawClients));memset(testClients,0,sizeof(testClients));memset(rawPopup,0,sizeof(rawPopup));memset(&popup,0,sizeof(popup));
 level.clients=testClients;level.maxclients=*(int*)(base+0x7febd8)=n;level.numConnectedClients=*(int*)(base+0x7fec0c)=n;level.time=*(int*)(base+0x7febe0)=11000;level.startTime=*(int*)(base+0x7febec)=10000;level.warmupTime=*(int*)(base+0x7feb90)=0;
 g_gametype.integer=*(int*)(base+0x6d8d4c)=5;g_gamestate.integer=*(int*)(base+0x784f6c)=0;level.tceVipTeam=*(int*)(base+0x2223940)=1;level.tceVipAssigned=*(int*)(base+0x2223944)=0;level.tceVipCarrier=*(int*)(base+0x2223948)=-1;
 *(int*)(base+0x222393c)=0;*(int*)(base+0x7fec08)=0;*(int*)(base+0x221bafc)=0;*(int*)(base+0x7feb8c)=0;*(int*)(base+0x784aec)=0;
 for(i=0;i<n;i++){byte*e=base+0x226e2a0+i*0x57c;g_entities[i].client=&testClients[i];*(void**)(e+0x190)=rawClients[i];g_entities[i].s.eFlags=*(int*)(e+8)=0;testClients[i].pers.connected=*(int*)(rawClients[i]+0x5ac)=2;testClients[i].sess.sessionTeam=*(int*)(rawClients[i]+0xb30)=1;testClients[i].ps.clientNum=*(int*)(rawClients[i]+0xa0)=i;testClients[i].ps.stats[STAT_HEALTH]=*(int*)(rawClients[i]+0xd0)=100;*(int*)(base+0x7fec18+i*4)=i;if(v&&i%8==v){testClients[i].ps.pm_flags=*(int*)(rawClients[i]+0xc)=PMF_LIMBO;}}
 if(side)G_TCEAssignObjective(qtrue);else if(!setjmp(stop))frame(11000);
 got[0]=randomCount;got[1]=spawnCount;got[2]=popupCount;got[3]=selected;got[4]=side?level.tceVipCarrier:*(int*)(base+0x2223948);got[5]=side?popup.s.effect2Time:*(int*)(rawPopup+0x110);got[6]=side?popup.s.effect3Time:*(int*)(rawPopup+0x114);got[7]=side?popup.s.density:*(int*)(rawPopup+0xf4);
 if(!side)memcpy(expected,got,sizeof(got));else{if(bad||memcmp(expected,got,sizeof(got))){printf("VIP assignment mismatch count%d rng%d v%d selected%d/%d\n",n,randomValue,v,expected[3],got[3]);return 1;}total++;}}
 printf("PASS VIP original assignment branch %d cases (not whole G_RunFrame)\n",total);return 0;
}

