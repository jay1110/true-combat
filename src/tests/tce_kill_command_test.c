#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rc[0x2000];static gentity_t ent;static gclient_t client;
static int side,calls,bad,state[10];
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void die(void*p,void*q,void*r,int damage,int mod){void*want=side?(void*)&ent:(void*)raw;calls++;if(p!=want||q!=want||r!=want)bad=1;
 state[0]=damage;state[1]=mod;state[2]=side?ent.health:*(int*)(raw+0x2c8);state[3]=side?ent.flags:*(int*)(raw+0x1b0);state[4]=side?client.ps.stats[STAT_HEALTH]:*(int*)(rc+0xd0);
 state[5]=side?client.ps.holdable[2]:*(int*)(rc+0x398);state[6]=side?client.ps.holdable[3]:*(int*)(rc+0x39c);state[7]=side?client.ps.holdable[4]:*(int*)(rc+0x3a0);state[8]=side?client.ps.persistant[PERS_HWEAPON_USE]:*(int*)(rc+0x140);
 /* A real death callback can overwrite the already-cleared health. */
 if(side)ent.health=-77;else *(int*)(raw+0x2c8)=-77;
}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*,int);int sc,mod,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x4fe20);if(!hook(b+0x57570,die)||!hook(player_die,die))return 2;
 for(sc=0;sc<4096;sc++)for(mod=0;mod<80;mod++){
  int expected[10],expectedCalls;memset(raw,0,sizeof(raw));memset(rc,0,sizeof(rc));memset(&ent,0,sizeof(ent));memset(&client,0,sizeof(client));ent.client=&client;*(void**)(raw+0x190)=rc;
  client.sess.sessionTeam=*(int*)(rc+0xb30)=sc&3;client.ps.pm_flags=*(int*)(rc+0xc)=(sc&4)?PMF_LIMBO:0;ent.health=*(int*)(raw+0x2c8)=((sc>>3)&7)-2;
  level.match_pause=*(int*)(b+0x221bafc)=(sc>>6)&3;g_gamestate.integer=*(int*)(b+0x784f6c)=(sc>>8)&3;
  ent.flags=*(int*)(raw+0x1b0)=sc*7919;client.ps.stats[STAT_HEALTH]=*(int*)(rc+0xd0)=sc;
  client.ps.holdable[2]=*(int*)(rc+0x398)=sc+1;client.ps.holdable[3]=*(int*)(rc+0x39c)=sc+2;client.ps.holdable[4]=*(int*)(rc+0x3a0)=sc+3;client.ps.persistant[PERS_HWEAPON_USE]=*(int*)(rc+0x140)=sc+4;
  side=0;bad=calls=0;memset(state,0,sizeof(state));original(raw,mod);expectedCalls=calls;memcpy(expected,state,sizeof(state));if(bad)return 3;
  side=1;bad=calls=0;memset(state,0,sizeof(state));Cmd_Kill_f(&ent,mod);
  if(bad||calls!=expectedCalls||memcmp(state,expected,sizeof(state))||ent.health!=*(int*)(raw+0x2c8)||ent.flags!=*(int*)(raw+0x1b0)||client.ps.stats[STAT_HEALTH]!=*(int*)(rc+0xd0)||client.ps.holdable[2]!=*(int*)(rc+0x398)||client.ps.holdable[3]!=*(int*)(rc+0x39c)||client.ps.holdable[4]!=*(int*)(rc+0x3a0)||client.ps.persistant[PERS_HWEAPON_USE]!=*(int*)(rc+0x140)){
   fprintf(stderr,"kill command case%d mod%d calls%d/%d failed\n",sc,mod,calls,expectedCalls);return 1;
  }n++;
 }
 printf("%d whole kill-command comparisons passed\n",n);return 0;
}
