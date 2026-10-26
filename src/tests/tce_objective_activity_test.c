#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_TCEObjectiveActivity(gclient_t*);
static byte raw[0x16b0];static gclient_t client;
int main(int argc,char**argv){HMODULE dll;byte*base;void(__cdecl*original)(void*);int gm,carry,axis,w,b,pm,inst,n=0;const int modes[]={2,5,7};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x458d0);
 for(gm=0;gm<3;gm++)for(carry=0;carry<4;carry++)for(axis=0;axis<8;axis++)for(w=0;w<256;w++)for(b=0;b<2;b++)for(pm=0;pm<2;pm++)for(inst=-1;inst<=1;inst++){
 memset(&client,0,sizeof(client));memset(raw,0,sizeof(raw));g_gametype.integer=*(int*)(base+0x6d8d4c)=modes[gm];level.time=*(int*)(base+0x7febe0)=100000+n%100;
 client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(raw+0xf0)=carry&1?0x100:0;client.ps.stats[STAT_TCE_FLAGS]=*(int*)(raw+0xf4)=carry&2?0x100:0;
 client.pers.cmd.forwardmove=*(signed char*)(raw+0x5c4)=axis&1?-127:0;client.pers.cmd.rightmove=*(signed char*)(raw+0x5c5)=axis&2?127:0;client.pers.cmd.upmove=*(signed char*)(raw+0x5c6)=axis&4?-1:0;
 client.pers.cmd.wbuttons=raw[0x5b5]=(byte)w;client.pers.cmd.buttons=raw[0x5b4]=(byte)b;client.ps.pm_type=*(int*)(raw+4)=pm?3:0;client.ps.stats[STAT_TCE_MOVEMENT_INSTABILITY]=*(int*)(raw+0xfc)=inst;client.tceObjectiveActivityUntil=*(int*)(raw+0x1314)=-123;
 original(raw);G_TCEObjectiveActivity(&client);if(client.tceObjectiveActivityUntil!=*(int*)(raw+0x1314)){printf("activity mismatch gm%d carry%d axis%d wb%d attack%d dead%d instability%d %d/%d\n",modes[gm],carry,axis,w,b,pm,inst,*(int*)(raw+0x1314),client.tceObjectiveActivityUntil);return 1;}n++;}
 printf("PASS objective carrier activity %d whole cases\n",n);return 0;
}
