#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawClient[0x2000],*base;static gentity_t ent;static gclient_t client;
static int side,countCalls,printCalls,bad,population,requestedTeam;static char printed[1024];
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static int countTeam(int ignore,int team){countCalls++;if(ignore!=-1||team!=requestedTeam)bad=1;return population;}
static void printFull(char*s,void*p){printCalls++;if(countCalls!=1||p!=(side?(void*)&ent:(void*)raw))bad=1;Q_strncpyz(printed,s,sizeof(printed));}
static void setup(int sc){int i;for(i=0;i<160;i++)((byte*)teamInfo)[i]=base[0x66fde0+i]=(byte)(sc+i*17);
 match_latejoin.integer=*(int*)(base+0x6d976c)=(sc>>1)&1;g_gamestate.integer=*(int*)(base+0x784f6c)=(sc>>2)&3;match_timeoutcount.integer=*(int*)(base+0x76c16c)=sc%7-2;
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*reset)(int,int);int(__cdecl*join)(int,void*);int team,clear,sc,gt,n=0;
 if(argc!=2||sizeof(team_info)!=40||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;reset=(void*)(base+0x8f590);join=(void*)(base+0x8fb40);ent.client=&client;*(void**)(raw+0x190)=rawClient;
 if(!hook(base+0x4b8a0,countTeam)||!hook(TeamCount,countTeam)||!hook(base+0x65120,printFull)||!hook(G_printFull,printFull))return 2;
 for(team=0;team<4;team++)for(clear=0;clear<3;clear++)for(sc=0;sc<1024;sc++){setup(sc);reset(team,clear);G_teamReset(team,clear);if(memcmp(teamInfo,base+0x66fde0,160)){fprintf(stderr,"team reset team%d clear%d case%d failed\n",team,clear,sc);return 1;}}
 for(team=0;team<4;team++)for(gt=0;gt<10;gt++)for(sc=0;sc<4096;sc++){
  char expected[1024];int x,y,expectedPrint;setup(sc);requestedTeam=team;population=(sc&7)-1;
  g_gametype.integer=*(int*)(base+0x6d8d4c)=gt;team_maxplayers.integer=*(int*)(base+0x23cec8c)=((sc>>3)&7)-1;g_lms_lockTeams.integer=*(int*)(base+0x785bcc)=(sc>>6)&1;
  client.sess.sessionTeam=*(int*)(rawClient+0xb30)=(sc>>7)&3;client.pers.invite=*(int*)(rawClient+0xa24)=(sc>>9)&7;
  teamInfo[team].team_lock=*(int*)(base+0x66fde4+team*40)=(sc>>8)&1;
  side=0;countCalls=printCalls=bad=0;printed[0]=0;x=join(team,raw);expectedPrint=printCalls;Q_strncpyz(expected,printed,sizeof(expected));if(bad||countCalls!=1)return 3;
  side=1;countCalls=printCalls=bad=0;printed[0]=0;y=G_teamJoinCheck(team,&ent);
  if(bad||countCalls!=1||x!=y||printCalls!=expectedPrint||strcmp(expected,printed)||memcmp(teamInfo,base+0x66fde0,160)){fprintf(stderr,"join team%d gt%d case%d result%d/%d print %s / %s failed\n",team,gt,sc,y,x,printed,expected);return 1;}n++;
 }
 printf("12288 whole team-reset and %d whole team-join comparisons passed\n",n);return 0;
}
