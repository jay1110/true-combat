#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static byte*base,rawClient[0x16b0];static gclient_t client;static jmp_buf stop;static int reached;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void noop(void){}static void*none(void){return NULL;}static void endpoint(void){longjmp(stop,1);}static void*lookup(const char*s){reached=strcmp(s,"ammo_VIP")?-1:1;longjmp(stop,1);return NULL;}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(int);int entNum,psNum,owner,flag,assigned,s,n=0,want;byte*r;gentity_t*e;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x4d840);
 if(!hook(base+0x5c230,noop)||!hook(base+0x4a5d0,noop)||!hook(base+0x535f0,noop)||!hook(base+0x6c370,noop)||!hook(base+0x90250,none)||!hook(base+0x90290,none)||!hook(base+0x4e03d,endpoint)||!hook(base+0x2b280,lookup)||!hook(BG_FindItemForClassName,lookup))return 2;
 for(entNum=0;entNum<4;entNum++)for(psNum=0;psNum<4;psNum++)for(owner=-1;owner<4;owner++)for(flag=0;flag<2;flag++)for(assigned=0;assigned<2;assigned++)for(s=0;s<2;s++){
 reached=0;e=&g_entities[entNum];r=base+0x226e2a0+entNum*0x57c;memset(e,0,sizeof(*e));memset(r,0,0x57c);memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));e->client=&client;*(void**)(r+0x190)=rawClient;e->s.number=*(int*)r=entNum;
 *(int*)(base+0x7fec0c)=0;*(int*)(base+0x6d856c)=0;g_gametype.integer=*(int*)(base+0x6d8d4c)=5;client.pers.connected=*(int*)(rawClient+0x5ac)=2;client.sess.sessionTeam=*(int*)(rawClient+0xb30)=1;client.ps.clientNum=*(int*)(rawClient+0xa0)=psNum;client.ps.stats[STAT_TCE_FLAGS]=*(int*)(rawClient+0xf4)=flag?0x100:0;level.tceVipCarrier=*(int*)(base+0x2223948)=owner;level.tceVipAssigned=*(int*)(base+0x2223944)=assigned;
 if(!setjmp(stop)){if(s)G_TCEReleaseObjectives(e,qtrue,qtrue);else original(entNum);}if(!s)want=reached;else{if(want!=reached){printf("VIP disconnect gate mismatch entity%d ps%d owner%d flags%d assigned%d %d/%d\n",entNum,psNum,owner,flag,assigned,want,reached);return 1;}n++;}}
 printf("PASS VIP disconnect ownership gate %d cases (not whole ClientDisconnect)\n",n);return 0;
}
