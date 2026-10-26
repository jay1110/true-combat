#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte*base,rawClient[0x16b0];static gclient_t client;static int side,count;
typedef struct{int client;char category[32],values[64];}event;static event calls[2];
typedef struct{int stats[64][3],marker,times[10],count;event calls[2];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void pb(int n,char*c,char*s){event*e;if(count==2)exit(3);e=&calls[count++];e->client=n;Q_strncpyz(e->category,c,sizeof(e->category));Q_strncpyz(e->values,s,sizeof(e->values));}
static void capture(result*r){int i;memset(r,0,sizeof(*r));for(i=0;i<64;i++){r->stats[i][0]=side?client.pers.playerStats.weaponStats[i].kills:*(int*)(rawClient+0x6a8+i*12);r->stats[i][1]=side?client.pers.playerStats.weaponStats[i].teamkills:*(int*)(rawClient+0x6ac+i*12);r->stats[i][2]=side?client.pers.playerStats.weaponStats[i].killedby:*(int*)(rawClient+0x6b0+i*12);}r->marker=side?client.soldierKillMarker:*(int*)(rawClient+0xff8);memcpy(r->times,side?(void*)client.soliderKillTimes:(void*)(rawClient+0xffc),40);r->count=count;memcpy(r->calls,calls,sizeof(calls));}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*death)(void*,int),(__cdecl*kill)(void*,int);int phase,v,i,j,slot,total=0;static result a,b;int times[]={0,39999,40000,40001,99999,100000,100001};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;death=(void*)(base+0x86730);kill=(void*)(base+0x867b0);if(!hook(base+0x8bad0,pb)||!hook(trap_PbStat,pb))return 2;
 for(phase=0;phase<2;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){
  byte*p;gentity_t*e;slot=(v/64)%64;p=base+0x226e2a0+slot*0x57c;e=&g_entities[slot];memset(p,0,0x57c);memset(e,0,sizeof(*e));memset(rawClient,0,sizeof(rawClient));memset(&client,0,sizeof(client));memset(calls,0,sizeof(calls));count=0;e->client=(v/640)%5?&client:NULL;*(void**)(p+0x190)=(v/640)%5?rawClient:NULL;e->s.number=*(int*)p=63-slot;client.ps.clientNum=*(int*)(rawClient+0xa0)=11;client.sess.sessionTeam=*(int*)(rawClient+0xb30)=(v/3200)%4;client.sess.playerType=*(int*)(rawClient+0xb40)=(v/12800)%2?0:(v/6400)%7;client.soldierKillMarker=*(int*)(rawClient+0xff8)=(v/64)%10;level.timeCurrent=*(int*)(base+0x221bc08)=100000;
  for(i=0;i<10;i++)client.soliderKillTimes[i]=*(int*)(rawClient+0xffc+i*4)=times[(v/128+i)%7];if(v&128)for(i=0;i<10;i++)client.soliderKillTimes[i]=*(int*)(rawClient+0xffc+i*4)=100000;
  for(i=0;i<64;i++){client.pers.playerStats.weaponStats[i].kills=*(int*)(rawClient+0x6a8+i*12)=i*3-100;client.pers.playerStats.weaponStats[i].teamkills=*(int*)(rawClient+0x6ac+i*12)=i*7;client.pers.playerStats.weaponStats[i].killedby=*(int*)(rawClient+0x6b0+i*12)=i*5-1;}
  if(!phase){if(side)G_LogDeath(e,v%64);else death(p,v%64);}else{if(side)G_LogKill(e,v%64);else kill(p,v%64);}capture(side?&b:&a);if(side){if(memcmp(&a,&b,sizeof(a))){printf("Kill/death statistics mismatch phase%d case%d calls%d/%d marker%d/%d\n",phase,v,a.count,b.count,a.marker,b.marker);for(j=0;j<sizeof(a);j++)if(((byte*)&a)[j]!=((byte*)&b)[j]){printf("byte%x %02x/%02x\n",j,((byte*)&a)[j],((byte*)&b)[j]);break;}return 1;}total++;}
 }
 printf("PASS whole G_LogDeath/G_LogKill %d cases (32768 each), real TC duplicate weapon mapping\n",total);return 0;
}
